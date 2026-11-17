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
};

static ULONG len(const char *s) { ULONG n = 0; while (s[n]) n++; return n; }

static void send(struct ln_scan *s, LONG kind, const char *path, const char *title)
{
    ULONG pn = path ? len(path) + 1 : 0, tn = title ? len(title) + 1 : 0;
    struct ln_scan_msg *m = AllocVec(sizeof *m + pn + tn, MEMF_PUBLIC | MEMF_CLEAR);
    if (!m) return;
    m->m.mn_Length = (UWORD)sizeof *m;
    m->kind = kind;
    if (path) { m->path = (char *)(m + 1); CopyMem((APTR)path, m->path, pn); }
    if (title) { m->title = (char *)(m + 1) + pn; CopyMem((APTR)title, m->title, tn); }
    PutMsg(s->to, &m->m);
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

static void walk(struct ln_scan *s, const char *dir, int depth)
{
    struct FileInfoBlock *fib;
    char *path;
    BPTR lock;
    if (!(lock = Lock((CONST_STRPTR)dir, SHARED_LOCK))) return;
    fib = AllocDosObject(DOS_FIB, NULL);
    path = AllocVec(PATH_MAX_LEN, MEMF_PUBLIC);
    if (fib && path && Examine(lock, fib) && fib->fib_DirEntryType > 0) {
        while (!stopping() && ExNext(lock, fib)) {
            const char *name = (const char *)fib->fib_FileName;
            if (!join(path, dir, name)) continue;
            if (fib->fib_DirEntryType > 0) {
                if (depth < 1) walk(s, path, depth + 1);
            } else if (!is_info(name)) {
                /* GetDiskObject reads <path>.info: no icon file, no program */
                struct DiskObject *icon = GetDiskObject((CONST_STRPTR)path);
                if (icon) {
                    if (icon->do_Type == WBTOOL || is_script(icon)) send(s, LN_SCAN_PUT, path, name);
                    FreeDiskObject(icon);
                }
            }
        }
    }
    if (path) FreeVec(path);
    if (fib) FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
}

static void scan_folder(struct ln_scan *s, int i)
{
    send(s, LN_SCAN_BEGIN, s->folder[i], NULL);
    walk(s, s->folder[i], 0);
    /* a scan cut short must not make programs vanish */
    if (!stopping()) send(s, LN_SCAN_END, s->folder[i], NULL);
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

    while (running) {
        ULONG got = Wait(portsig | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F);
        struct NotifyMessage *nm;
        int any = 0;
        if (got & SIGBREAKF_CTRL_C) break;
        if (got & SIGBREAKF_CTRL_F)
            for (i = 0; i < s->n; i++) s->dirty[i] = any = 1;
        if (got & (SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F)) packages(s);
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
    send(s, LN_SCAN_EXIT, NULL, NULL);
}

void ln_scan_free_msg(struct ln_scan_msg *m) { FreeVec(m); }

static void free_scan(struct ln_scan *s)
{
    int i;
    if (s->folder)
        for (i = 0; i < s->n; i++)
            if (s->folder[i]) FreeVec(s->folder[i]);
    if (s->folder) FreeVec(s->folder);
    if (s->notify) FreeVec(s->notify);
    if (s->dirty) FreeVec(s->dirty);
    FreeVec(s);
}

struct ln_scan *ln_scan_start(struct MsgPort *to, char *const *folders, int n)
{
    struct ln_scan *s = AllocVec(sizeof *s, MEMF_PUBLIC | MEMF_CLEAR);
    int i;
    if (!s) return NULL;
    s->to = to;
    s->n = n;
    s->folder = AllocVec(sizeof(char *) * (ULONG)(n + 1), MEMF_PUBLIC | MEMF_CLEAR);
    s->notify = AllocVec(sizeof(struct NotifyRequest) * (ULONG)(n + 1), MEMF_PUBLIC | MEMF_CLEAR);
    s->dirty = AllocVec((ULONG)(n + 1), MEMF_PUBLIC | MEMF_CLEAR);
    if (!s->folder || !s->notify || !s->dirty) { free_scan(s); return NULL; }
    for (i = 0; i < n; i++) {
        ULONG k = len(folders[i]) + 1;
        if (!(s->folder[i] = AllocVec(k, MEMF_PUBLIC))) { free_scan(s); return NULL; }
        CopyMem((APTR)folders[i], s->folder[i], k);
    }
    s->proc = CreateNewProcTags(NP_Entry, (IPTR)entry, NP_Name, (IPTR)"Launcher scanner",
                                NP_StackSize, 65536, NP_Priority, -1, NP_UserData, (IPTR)s,
                                NP_WindowPtr, (IPTR)-1, NP_CopyVars, FALSE, TAG_DONE);
    if (!s->proc) { free_scan(s); return NULL; }
    return s;
}

void ln_scan_again(struct ln_scan *s)
{
    if (s && s->proc) Signal(&s->proc->pr_Task, SIGBREAKF_CTRL_F);
}

void ln_scan_packages(struct ln_scan *s)
{
    if (s && s->proc) Signal(&s->proc->pr_Task, SIGBREAKF_CTRL_E);
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
            if (m->kind == LN_SCAN_EXIT) gone = 1;
            ln_scan_free_msg(m);
        }
    }
    free_scan(s);
}
