/* SPDX-License-Identifier: MIT */
/* The launcher: a commodity whose hotkey opens a search panel.
 *
 * Arguments (Shell) or ToolTypes (Workbench):
 *   CX_POPKEY=<hotkey>    default "ctrl alt space"
 *   CX_PRIORITY=<n>       default 0
 *   CX_POPUP=YES|NO       show the panel at start, default NO
 *   LOG=<file>            write what happens to a file (for tests)
 *   SETTINGS=<drawer>     where settings and use counts are kept,
 *                         default ENVARC:Launcher
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aros/startup.h>
#include <clib/alib_protos.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <exec/pm.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <libraries/commodities.h>
#include <proto/commodities.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/workbench.h>

#include "autostart_aros.h"
#include "calc.h"
#include "config.h"
#include "draw.h"
#include "icons_aros.h"
#include "index.h"
#include "keys_aros.h"
#include "layout.h"
#include "pkglist.h"
#include "prefs_aros.h"
#include "queryline.h"
#include "results.h"
#include "scan_aros.h"
#include "style.h"
#include "zk_glass_core.h"
#include "zk_present.h"
#include "zk_text_aros.h"

static const char version[] __attribute__((used)) = "$VER: Launcher 0.2 (17.11.2026)";

#define EVT_HOTKEY 1
#define SHADOW 28
#define BLUR 10
#define QUERY_ROWS 60

struct panel {
    struct Window *win;
    const struct ln_style *style;
    struct ln_fonts f;
    int font_dark, font_compact, fonts_open;
    int composed, captured, pinned, shadow;
    int w, h;                       /* the window: the largest picture */
    unsigned char *pic, *under, *glass;
    struct Region *region;
    struct ln_layout l;
    struct ln_draw_cache cache;
    int dirty;                      /* something changed: draw once the pending input is handled */
    int first;
    ze_doc *doc;
    struct ln_results *res;
    int caret_on, ticks;
    char footer[160];
    int click_row;
    ULONG click_secs, click_micros;
};

static struct {
    struct ln_config cfg;
    struct ln_index *index;
    struct ln_scan *scan;
    struct ln_icons *icons;
    struct MsgPort *cxport, *scanport;
    CxObj *broker;
    char logname[256];
    struct panel p;
    int index_changed;
    char dir[256], config[272], counts[272];
    char program[512];      /* this program's full path, "" when unknown */
    char calc[64];          /* the calculator's result on show */
    int quit, want_prefs;
    unsigned confirm;       /* a command waiting for a second Return */
    struct ln_pkglist pkgs; /* apkg's catalogue */
    char apkg[64];          /* apkg's path, "" when there is none */
    int confirm_pkg;        /* a package waiting for a second Return, -1 none */
    int pkgs_stale;         /* an install was started: ask for the list again */
} app;

/* --- small things -------------------------------------------------------- */

static void note(const char *fmt, ...)
{
    char line[400];
    struct DateStamp ds;
    va_list ap;
    int n;
    BPTR f;
    if (!app.logname[0]) return;
    DateStamp(&ds);
    n = snprintf(line, sizeof line, "%ld.%02ld ", (long)(ds.ds_Minute * 60 + ds.ds_Tick / 50), (long)(ds.ds_Tick % 50 * 2));
    va_start(ap, fmt);
    n += vsnprintf(line + n, sizeof line - (size_t)n - 1, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof line - 2) n = (int)sizeof line - 2;
    line[n++] = '\n';
    /* opened and closed for every line: the file can be read at any time,
       also after the program was stopped by force */
    if ((f = Open((CONST_STRPTR)app.logname, MODE_READWRITE))) {
        Seek(f, 0, OFFSET_END);
        Write(f, line, n);
        Close(f);
    }
}


static unsigned now(void)
{
    struct DateStamp ds;
    DateStamp(&ds);
    return (unsigned)ds.ds_Days * 86400u + (unsigned)ds.ds_Minute * 60u + (unsigned)ds.ds_Tick / 50u;
}

static char *read_file(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    char *buf = NULL;
    long size;
    *n = 0;
    if (!f) return NULL;
    if (!fseek(f, 0, SEEK_END) && (size = ftell(f)) >= 0 && size < 4 * 1024 * 1024 && !fseek(f, 0, SEEK_SET)
        && (buf = malloc((size_t)size + 1))) {
        *n = fread(buf, 1, (size_t)size, f);
        buf[*n] = 0;
    }
    fclose(f);
    return buf;
}

static int write_file(const char *path, const char *text, size_t n)
{
    FILE *f = fopen(path, "wb");
    int ok;
    if (!f) return 0;
    ok = fwrite(text, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

/* Runs f without requesters: a missing volume is an error to handle. */
static APTR quiet(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR was = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;
    return was;
}
static void loud(APTR was) { ((struct Process *)FindTask(NULL))->pr_WindowPtr = was; }

static void save_counts(void)
{
    size_t n;
    char *text = ln_index_save_counts(app.index, &n);
    APTR was = quiet();
    BPTR dir;
    if (text) {
        if ((dir = CreateDir((CONST_STRPTR)app.dir))) UnLock(dir);
        if (!write_file(app.counts, text, n)) note("counts not saved");
        free(text);
    }
    loud(was);
}

static void save_config(void)
{
    size_t n;
    char *text = ln_config_text(&app.cfg, &n);
    APTR was = quiet();
    BPTR dir;
    if (text) {
        if ((dir = CreateDir((CONST_STRPTR)app.dir))) UnLock(dir);
        if (!write_file(app.config, text, n)) note("settings not saved");
        free(text);
    }
    loud(was);
}

static void load_settings(void)
{
    size_t n;
    char *text;
    APTR was = quiet();
    memset(&app.cfg, 0, sizeof app.cfg);
    app.cfg.dark = 1;
    app.cfg.glass = 1;
    app.cfg.autostart = -1;
    if ((text = read_file(app.config, &n))) {
        ln_config_parse(&app.cfg, text, n);
        free(text);
    }
    if (!app.cfg.folders) {
        struct ln_config d;
        int i;
        if (ln_config_defaults(&d)) {
            for (i = 0; i < d.folders; i++) ln_config_add_folder(&app.cfg, d.folder[i]);
            ln_config_free(&d);
        }
    }
    if ((text = read_file(app.counts, &n))) {
        ln_index_load_counts(app.index, text, n);
        free(text);
    }
    loud(was);
}

/* --- the rows ------------------------------------------------------------ */

/* The drawer of a path, as text to show. malloc'd through the text layer. */
static char *drawer_of(const char *path)
{
    size_t n = strlen(path);
    char *part, *shown;
    while (n && path[n - 1] != '/' && path[n - 1] != ':') n--;
    if (n > 1 && path[n - 1] == '/') n--;
    if (!(part = malloc(n + 1))) return NULL;
    memcpy(part, path, n);
    part[n] = 0;
    shown = zt_path_from_dos(part);
    free(part);
    return shown;
}

/* What the launcher itself can do, found by name like programs. */
enum { CMD_SETTINGS = 1, CMD_RESCAN, CMD_QUIT, CMD_RESTART, CMD_SHUTDOWN };
#define CMD_KEY 0xffff0000u         /* row keys of commands; program ids are far below */
#define WIN_KEY 0xfffe0000u         /* row keys of open windows */
static const struct { unsigned id; const char *title, *subtitle; } commands[] = {
    { CMD_SETTINGS, "Launcher Settings", "Folders, look, start with the system" },
    { CMD_RESCAN, "Rescan Programs", "Look through the folders again" },
    { CMD_QUIT, "Quit Launcher", "Stop the launcher until it is started again" },
    { CMD_RESTART, "Restart Computer", "Asks once more before it restarts" },
    { CMD_SHUTDOWN, "Shut Down Computer", "Asks once more before it switches off" },
};

/* The windows that were open when the panel was called: what is shown
 * (copied under Intuition's lock) and the pointers as tokens. A token is
 * used only after it was found again in Intuition's lists. */
#define WIN_MAX 64
static struct {
    struct Window *w;
    struct Screen *s;
    char raw[128];          /* the title as Intuition has it, to recognise the window again */
    char title[400];        /* the same as UTF-8 */
    char where[160];        /* its screen, as UTF-8 */
} wins[WIN_MAX];
static int win_count;

static void copy_shown(char *out, size_t cap, const char *local)
{
    char *u = zt_path_from_dos(local);
    snprintf(out, cap, "%s", u ? u : "");
    if (u) zt_free(u);
}

static void snapshot_windows(void)
{
    static char screens[WIN_MAX][64];
    struct Screen *sc;
    struct Window *w;
    ULONG lock;
    int i;
    win_count = 0;
    lock = LockIBase(0);
    for (sc = IntuitionBase->FirstScreen; sc; sc = sc->NextScreen)
        for (w = sc->FirstWindow; w && win_count < WIN_MAX; w = w->NextWindow) {
            const char *st = (const char *)(sc->DefaultTitle ? sc->DefaultTitle : sc->Title);
            if (!w->Title || !*w->Title || (w->Flags & WFLG_BACKDROP)) continue;
            wins[win_count].w = w;
            wins[win_count].s = sc;
            strncpy(wins[win_count].raw, (const char *)w->Title, sizeof wins[0].raw - 1);
            wins[win_count].raw[sizeof wins[0].raw - 1] = 0;
            strncpy(screens[win_count], st ? st : "", sizeof screens[0] - 1);
            screens[win_count][sizeof screens[0] - 1] = 0;
            win_count++;
        }
    UnlockIBase(lock);
    for (i = 0; i < win_count; i++) {
        copy_shown(wins[i].title, sizeof wins[i].title, wins[i].raw);
        copy_shown(wins[i].where, sizeof wins[i].where, screens[i]);
    }
}

/* Brings a window of the snapshot to the front if it is still there. */
static int show_window(int i)
{
    struct Screen *sc;
    struct Window *w;
    ULONG lock;
    int found = 0;
    if (i < 0 || i >= win_count) return 0;
    lock = LockIBase(0);
    for (sc = IntuitionBase->FirstScreen; sc && !found; sc = sc->NextScreen)
        if (sc == wins[i].s)
            for (w = sc->FirstWindow; w; w = w->NextWindow)
                if (w == wins[i].w && w->Title && !strncmp((const char *)w->Title, wins[i].raw, sizeof wins[0].raw - 1)) { found = 1; break; }
    UnlockIBase(lock);
    if (!found) return 0;
    ScreenToFront(wins[i].s);
    WindowToFront(wins[i].w);
    ActivateWindow(wins[i].w);
    return 1;
}

/* Answers the current query: the calculator's row, then one list of
 * programs and the launcher's own commands, best match first. Commands
 * and programs are ranked together, so that "quit" finds Quit Launcher
 * before a program that merely has those letters. */
static void answer(void)
{
    struct panel *p = &app.p;
    const struct ln_entry *e[QUERY_ROWS];
    static struct ln_match m[QUERY_ROWS];
    static struct ln_row row[QUERY_ROWS + WIN_MAX + 8];
    char *sub[QUERY_ROWS];
    size_t qn, c;
    const char *q = ze_text(p->doc, &qn);
    unsigned cur = ln_results_current(p->res);
    int n, i, total;

    /* "> command": only the Shell's row */
    memset(row, 0, sizeof row);
    total = 0;
    if (qn && q[0] == '>') {
        const char *cmd = q + 1;
        while (*cmd == ' ') cmd++;
        if (*cmd) {
            row[0].key = 1; row[0].title = cmd; row[0].subtitle = "Run as a Shell command"; row[0].kind = "Shell"; row[0].score = 1;
            total = 1;
        }
        ln_results_answer(p->res, cur, LN_SRC_SHELL, row, total);
        ln_results_answer(p->res, cur, LN_SRC_CALC, row, 0);
        ln_results_answer(p->res, cur, LN_SRC_PROGRAMS, row, 0);
        ln_results_answer(p->res, cur, LN_SRC_PACKAGES, row, 0);
        return;
    }
    ln_results_answer(p->res, cur, LN_SRC_SHELL, row, 0);

    if (ln_calc(q, qn, app.calc, sizeof app.calc)) {
        row[0].key = 1; row[0].title = app.calc; row[0].subtitle = q; row[0].kind = "Result"; row[0].score = 1;
        total = 1;
    }
    ln_results_answer(p->res, cur, LN_SRC_CALC, row, total);

    memset(row, 0, sizeof row);
    n = ln_index_query(app.index, q, qn, e, m, QUERY_ROWS);
    for (i = 0; i < n; i++) {
        unsigned uses = e[i]->uses > 20 ? 20 : e[i]->uses;
        sub[i] = drawer_of(e[i]->path);
        row[i].key = e[i]->id;
        row[i].title = e[i]->title;
        row[i].subtitle = sub[i];
        row[i].kind = "Program";
        row[i].m = m[i];
        /* the index's own order; with a query that is its rank */
        row[i].score = qn ? m[i].score + (int)uses * 3 : QUERY_ROWS - i;
    }
    total = n;
    for (c = 0; qn && c < sizeof commands / sizeof commands[0]; c++)
        if (ln_match(q, qn, commands[c].title, strlen(commands[c].title), &row[total].m)) {
            row[total].key = CMD_KEY | commands[c].id;
            row[total].title = commands[c].title;
            row[total].subtitle = commands[c].subtitle;
            row[total].kind = "Command";
            row[total].score = row[total].m.score;
            total++;
        }
    for (i = 0; qn && i < win_count; i++)
        if (ln_match(q, qn, wins[i].title, strlen(wins[i].title), &row[total].m)) {
            row[total].key = WIN_KEY | (unsigned)i;
            row[total].title = wins[i].title;
            row[total].subtitle = wins[i].where;
            row[total].kind = "Window";
            row[total].score = row[total].m.score - 5;       /* a program of the same name comes first */
            total++;
        }
    ln_results_answer(p->res, cur, LN_SRC_PROGRAMS, row, total);
    for (i = 0; i < n; i++) zt_free(sub[i]);

    /* packages that are not installed, by their name; installed ones are
       found as programs */
    memset(row, 0, sizeof row);
    total = 0;
    /* by name (letters in order, as for programs) or, from three letters
       on, by words of the description; a name match comes first */
    for (i = 0; qn >= 2 && i < app.pkgs.n && total < 5; i++) {
        const struct ln_pkg *g = &app.pkgs.v[i];
        int by_name, by_text = 0;
        if (g->installed) continue;
        by_name = ln_match(q, qn, g->id, strlen(g->id), &row[total].m);
        if (!by_name && qn >= 3 && g->summary)
            by_text = zt_contains_folded(g->summary, strlen(g->summary), q, qn) == 1;
        if (!by_name && !by_text) continue;
        if (!by_name) memset(&row[total].m, 0, sizeof row[total].m);
        row[total].key = (unsigned)i;
        row[total].title = g->id;
        row[total].subtitle = g->summary ? g->summary : "";
        row[total].kind = g->version ? g->version : "Package";
        row[total].score = by_name ? 1000 + row[total].m.score : 0;
        total++;
    }
    ln_results_answer(p->res, cur, LN_SRC_PACKAGES, row, total);
}

/* --- icons --------------------------------------------------------------- */

static const unsigned char *row_icon(void *ctx, const struct ln_row *row, int size)
{
    (void)ctx;
    return row->source == LN_SRC_PROGRAMS && row->key < WIN_KEY ? ln_icons_get(app.icons, row->key, size) : NULL;
}

static const char *icon_path(void *ctx, unsigned id)
{
    const struct ln_entry *e = ln_index_find(app.index, id);
    (void)ctx;
    return e ? e->path : NULL;
}

/* --- the panel ----------------------------------------------------------- */

static void close_fonts(struct panel *p)
{
    if (p->f.query) zt_font_close(p->f.query);
    if (p->f.title) zt_font_close(p->f.title);
    if (p->f.small) zt_font_close(p->f.small);
    if (p->f.big) zt_font_close(p->f.big);
    memset(&p->f, 0, sizeof p->f);
    p->fonts_open = 0;
}

static int open_fonts(struct panel *p)
{
    if (p->fonts_open && p->font_dark == app.cfg.dark && p->font_compact == app.cfg.compact) return 1;
    close_fonts(p);
    p->f.query = zt_open_for(NULL, p->style->font_query, NULL, 0, NULL);
    p->f.title = zt_open_for(NULL, p->style->font_title, NULL, 0, NULL);
    p->f.small = zt_open_for(NULL, p->style->font_small, NULL, 0, NULL);
    p->f.big = zt_open_for(NULL, p->style->font_big, NULL, 0, NULL);        /* may fail: the card then uses the query's font */
    if (!p->f.query || !p->f.title || !p->f.small) { close_fonts(p); return 0; }
    p->font_dark = app.cfg.dark; p->font_compact = app.cfg.compact; p->fonts_open = 1;
    return 1;
}

static void paint(struct panel *p)
{
    struct ln_view v;
    size_t qn, a, b;
    memset(&v, 0, sizeof v);
    v.query = ze_text(p->doc, &qn);
    v.qn = qn;
    v.caret = ze_caret(p->doc);
    ze_selection(p->doc, &a, &b);
    v.sel_start = a; v.sel_end = b;
    v.caret_on = p->caret_on;
    v.results = p->res;
    v.footer = p->footer[0] ? p->footer : NULL;
    v.backdrop = p->glass;
    v.icon = row_icon;
    ln_layout_make(&p->l, p->style, p->res, p->first, p->shadow);
    p->first = p->l.first;
    if (p->pinned) ln_layout_pin(&p->l, p->style);
    memset(p->pic, 0, (size_t)p->w * (size_t)p->h * 4);
    /* the picture never leaves its buffer, whatever the layout says */
    if (p->l.w > p->w || p->l.h > p->h) { note("layout %dx%d does not fit %dx%d", p->l.w, p->l.h, p->w, p->h); return; }
    ln_draw_cached(p->pic, &p->l, p->style, &v, &p->f, &p->cache);
}

static void show(struct panel *p)
{
    paint(p);
    if (p->win) zk_present_put(p->win, p->pic, p->w, p->h, (!p->composed && p->captured) ? p->under : NULL);
}

static void close_panel(const char *why)
{
    struct panel *p = &app.p;
    if (!p->win) return;
    CloseWindow(p->win);
    p->win = NULL;
    if (p->region) { DisposeRegion(p->region); p->region = NULL; }
    ln_draw_cache_reset(&p->cache);
    free(p->pic); free(p->under); free(p->glass);
    p->pic = p->under = p->glass = NULL;
    note("close %s", why);
}

/* The screen to open on: the one in front when it is public, else the
 * default public screen, brought to the front. Locked; unlock after use. */
static struct Screen *pick_screen(void)
{
    struct Screen *front, *scr = NULL;
    struct List *list;
    struct Node *node;
    char name[MAXPUBSCREENNAME + 1];
    ULONG lock = LockIBase(0);
    front = IntuitionBase->FirstScreen;
    UnlockIBase(lock);
    name[0] = 0;
    if ((list = LockPubScreenList())) {
        for (node = list->lh_Head; node->ln_Succ; node = node->ln_Succ)
            if (((struct PubScreenNode *)node)->psn_Screen == front && node->ln_Name) {
                strncpy(name, node->ln_Name, MAXPUBSCREENNAME);
                name[MAXPUBSCREENNAME] = 0;
            }
        UnlockPubScreenList();
    }
    if (name[0]) scr = LockPubScreen((CONST_STRPTR)name);
    if (!scr && (scr = LockPubScreen(NULL))) ScreenToFront(scr);
    return scr;
}

static int open_panel(const char *footer)
{
    struct panel *p = &app.p;
    struct Screen *scr;
    struct Window *helper = NULL;
    int compose, left, top, y;
    if (p->win) return 1;
    p->style = ln_style_get(app.cfg.dark, app.cfg.compact);
    if (!open_fonts(p)) { note("open failed: fonts"); return 0; }
    snapshot_windows();
    app.confirm = 0;
    app.confirm_pkg = -1;
    if (app.pkgs_stale) { app.pkgs_stale = 0; ln_scan_packages(app.scan); }
    if (!(scr = pick_screen())) { note("open failed: no screen"); return 0; }

    compose = zk_present_can_compose(scr);
    p->shadow = SHADOW;
    ln_layout_max(p->style, p->shadow, &p->w, &p->h);
    left = (scr->Width - p->w) / 2;
    top = scr->Height / 5 - p->shadow;
    /* the whole window, at its largest, stays on the screen: its picture
       of the screen can only be taken there */
    if (top + p->h > scr->Height) top = scr->Height - p->h;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    p->pic = malloc((size_t)p->w * (size_t)p->h * 4);
    p->under = malloc((size_t)p->w * (size_t)p->h * 4);
    p->glass = NULL;
    if (!compose && p->pic && p->under) {
        /* Without composition the panel is laid over a picture of the
           screen. The window that is active now will be drawn inactive as
           soon as the panel takes the focus, so its picture must be taken
           after that: a one-pixel window takes the focus first, and the
           picture is taken once the screen has stopped changing. */
        struct Window *first = OpenWindowTags(NULL, WA_PubScreen, (IPTR)scr, WA_Left, left + p->w / 2, WA_Top, top + p->shadow + 4,
                                              WA_Width, 1, WA_Height, 1, WA_Borderless, TRUE, WA_Activate, TRUE,
                                              WA_RMBTrap, TRUE, TAG_DONE);
        int tries, ok = zk_glass_capture(scr, left, top, p->w, p->h, p->pic);
        for (tries = 0; first && ok && tries < 8; tries++) {
            Delay(1);
            if (!zk_glass_capture(scr, left, top, p->w, p->h, p->under)) { ok = 0; break; }
            if (tries >= 1 && !memcmp(p->pic, p->under, (size_t)p->w * (size_t)p->h * 4)) break;
            memcpy(p->pic, p->under, (size_t)p->w * (size_t)p->h * 4);
        }
        helper = first;
        note("settle tries=%d", tries);
    }
    p->captured = p->pic && p->under && zk_glass_capture(scr, left, top, p->w, p->h, p->under);
    p->pinned = 0;
    if (!p->pic || !p->under) goto fail;
    if (!compose && !p->captured) {
        /* neither composition nor a picture of the screen: a window cut
           to the panel's outline, at its largest height, without a shadow */
        left += p->shadow; top += p->shadow;
        p->shadow = 0;
        p->pinned = 1;
        ln_layout_max(p->style, 0, &p->w, &p->h);
    }
    if (p->captured && app.cfg.glass) {
        int pw = p->style->width, ph = p->h - 2 * p->shadow;
        if ((p->glass = malloc((size_t)pw * (size_t)ph * 4))) {
            for (y = 0; y < ph; y++)
                memcpy(p->glass + (size_t)y * (size_t)pw * 4,
                       p->under + ((size_t)(y + p->shadow) * (size_t)p->w + (size_t)p->shadow) * 4, (size_t)pw * 4);
            zk_glass_blur(p->glass, pw, ph, BLUR);
        }
    }

    if (!p->doc && !(p->doc = ze_new())) goto fail;
    if (!p->res && !(p->res = ln_results_new())) goto fail;
    ze_set_text(p->doc, "", 0);
    ln_results_query(p->res);
    p->first = 0;
    p->caret_on = 1; p->ticks = 0; p->click_row = -1;
    snprintf(p->footer, sizeof p->footer, "%s", footer ? footer : "");
    answer();
    paint(p);
    if (p->pinned) p->region = zk_present_region(p->pic, p->w, p->h);
    {
        struct TagItem tags[] = {
            { WA_PubScreen, (IPTR)scr }, { WA_Left, left }, { WA_Top, top },
            { WA_InnerWidth, p->w }, { WA_InnerHeight, p->h },
            { WA_Borderless, TRUE }, { WA_Activate, TRUE }, { WA_RMBTrap, TRUE },
            { WA_IDCMP, IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_INTUITICKS },
            { p->region ? WA_ShapeRegion : TAG_IGNORE, (IPTR)p->region },
            { TAG_DONE, 0 }
        };
        p->win = zk_present_open(compose, tags);
    }
    if (!p->win) goto fail;
    if (helper) { CloseWindow(helper); helper = NULL; }
    p->composed = zk_present_composed(p->win);
    UnlockPubScreen(NULL, scr);
    zk_present_put(p->win, p->pic, p->w, p->h, (!p->composed && p->captured) ? p->under : NULL);
    note("open composed=%d captured=%d glass=%d size=%dx%d rows=%d programs=%d",
         p->composed, p->captured, p->glass != NULL, p->w, p->h, ln_results_count(p->res), ln_index_count(app.index));
    return 1;
fail:
    if (helper) CloseWindow(helper);
    UnlockPubScreen(NULL, scr);
    if (p->region) { DisposeRegion(p->region); p->region = NULL; }
    free(p->pic); free(p->under); free(p->glass);
    p->pic = p->under = p->glass = NULL;
    note("open failed");
    return 0;
}

static void query_changed(void)
{
    struct panel *p = &app.p;
    size_t n;
    const char *q = ze_text(p->doc, &n);
    ln_results_query(p->res);
    p->first = 0;
    p->footer[0] = 0;
    answer();
    note("query '%s' rows=%d", q, ln_results_count(p->res));
}

/* Starts the selected program the Workbench way. */
static void run_selected(void)
{
    struct panel *p = &app.p;
    const struct ln_row *row = ln_results_row(p->res, ln_results_selected(p->res));
    const struct ln_entry *e;
    char *path, *title;
    APTR was;
    BPTR lock;
    int ok = 0;
    if (!row) return;
    if (row->source == LN_SRC_CALC) {
        ok = ln_clip_write(app.calc, strlen(app.calc));
        note("calc '%s' copied=%d", app.calc, ok);
        close_panel("copied");
        return;
    }
    if (row->source == LN_SRC_PACKAGES) {
        int i = (int)row->key;
        char cmd[200], title[160];
        BPTR con;
        LONG rc = -1;
        if (i < 0 || i >= app.pkgs.n || !app.apkg[0]) return;
        if (app.confirm_pkg != i) {
            app.confirm_pkg = i;
            snprintf(p->footer, sizeof p->footer, "Return again to install %s", app.pkgs.v[i].id);
            show(p);
            return;
        }
        /* in a console of its own, so that the download and its outcome are seen */
        snprintf(cmd, sizeof cmd, "%s install %s --yes", app.apkg, app.pkgs.v[i].id);
        snprintf(title, sizeof title, "CON:40/60/640/240/Installing %s/AUTO/CLOSE/WAIT", app.pkgs.v[i].id);
        if ((con = Open((CONST_STRPTR)title, MODE_OLDFILE))) {
            rc = SystemTags((CONST_STRPTR)cmd, SYS_Asynch, TRUE, SYS_Input, (IPTR)con, SYS_Output, (IPTR)NULL, TAG_DONE);
            if (rc == -1) Close(con);
        }
        note("install '%s' started=%d", app.pkgs.v[i].id, rc != -1);
        app.pkgs_stale = 1;
        close_panel("install");
        return;
    }
    if (row->source == LN_SRC_SHELL) {
        size_t k = 0;
        char *cmd = zt_local_copy(row->title, strlen(row->title), &k);
        BPTR con = cmd ? Open((CONST_STRPTR)"CON:40/60/640/240/Launcher/AUTO/CLOSE/WAIT", MODE_OLDFILE) : 0;
        LONG rc = -1;
        if (con) {
            /* started and left to run: the console is the command's and closes with it */
            rc = SystemTags((CONST_STRPTR)cmd, SYS_Asynch, TRUE, SYS_Input, (IPTR)con, SYS_Output, (IPTR)NULL, TAG_DONE);
            if (rc == -1) Close(con);
        }
        note("shell '%s' started=%d", row->title, rc != -1);
        if (cmd) zt_free(cmd);
        close_panel("shell");
        return;
    }
    if (row->source == LN_SRC_PROGRAMS && row->key >= CMD_KEY) {
        unsigned id = row->key & 0xffff;
        if (id == CMD_RESTART || id == CMD_SHUTDOWN) {
            if (app.confirm != id) {
                app.confirm = id;
                snprintf(p->footer, sizeof p->footer, "Return again to %s the computer",
                         id == CMD_RESTART ? "restart" : "switch off");
                show(p);
                return;
            }
            note("command %u confirmed", id);
            close_panel("command");
            ShutdownA(id == CMD_RESTART ? SD_ACTION_COLDREBOOT : SD_ACTION_POWEROFF);
            return;
        }
        note("command %u", id);
        close_panel("command");
        if (id == CMD_SETTINGS) app.want_prefs = 1;
        else if (id == CMD_RESCAN) ln_scan_again(app.scan);
        else if (id == CMD_QUIT) app.quit = 1;
        return;
    }
    if (row->source == LN_SRC_PROGRAMS && row->key >= WIN_KEY) {
        int shown = show_window((int)(row->key & 0xffff));
        note("window '%s' shown=%d", row->title, shown);
        close_panel("window");
        if (!shown) open_panel("That window is no longer open");
        return;
    }
    if (!(e = ln_index_find(app.index, row->key))) return;
    path = strdup(e->path);
    title = strdup(e->title);
    if (!path || !title) { free(path); free(title); return; }
    ln_index_used(app.index, e->id, now());
    save_counts();
    close_panel("run");
    was = quiet();
    if ((lock = Lock((CONST_STRPTR)path, SHARED_LOCK))) {
        UnLock(lock);
        ok = OpenWorkbenchObjectA((STRPTR)path, NULL) != 0;
    }
    loud(was);
    note("run '%s' ok=%d", path, ok);
    if (!ok) {
        char msg[160];
        ln_index_remove(app.index, path);
        snprintf(msg, sizeof msg, "Could not start %s", title);
        open_panel(msg);
    }
    free(path); free(title);
}

static void panel_key(const struct IntuiMessage *im)
{
    struct panel *p = &app.p;
    enum ln_key key;
    unsigned mods;
    char utf8[64];
    size_t n;
    struct ln_key_out o;
    if (!ln_keys_translate(im, &key, &mods, utf8, sizeof utf8, &n)) return;
    p->caret_on = 1; p->ticks = 0;
    if ((app.confirm || app.confirm_pkg >= 0) && key != LN_K_RETURN) { app.confirm = 0; app.confirm_pkg = -1; p->footer[0] = 0; }
    switch (ln_queryline_key(p->doc, key, mods, utf8, n, &o)) {
    case LN_ACT_NONE: return;
    case LN_ACT_CARET: break;
    case LN_ACT_QUERY_CHANGED: query_changed(); break;
    case LN_ACT_MOVE: ln_results_move(p->res, o.delta); break;
    case LN_ACT_RUN: run_selected(); return;
    case LN_ACT_CLOSE: close_panel("esc"); return;
    case LN_ACT_ACTIONS: return;                    /* actions come with the next stage */
    case LN_ACT_COPY: ln_clip_write(o.out, o.outn); return;
    case LN_ACT_CUT: ln_clip_write(o.out, o.outn); query_changed(); break;
    case LN_ACT_WANT_PASTE: {
        char *text = ln_clip_read(&n);
        enum ln_act a = text ? ln_queryline_paste(p->doc, text, n) : LN_ACT_NONE;
        free(text);
        if (a != LN_ACT_QUERY_CHANGED) return;
        query_changed();
        break;
    }
    }
    p->dirty = 1;
}

static void panel_events(void)
{
    struct panel *p = &app.p;
    struct IntuiMessage *im;
    while (p->win && (im = (struct IntuiMessage *)GetMsg(p->win->UserPort))) {
        struct IntuiMessage m = *im;
        ULONG prev = 0;
        /* the dead-key bytes live in the message: copy them before the reply */
        if (m.Class == IDCMP_RAWKEY && m.IAddress) { prev = *(ULONG *)m.IAddress; m.IAddress = &prev; }
        ReplyMsg(&im->ExecMessage);
        switch (m.Class) {
        case IDCMP_RAWKEY: panel_key(&m); break;
        case IDCMP_INACTIVEWINDOW: close_panel("inactive"); break;
        case IDCMP_INTUITICKS:
            if (++p->ticks >= 5) { p->ticks = 0; p->caret_on = !p->caret_on; p->dirty = 1; }
            break;
        case IDCMP_MOUSEBUTTONS:
            if (m.Code == SELECTDOWN) {
                int row = ln_layout_row_at(&p->l, p->style, m.MouseX, m.MouseY);
                if (row >= 0) {
                    int twice = row == p->click_row && DoubleClick(p->click_secs, p->click_micros, m.Seconds, m.Micros);
                    p->click_row = row; p->click_secs = m.Seconds; p->click_micros = m.Micros;
                    ln_results_select(p->res, row);
                    if (twice) run_selected(); else p->dirty = 1;
                } else if (!ln_layout_inside(&p->l, p->style, m.MouseX, m.MouseY)) {
                    close_panel("click outside");
                }
            }
            break;
        }
    }
    /* one drawing for everything that was waiting: held keys do not queue up drawings */
    if (p->win && p->dirty) show(p);
    p->dirty = 0;
}

/* --- the scanner's messages ---------------------------------------------- */

static void scan_events(void)
{
    struct ln_scan_msg *m;
    int puts = 0;
    while ((m = (struct ln_scan_msg *)GetMsg(app.scanport))) {
        switch (m->kind) {
        case LN_SCAN_BEGIN: ln_index_begin(app.index, m->path); break;
        case LN_SCAN_END:
            ln_index_end(app.index, m->path);
            app.index_changed = 1;
            note("scanned '%s' programs=%d", m->path, ln_index_count(app.index));
            break;
        case LN_SCAN_PACKAGES: {
            size_t n;
            char *text = read_file(LN_PACKAGES_FILE, &n);
            struct ln_pkglist fresh;
            memset(&fresh, 0, sizeof fresh);
            if (text && ln_pkglist_parse(&fresh, text, n)) {
                ln_pkglist_free(&app.pkgs);
                app.pkgs = fresh;
                snprintf(app.apkg, sizeof app.apkg, "%s", m->path);
                app.index_changed = 1;
            }
            note("packages %d (apkg '%s')", app.pkgs.n, app.apkg);
            free(text);
            DeleteFile((CONST_STRPTR)LN_PACKAGES_FILE);
            break;
        }
        case LN_SCAN_PUT: {
            char *title = zt_path_from_dos(m->title);
            if (title) { ln_index_put(app.index, m->path, title); zt_free(title); puts++; }
            break;
        }
        }
        ln_scan_free_msg(m);
    }
    if (puts) app.index_changed = 1;
    if (app.index_changed && app.p.win) {
        app.index_changed = 0;
        answer();           /* same query number: the selection stays on its row */
        show(&app.p);
    }
}

/* --- settings ------------------------------------------------------------ */

static void open_settings(void)
{
    char **before;
    int n = app.cfg.folders, i, same;
    close_panel("settings");
    if (!(before = calloc((size_t)n + 1, sizeof *before))) return;
    for (i = 0; i < n; i++) before[i] = strdup(app.cfg.folder[i]);
    app.cfg.autostart = ln_autostart_on();
    if (ln_prefs_run(&app.cfg)) {
        save_config();
        if (app.cfg.autostart != ln_autostart_on()) {
            int ok = ln_autostart_set(app.cfg.autostart, app.program);
            note("autostart %d ok=%d", app.cfg.autostart, ok);
        }
        same = n == app.cfg.folders;
        for (i = 0; same && i < n; i++) same = before[i] && !strcmp(before[i], app.cfg.folder[i]);
        if (!same) {
            /* other folders: a new scanner; programs of folders no longer
               searched leave the index */
            if (app.scan) ln_scan_stop(app.scan);
            for (i = 0; i < n; i++)
                if (before[i]) { ln_index_begin(app.index, before[i]); ln_index_end(app.index, before[i]); }
            app.scan = ln_scan_start(app.scanport, app.cfg.folder, app.cfg.folders);
        }
        note("settings saved folders=%d dark=%d compact=%d glass=%d", app.cfg.folders, app.cfg.dark, app.cfg.compact, app.cfg.glass);
    }
    for (i = 0; i < n; i++) free(before[i]);
    free(before);
}

/* --- the commodity ------------------------------------------------------- */

static int cx_events(void)
{
    CxMsg *m;
    int running = 1;
    while ((m = (CxMsg *)GetMsg(app.cxport))) {
        ULONG type = CxMsgType(m), id = CxMsgID(m);
        ReplyMsg((struct Message *)m);
        if (type == CXM_IEVENT && id == EVT_HOTKEY) {
            note("hotkey");
            if (app.p.win) close_panel("hotkey"); else open_panel(NULL);
        } else if (type == CXM_COMMAND) {
            switch (id) {
            case CXCMD_KILL: running = 0; break;
            case CXCMD_DISABLE: ActivateCxObj(app.broker, FALSE); break;
            case CXCMD_ENABLE: ActivateCxObj(app.broker, TRUE); break;
            case CXCMD_APPEAR:
            case CXCMD_UNIQUE: open_panel(NULL); break;
            case CXCMD_DISAPPEAR: close_panel("exchange"); break;
            }
        }
    }
    return running;
}

int main(int argc, char **argv)
{
    /* started from Workbench: its message has the icon the ToolTypes are in */
    struct WBStartup *wb = WBenchMsg;
    UBYTE **args = wb ? ArgArrayInit(0, (UBYTE **)wb) : ArgArrayInit(argc, (UBYTE **)argv);
    const char *key = (const char *)ArgString(args, (UBYTE *)"CX_POPKEY", (UBYTE *)"ctrl alt space");
    const char *popup = (const char *)ArgString(args, (UBYTE *)"CX_POPUP", (UBYTE *)"NO");
    const char *logname = (const char *)ArgString(args, (UBYTE *)"LOG", (UBYTE *)"");
    const char *settings = (const char *)ArgString(args, (UBYTE *)"SETTINGS", (UBYTE *)"ENVARC:Launcher");
    LONG pri = ArgInt(args, (UBYTE *)"CX_PRIORITY", 0);
    struct NewBroker nb;
    CxObj *filter;
    LONG err = 0;
    int rc = RETURN_FAIL, running = 1, text = 0;

    snprintf(app.logname, sizeof app.logname, "%s", logname);
    if (app.logname[0]) DeleteFile((CONST_STRPTR)app.logname);
    snprintf(app.dir, sizeof app.dir, "%s", settings);
    snprintf(app.config, sizeof app.config, "%s/config", app.dir);
    snprintf(app.counts, sizeof app.counts, "%s/counts", app.dir);
    note("start key='%s' settings='%s'", key, app.dir);
    if (zt_layer_open() != ZT_OK) { PutStr((CONST_STRPTR)"Launcher: the text layer could not start\n"); goto out; }
    text = 1;
    if (!(app.index = ln_index_new()) || !(app.icons = ln_icons_new())) goto out;
    if (!(app.cxport = CreateMsgPort()) || !(app.scanport = CreateMsgPort())) goto out;
    load_settings();
    if (!ln_program_path(wb, app.program, sizeof app.program)) app.program[0] = 0;
    note("program '%s' workbench=%d", app.program, wb != NULL);

    memset(&nb, 0, sizeof nb);
    nb.nb_Version = NB_VERSION;
    nb.nb_Name = (STRPTR)"Launcher";
    nb.nb_Title = (STRPTR)"Launcher";
    nb.nb_Descr = (STRPTR)"Finds and starts programs";
    nb.nb_Unique = NBU_UNIQUE | NBU_NOTIFY;
    nb.nb_Flags = COF_SHOW_HIDE;
    nb.nb_Pri = (BYTE)pri;
    nb.nb_Port = app.cxport;
    if (!(app.broker = CxBroker(&nb, &err))) {
        /* already running: the first one was told to show its panel */
        note("broker refused: %ld", (long)err);
        rc = err == CBERR_DUP ? RETURN_OK : RETURN_FAIL;
        goto out;
    }
    filter = CxFilter((STRPTR)key);
    AttachCxObj(app.broker, filter);
    AttachCxObj(filter, CxSender(app.cxport, EVT_HOTKEY));
    AttachCxObj(filter, CxTranslate(NULL));
    if (CxObjError(filter)) {
        Printf((CONST_STRPTR)"Launcher: \"%s\" is not a hotkey\n", (IPTR)key);
        note("bad hotkey");
        goto out;
    }
    ActivateCxObj(app.broker, TRUE);

    /* Started from its icon for the first time, with its settings in the
       usual place: from now on it starts with the system. The switch is in
       the settings. */
    if (app.cfg.autostart < 0 && wb && app.program[0] && !strcmp(app.dir, "ENVARC:Launcher")) {
        app.cfg.autostart = ln_autostart_on() || ln_autostart_set(1, app.program);
        save_config();
        note("first start: autostart=%d", app.cfg.autostart);
    }
    app.scan = ln_scan_start(app.scanport, app.cfg.folder, app.cfg.folders);
    note("ready folders=%d scanner=%d", app.cfg.folders, app.scan != NULL);
    if ((popup[0] | 32) == 'y') open_panel(NULL);

    while (running) {
        ULONG winsig = app.p.win ? 1UL << app.p.win->UserPort->mp_SigBit : 0;
        ULONG mask = 1UL << app.cxport->mp_SigBit | 1UL << app.scanport->mp_SigBit | winsig | SIGBREAKF_CTRL_C, got;
        /* icons are read while nothing else asks for attention */
        if (app.p.win && ln_icons_wanted(app.icons)) {
            APTR was = quiet();
            int read = 0;
            while (!(SetSignal(0, 0) & mask) && ln_icons_work(app.icons, icon_path, NULL)) read++;
            loud(was);
            if (read) show(&app.p);
            if (!(SetSignal(0, 0) & mask) && !ln_icons_wanted(app.icons)) got = Wait(mask);
            else got = SetSignal(0, mask) & mask;
        } else {
            got = Wait(mask);
        }
        if (got & SIGBREAKF_CTRL_C) running = 0;
        panel_events();
        scan_events();
        if (!cx_events() || app.quit) running = 0;
        if (app.want_prefs && running) { app.want_prefs = 0; open_settings(); }
    }
    rc = RETURN_OK;
out:
    close_panel("quit");
    if (app.broker) DeleteCxObjAll(app.broker);
    if (app.cxport) {
        struct Message *m;
        while ((m = GetMsg(app.cxport))) ReplyMsg(m);
        DeleteMsgPort(app.cxport);
    }
    if (app.scan) ln_scan_stop(app.scan);
    if (app.scanport) {
        struct ln_scan_msg *m;
        while ((m = (struct ln_scan_msg *)GetMsg(app.scanport))) ln_scan_free_msg(m);
        DeleteMsgPort(app.scanport);
    }
    close_fonts(&app.p);
    if (app.p.doc) ze_free(app.p.doc);
    if (app.p.res) ln_results_free(app.p.res);
    ln_pkglist_free(&app.pkgs);
    if (app.icons) ln_icons_free(app.icons);
    if (app.index) ln_index_free(app.index);
    ln_config_free(&app.cfg);
    if (text) zt_layer_close();
    note("end");
    ArgArrayDone();
    return rc;
}
