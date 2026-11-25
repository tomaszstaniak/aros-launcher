/* SPDX-License-Identifier: MIT */
/* Runs in its own process. Nothing here uses the C library: its state
 * belongs to the task that opened it. Memory comes from exec. */
#include "scan_aros.h"

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <dos/notify.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>

#define PATH_MAX_LEN 512

struct ln_scan {
    struct MsgPort *to;         /* the main task's port */
    struct Process *proc;
    int n;
    char **folder;              /* AllocVec'd copies */
    struct NotifyRequest *notify;
    UBYTE *dirty;
    int files_n;
    char **files;               /* folders walked for the file index */
    volatile LONG want_files;   /* walk them (again) */
    int exclude_n;
    char **exclude;             /* paths and patterns kept out */
    char **pattern;             /* the patterns, parsed; NULL for a path */
    char command[1200];         /* set by the main task before it signals CTRL_D */
    volatile LONG busy;         /* a command is waiting or running */
    struct ln_scan_msg *last;   /* the exit message, made before the process starts */
};

static ULONG len(const char *s) { ULONG n = 0; while (s[n]) n++; return n; }

/* 0 when there was no memory for the message. */
static int send(struct ln_scan *s, LONG kind, const char *path, const char *title)
{
    ULONG pn = path ? len(path) + 1 : 0, tn = title ? len(title) + 1 : 0;
    struct ln_scan_msg *m = AllocVec(sizeof *m + pn + tn, MEMF_PUBLIC | MEMF_CLEAR);
    if (!m) return 0;
    m->m.mn_Length = (UWORD)sizeof *m;
    m->kind = kind;
    if (path) { m->path = (char *)(m + 1); CopyMem((APTR)path, m->path, pn); }
    if (title) { m->title = (char *)(m + 1) + pn; CopyMem((APTR)title, m->title, tn); }
    PutMsg(s->to, &m->m);
    return 1;
}

static int stopping(void) { return (SetSignal(0, 0) & SIGBREAKF_CTRL_C) != 0; }

/* dir + name into out; 0 when it does not fit. */
static int join(char *out, const char *dir, const char *name)
{
    ULONG dn = len(dir), nn = len(name), sep = dn && dir[dn - 1] != ':' && dir[dn - 1] != '/';
    if (dn + sep + nn + 1 > PATH_MAX_LEN) return 0;
    CopyMem((APTR)dir, out, dn);
    if (sep) out[dn] = '/';
    CopyMem((APTR)name, out + dn + sep, nn + 1);
    return 1;
}

static int is_info(const char *name)
{
    ULONG n = len(name);
    if (n < 5) return 0;
    name += n - 5;
    return name[0] == '.' && (name[1] | 32) == 'i' && (name[2] | 32) == 'n' && (name[3] | 32) == 'f' && (name[4] | 32) == 'o';
}

/* A script with an icon of its own: a project icon whose default tool is
 * IconX. Many programs are started this way, the executable itself having
 * no icon. Other project icons are documents and are left out. */
static int is_script(const struct DiskObject *icon)
{
    const char *tool = (const char *)icon->do_DefaultTool, *part;
    static const char want[] = "iconx";
    int i;
    if (icon->do_Type != WBPROJECT || !tool) return 0;
    for (part = tool; *tool; tool++)
        if (*tool == ':' || *tool == '/') part = tool + 1;
    for (i = 0; i < 5; i++)
        if ((part[i] | 32) != want[i]) return 0;
    return part[5] == 0;
}

/* 0 when something of the folder may be missing from what was sent. */
static int walk(struct ln_scan *s, const char *dir, int depth)
{
    int whole = 1;
    struct FileInfoBlock *fib;
    char *path;
    BPTR lock;
    /* a folder that is not there has no programs; one that cannot be read
       for another reason (its volume is away) keeps what was known */
    if (!(lock = Lock((CONST_STRPTR)dir, SHARED_LOCK))) return depth > 0 || IoErr() == ERROR_OBJECT_NOT_FOUND;
    fib = AllocDosObject(DOS_FIB, NULL);
    path = AllocVec(PATH_MAX_LEN, MEMF_PUBLIC);
    if (!fib || !path) whole = 0;
    if (fib && path && Examine(lock, fib) && fib->fib_DirEntryType > 0) {
        while (!stopping() && ExNext(lock, fib)) {
            const char *name = (const char *)fib->fib_FileName;
            if (!join(path, dir, name)) continue;
            if (fib->fib_DirEntryType > 0) {
                if (depth < 1 && !walk(s, path, depth + 1)) whole = 0;
            } else if (!is_info(name)) {
                /* GetDiskObject reads <path>.info: no icon file, no program */
                struct DiskObject *icon = GetDiskObject((CONST_STRPTR)path);
                if (icon) {
                    if ((icon->do_Type == WBTOOL || is_script(icon)) && !send(s, LN_SCAN_PUT, path, name)) whole = 0;
                    FreeDiskObject(icon);
                }
            }
        }
    }
    if (path) FreeVec(path);
    if (fib) FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return whole;
}

static void scan_folder(struct ln_scan *s, int i)
{
    int whole;
    if (!send(s, LN_SCAN_BEGIN, s->folder[i], NULL)) return;
    whole = walk(s, s->folder[i], 0);
    /* a scan cut short or incomplete must not make programs vanish */
    if (whole && !stopping()) send(s, LN_SCAN_END, s->folder[i], NULL);
}

/* --- the file index ------------------------------------------------------- */

#define BLOB 8192
#define DEPTH_MAX 12

static int same_path(const char *a, const char *b)
{
    while (*a && *b && (*a | 32) == (*b | 32)) { a++; b++; }
    /* a trailing '/' on either side does not count */
    while (*a == '/') a++;
    while (*b == '/') b++;
    return !*a && !*b;
}

/* Whether the entry name of drawer dir (full path in path) is kept out. */
static int excluded(struct ln_scan *s, const char *path, const char *name)
{
    int i;
    for (i = 0; i < s->exclude_n; i++) {
        if (s->pattern[i]) { if (MatchPatternNoCase((CONST_STRPTR)s->pattern[i], (CONST_STRPTR)name)) return 1; }
        else if (path && same_path(s->exclude[i], path)) return 1;
    }
    return 0;
}

/* Sends what was gathered of one drawer and starts again. 0 when it was lost. */
static int flush(struct ln_scan *s, const char *dir, char *blob, ULONG *used)
{
    int ok;
    if (!*used) return 1;
    blob[*used] = 0;
    ok = send(s, LN_SCAN_FILES, dir, blob);
    *used = 0;
    return ok;
}

/* Walks dir and everything below it. 0 when it was cut short or something
 * of it could not be read or sent. A drawer below that has gone meanwhile is
 * empty; the folder itself (depth 0) is the caller's to check. */
static int walk_files(struct ln_scan *s, const char *dir, int depth, char *blob)
{
    struct FileInfoBlock *fib;
    char *path;
    BPTR lock;
    ULONG used = 0;
    int whole = 1;
    if (!(lock = Lock((CONST_STRPTR)dir, SHARED_LOCK))) return depth > 0 && IoErr() == ERROR_OBJECT_NOT_FOUND;
    fib = AllocDosObject(DOS_FIB, NULL);
    path = AllocVec(PATH_MAX_LEN, MEMF_PUBLIC);
    if (!fib || !path) whole = 0;
    if (fib && path && Examine(lock, fib) && fib->fib_DirEntryType > 0) {
        while (ExNext(lock, fib)) {
            const char *name = (const char *)fib->fib_FileName;
            ULONG nn = len(name);
            int is_dir = fib->fib_DirEntryType > 0;
            if (stopping()) { whole = 0; break; }
            /* icons belong to their files; names starting with a dot are kept out */
            if (!nn || name[0] == '.' || (!is_dir && is_info(name))) continue;
            if (s->exclude_n && (!join(path, dir, name) || excluded(s, path, name))) continue;
            if (used + nn + 3 >= BLOB && !flush(s, dir, blob, &used)) { whole = 0; break; }
            blob[used++] = is_dir ? 'D' : 'F';
            CopyMem((APTR)name, blob + used, nn);
            used += nn;
            blob[used++] = '\n';
        }
        if (!flush(s, dir, blob, &used)) whole = 0;
        /* the drawers below, after this one's entries went out: the blob is free again */
        if (whole && depth < DEPTH_MAX && Examine(lock, fib)) {
            while (whole && ExNext(lock, fib)) {
                const char *name = (const char *)fib->fib_FileName;
                if (fib->fib_DirEntryType <= 0 || name[0] == '.' || !join(path, dir, name)) continue;
                if (s->exclude_n && excluded(s, path, name)) continue;
                whole = walk_files(s, path, depth + 1, blob);
            }
        }
    }
    if (path) FreeVec(path);
    if (fib) FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return whole;
}

static void index_files(struct ln_scan *s)
{
    char *blob = AllocVec(BLOB + 4, MEMF_PUBLIC);
    int i, whole = 1;
    struct DateStamp t0, t1;
    if (!blob) return;
    DateStamp(&t0);
    if (!send(s, LN_SCAN_FILES_BEGIN, NULL, NULL)) { FreeVec(blob); return; }
    for (i = 0; i < s->files_n && whole; i++) {
        BPTR lock;
        if (excluded(s, s->files[i], "")) continue;
        if ((lock = Lock((CONST_STRPTR)s->files[i], SHARED_LOCK))) {
            UnLock(lock);
            whole = walk_files(s, s->files[i], 0, blob);
        } else {
            /* the folder cannot be reached (its volume is away, or it is
               gone): what was known of it stays in the index */
            whole = send(s, LN_SCAN_FILES_KEEP, s->files[i], NULL);
        }
    }
    /* a walk cut short or incomplete must not replace a whole index */
    if (whole && !stopping()) {
        /* how long the walk took, in milliseconds, as the message's text */
        char ms[16];
        ULONG v, k = 0, d = 1;
        DateStamp(&t1);
        v = (ULONG)((t1.ds_Days - t0.ds_Days) * 1440 * 3000 + (t1.ds_Minute - t0.ds_Minute) * 3000 + (t1.ds_Tick - t0.ds_Tick)) * 20;
        while (v / d >= 10) d *= 10;
        for (; d; d /= 10) ms[k++] = (char)('0' + v / d % 10);
        ms[k] = 0;
        send(s, LN_SCAN_FILES_END, NULL, ms);
    }
    FreeVec(blob);
}

/* The package catalogue: apkg prints it, the main task reads the file.
 * apkg is looked for where its package and its manual install put it. */
static void packages(struct ln_scan *s)
{
    static const char *const places[] = { "C:apkg", "SYS:Packages/arospkg/apkg" };
    char cmd[160];
    ULONG i, k, n;
    for (i = 0; i < 2; i++) {
        BPTR lock = Lock((CONST_STRPTR)places[i], SHARED_LOCK);
        if (!lock) continue;
        UnLock(lock);
        n = len(places[i]);
        CopyMem((APTR)places[i], cmd, n);
        k = len(" search --json >" LN_PACKAGES_FILE) + 1;
        CopyMem((APTR)" search --json >" LN_PACKAGES_FILE, cmd + n, k);
        if (SystemTags((CONST_STRPTR)cmd, SYS_Input, (IPTR)NULL, SYS_Output, (IPTR)NULL, TAG_DONE) != -1)
            send(s, LN_SCAN_PACKAGES, places[i], NULL);
        return;
    }
}

static void entry(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct ln_scan *s = me->pr_Task.tc_UserData;
    struct MsgPort *port = CreateMsgPort();
    ULONG portsig = port ? 1UL << port->mp_SigBit : 0;
    int i, running = 1;

    me->pr_WindowPtr = (APTR)-1;        /* a missing volume is an error, not a requester */
    if (port)
        for (i = 0; i < s->n; i++) {
            struct NotifyRequest *nr = &s->notify[i];
            nr->nr_Name = (STRPTR)s->folder[i];
            nr->nr_Flags = NRF_SEND_MESSAGE;
            nr->nr_stuff.nr_Msg.nr_Port = port;
            nr->nr_UserData = (IPTR)i;
            if (!StartNotify(nr)) nr->nr_Name = NULL;       /* not watched */
        }
    for (i = 0; i < s->n && !stopping(); i++) scan_folder(s, i);
    if (!stopping()) packages(s);
    if (!stopping() && s->files_n) index_files(s);

    while (running) {
        ULONG got = Wait(portsig | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F);
        struct NotifyMessage *nm;
        int any = 0;
        if (got & SIGBREAKF_CTRL_C) break;
        if (got & SIGBREAKF_CTRL_F)
            for (i = 0; i < s->n; i++) s->dirty[i] = any = 1;
        if (got & (SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F)) packages(s);
        if (s->want_files) { s->want_files = 0; if (s->files_n) index_files(s); }
        if ((got & SIGBREAKF_CTRL_D) && s->busy) {
            SystemTags((CONST_STRPTR)s->command, SYS_Input, (IPTR)NULL, SYS_Output, (IPTR)NULL, TAG_DONE);
            /* free for the next command before the main task hears of this one */
            s->busy = 0;
            send(s, LN_SCAN_RAN, s->command, NULL);
        }
        if (port) {
            /* a copy into a folder sends many notes: wait for them to stop */
            do {
                while ((nm = (struct NotifyMessage *)GetMsg(port))) {
                    LONG which = (LONG)nm->nm_NReq->nr_UserData;
                    if (which >= 0 && which < s->n) s->dirty[which] = any = 1;
                    ReplyMsg(&nm->nm_ExecMessage);
                }
                if (any) Delay(25);
            } while (any && !IsMsgPortEmpty(port));
        }
        for (i = 0; i < s->n && running; i++) {
            if (stopping()) { running = 0; break; }
            if (s->dirty[i]) { s->dirty[i] = 0; scan_folder(s, i); }
        }
    }

    if (port) {
        struct Message *m;
        for (i = 0; i < s->n; i++)
            if (s->notify[i].nr_Name) EndNotify(&s->notify[i]);
        while ((m = GetMsg(port))) ReplyMsg(m);
        DeleteMsgPort(port);
    }
    /* The last thing this process does; the main task waits for it. */
    Forbid();
    PutMsg(s->to, &s->last->m);
}

void ln_scan_free_msg(struct ln_scan_msg *m) { FreeVec(m); }

static void free_scan(struct ln_scan *s)
{
    int i;
    if (s->folder)
        for (i = 0; i < s->n; i++)
            if (s->folder[i]) FreeVec(s->folder[i]);
    if (s->folder) FreeVec(s->folder);
    if (s->files)
        for (i = 0; i < s->files_n; i++)
            if (s->files[i]) FreeVec(s->files[i]);
    if (s->files) FreeVec(s->files);
    if (s->exclude)
        for (i = 0; i < s->exclude_n; i++) {
            if (s->exclude[i]) FreeVec(s->exclude[i]);
            if (s->pattern && s->pattern[i]) FreeVec(s->pattern[i]);
        }
    if (s->exclude) FreeVec(s->exclude);
    if (s->pattern) FreeVec(s->pattern);
    if (s->notify) FreeVec(s->notify);
    if (s->dirty) FreeVec(s->dirty);
    if (s->last) FreeVec(s->last);
    FreeVec(s);
}

struct ln_scan *ln_scan_start(struct MsgPort *to, char *const *folders, int n, char *const *files, int files_n,
                              char *const *exclude, int exclude_n)
{
    struct ln_scan *s = AllocVec(sizeof *s, MEMF_PUBLIC | MEMF_CLEAR);
    int i;
    if (!s) return NULL;
    s->to = to;
    s->n = n;
    s->folder = AllocVec(sizeof(char *) * (ULONG)(n + 1), MEMF_PUBLIC | MEMF_CLEAR);
    s->notify = AllocVec(sizeof(struct NotifyRequest) * (ULONG)(n + 1), MEMF_PUBLIC | MEMF_CLEAR);
    s->dirty = AllocVec((ULONG)(n + 1), MEMF_PUBLIC | MEMF_CLEAR);
    /* the scanner's last message exists before it starts, so that it can
       always say that it has gone */
    if ((s->last = AllocVec(sizeof *s->last, MEMF_PUBLIC | MEMF_CLEAR))) {
        s->last->m.mn_Length = (UWORD)sizeof *s->last;
        s->last->kind = LN_SCAN_EXIT;
    }
    if (!s->folder || !s->notify || !s->dirty || !s->last) { free_scan(s); return NULL; }
    for (i = 0; i < n; i++) {
        ULONG k = len(folders[i]) + 1;
        if (!(s->folder[i] = AllocVec(k, MEMF_PUBLIC))) { free_scan(s); return NULL; }
        CopyMem((APTR)folders[i], s->folder[i], k);
    }
    s->files = AllocVec(sizeof(char *) * (ULONG)(files_n + 1), MEMF_PUBLIC | MEMF_CLEAR);
    if (!s->files) { free_scan(s); return NULL; }
    s->files_n = files_n;
    for (i = 0; i < files_n; i++) {
        ULONG k = len(files[i]) + 1;
        if (!(s->files[i] = AllocVec(k, MEMF_PUBLIC))) { free_scan(s); return NULL; }
        CopyMem((APTR)files[i], s->files[i], k);
    }
    s->exclude = AllocVec(sizeof(char *) * (ULONG)(exclude_n + 1), MEMF_PUBLIC | MEMF_CLEAR);
    s->pattern = AllocVec(sizeof(char *) * (ULONG)(exclude_n + 1), MEMF_PUBLIC | MEMF_CLEAR);
    if (!s->exclude || !s->pattern) { free_scan(s); return NULL; }
    s->exclude_n = exclude_n;
    for (i = 0; i < exclude_n; i++) {
        ULONG k = len(exclude[i]) + 1, c, is_path = 0;
        if (!(s->exclude[i] = AllocVec(k, MEMF_PUBLIC))) { free_scan(s); return NULL; }
        CopyMem((APTR)exclude[i], s->exclude[i], k);
        for (c = 0; c + 1 < k; c++) if (exclude[i][c] == ':' || exclude[i][c] == '/') is_path = 1;
        if (!is_path) {
            /* a name pattern; one that does not parse matches nothing */
            if ((s->pattern[i] = AllocVec(k * 2 + 4, MEMF_PUBLIC)) &&
                ParsePatternNoCase((CONST_STRPTR)exclude[i], (STRPTR)s->pattern[i], (LONG)(k * 2 + 2)) < 0) {
                FreeVec(s->pattern[i]);
                s->pattern[i] = NULL;
                s->exclude[i][0] = 0;
            }
        }
    }
    s->proc = CreateNewProcTags(NP_Entry, (IPTR)entry, NP_Name, (IPTR)"Jumpstart scanner",
                                NP_StackSize, 65536, NP_Priority, -1, NP_UserData, (IPTR)s,
                                NP_WindowPtr, (IPTR)-1, NP_CopyVars, FALSE, TAG_DONE);
    if (!s->proc) { free_scan(s); return NULL; }
    return s;
}

void ln_scan_again(struct ln_scan *s)
{
    if (s && s->proc) Signal(&s->proc->pr_Task, SIGBREAKF_CTRL_F);
}

void ln_scan_files(struct ln_scan *s)
{
    if (s && s->proc) { s->want_files = 1; Signal(&s->proc->pr_Task, SIGBREAKF_CTRL_E); }
}

void ln_scan_packages(struct ln_scan *s)
{
    if (s && s->proc) Signal(&s->proc->pr_Task, SIGBREAKF_CTRL_E);
}

int ln_scan_run(struct ln_scan *s, const char *command)
{
    ULONG n = len(command) + 1;
    if (!s || !s->proc || s->busy || n > sizeof s->command) return 0;
    CopyMem((APTR)command, s->command, n);
    s->busy = 1;
    Signal(&s->proc->pr_Task, SIGBREAKF_CTRL_D);
    return 1;
}

void ln_scan_stop(struct ln_scan *s)
{
    int gone = 0;
    if (!s) return;
    Signal(&s->proc->pr_Task, SIGBREAKF_CTRL_C);
    while (!gone) {
        struct ln_scan_msg *m;
        WaitPort(s->to);
        while ((m = (struct ln_scan_msg *)GetMsg(s->to))) {
            /* only scanner messages can arrive here: the caller has taken
               its other senders off the port before stopping the scanner */
            if (m->kind == LN_SCAN_EXIT) { gone = 1; s->last = NULL; }
            ln_scan_free_msg(m);
        }
    }
    free_scan(s);
}
