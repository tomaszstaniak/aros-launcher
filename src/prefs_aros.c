/* SPDX-License-Identifier: MIT */
#include "prefs_aros.h"

#include <stdio.h>
#include <string.h>

#include <dos/dos.h>
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdlib.h>

/* Opened only while the window is shown: the program runs all the time
 * and has no use for Zune otherwise. */
struct Library *MUIMasterBase;

#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d) ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))
#endif

enum { ID_SAVE = 1, ID_CANCEL, ID_ADD_PROGRAMS, ID_REMOVE_PROGRAMS, ID_ADD_FILES, ID_REMOVE_FILES,
       ID_ADD_EXCLUDE_DRAWER, ID_ADD_EXCLUDE_PATTERN, ID_REMOVE_EXCLUDE };

static IPTR xget(Object *o, ULONG attr)
{
    IPTR v = 0;
    get(o, attr, &v);
    return v;
}

static Object *checkmark(int on)
{
    Object *o = MUI_MakeObject(MUIO_Checkmark, (IPTR)NULL);
    if (o) { set(o, MUIA_Selected, on); set(o, MUIA_CycleChain, 1); }
    return o;
}

static Object *button(const char *label)
{
    Object *o = SimpleButton(label);
    if (o) set(o, MUIA_CycleChain, 1);
    return o;
}

static Object *list(void)
{
    return ListObject, InputListFrame,
        MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
        MUIA_List_DestructHook, MUIV_List_DestructHook_String,
    End;
}

/* A drawer chosen in a requester goes to the end of a list. */
static void add_drawer(Object *app, Object *win, Object *to, const char *title)
{
    struct FileRequester *req;
    set(app, MUIA_Application_Sleep, TRUE);
    if ((req = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_Window, (IPTR)xget(win, MUIA_Window_Window),
                                       ASLFR_TitleText, (IPTR)title, ASLFR_DrawersOnly, TRUE,
                                       ASLFR_InitialDrawer, (IPTR)"SYS:", TAG_DONE))) {
        if (MUI_AslRequestTags(req, TAG_DONE) && req->fr_Drawer && *req->fr_Drawer)
            DoMethod(to, MUIM_List_InsertSingle, (IPTR)req->fr_Drawer, MUIV_List_Insert_Bottom);
        MUI_FreeAslRequest(req);
    }
    set(app, MUIA_Application_Sleep, FALSE);
}

static void fill(Object *to, char *const *item, int n)
{
    int i;
    for (i = 0; i < n; i++) DoMethod(to, MUIM_List_InsertSingle, (IPTR)item[i], MUIV_List_Insert_Bottom);
}

int ln_prefs_run(struct ln_config *c, const struct ln_about *about, int page)
{
    static const char *themes[] = { "Dark", "Light", NULL };
    static const char *pages[] = { "Programs", "Files", "Look", "General", "About", NULL };
    static char facts[400], title[80], known[200], values[300];
    Object *app, *win, *reg, *programs, *files, *exclude, *pattern, *theme, *colours, *compact, *glass, *start;
    Object *add_p, *rem_p, *add_f, *rem_f, *add_xd, *add_xp, *rem_x, *save, *cancel;
    int saved = 0, running = 1, i;
    ULONG sigs = 0;

    snprintf(title, sizeof title, "\33c\33b%s", about->version);
    snprintf(facts, sizeof facts,
             "Hotkey\t%s\nPrograms\t%d\nFiles\t%d\nPackages\t%d\nExchange rates\t%s",
             about->hotkey, about->programs, about->files, about->packages,
             about->rates_date[0] ? about->rates_date : "not fetched yet");
    for (i = 0, known[0] = values[0] = 0; facts[i]; ) {
        /* two columns: the names and the values */
        char *tab = strchr(facts + i, '\t'), *end = strchr(facts + i, '\n');
        size_t line = end ? (size_t)(end - (facts + i)) : strlen(facts + i);
        if (!tab) break;
        snprintf(known + strlen(known), sizeof known - strlen(known), "%s%.*s", known[0] ? "\n" : "\33r", (int)(tab - (facts + i)), facts + i);
        snprintf(values + strlen(values), sizeof values - strlen(values), "%s%.*s", values[0] ? "\n" : "\33b", (int)(facts + i + line - tab - 1), tab + 1);
        i += (int)line + (end ? 1 : 0);
    }

    if (!(MUIMasterBase = OpenLibrary((CONST_STRPTR)"muimaster.library", 19))) return 0;
    app = ApplicationObject,
        MUIA_Application_Title, (IPTR)"Jumpstart Settings",
        MUIA_Application_Base, (IPTR)"JUMPSTARTSETTINGS",
        MUIA_Application_UseCommodities, FALSE,
        MUIA_Application_UseRexx, FALSE,
        SubWindow, (IPTR)(win = WindowObject,
            MUIA_Window_Title, (IPTR)"Jumpstart Settings",
            MUIA_Window_ID, MAKE_ID('L', 'N', 'S', '2'),
            MUIA_Window_Width, MUIV_Window_Width_Visible(50),
            WindowContents, (IPTR)(VGroup,
                Child, (IPTR)(reg = RegisterGroup(pages),
                    MUIA_CycleChain, 1,
                    /* Programs */
                    Child, (IPTR)(VGroup,
                        Child, (IPTR)(TextObject, MUIA_Text_Contents, (IPTR)"Programs are looked for in these folders and in the drawers directly inside them.", End),
                        Child, (IPTR)(ListviewObject, MUIA_Listview_List, (IPTR)(programs = list()), MUIA_CycleChain, 1, End),
                        Child, (IPTR)(HGroup, Child, (IPTR)(add_p = button("_Add...")), Child, (IPTR)(rem_p = button("_Remove")), End),
                    End),
                    /* Files */
                    Child, (IPTR)(VGroup,
                        Child, (IPTR)(TextObject, MUIA_Text_Contents, (IPTR)"Files and drawers are found by name in these folders and everything below them.", End),
                        Child, (IPTR)(ListviewObject, MUIA_Listview_List, (IPTR)(files = list()), MUIA_CycleChain, 1, End),
                        Child, (IPTR)(HGroup, Child, (IPTR)(add_f = button("A_dd...")), Child, (IPTR)(rem_f = button("R_emove")), End),
                        Child, (IPTR)(TextObject, MUIA_Text_Contents, (IPTR)"Left out: folders with all below them, and names matching a pattern (for example #?.bak).", End),
                        Child, (IPTR)(ListviewObject, MUIA_Listview_List, (IPTR)(exclude = list()), MUIA_CycleChain, 1, End),
                        Child, (IPTR)(HGroup,
                            Child, (IPTR)(add_xd = button("Add _folder...")),
                            Child, (IPTR)(rem_x = button("Re_move")),
                        End),
                        Child, (IPTR)(HGroup,
                            Child, (IPTR)Label2("Pattern"),
                            Child, (IPTR)(pattern = StringObject, StringFrame, MUIA_String_MaxLen, 80, MUIA_CycleChain, 1, End),
                            Child, (IPTR)(add_xp = button("Add _pattern")),
                        End),
                    End),
                    /* Look */
                    Child, (IPTR)(VGroup,
                        Child, (IPTR)(ColGroup(2),
                            Child, (IPTR)Label1("Theme"),
                            Child, (IPTR)(theme = CycleObject, MUIA_Cycle_Entries, (IPTR)themes, MUIA_CycleChain, 1, End),
                            Child, (IPTR)Label1("Colours"),
                            Child, (IPTR)(colours = StringObject, StringFrame, MUIA_String_MaxLen, 200, MUIA_CycleChain, 1, End),
                            Child, (IPTR)Label1("Compact rows"),
                            Child, (IPTR)(HGroup, Child, (IPTR)(compact = checkmark(c->compact)), Child, (IPTR)HSpace(0), End),
                            Child, (IPTR)Label1("Frosted glass"),
                            Child, (IPTR)(HGroup, Child, (IPTR)(glass = checkmark(c->glass)), Child, (IPTR)HSpace(0), End),
                        End),
                        Child, (IPTR)(TextObject, MUIA_Text_Contents, (IPTR)
                            "Colours: the name of a theme in the Themes drawer (nord, dracula,\n"
                            "gruvbox-dark, solarized-light, catppuccin-latte, tokyo-night...) or a\n"
                            "theme file. Empty: the launcher's own colours. Dark or Light above picks\n"
                            "the variant; the one a theme lacks is made from it.", End),
                        Child, (IPTR)VSpace(0),
                    End),
                    /* General */
                    Child, (IPTR)(VGroup,
                        Child, (IPTR)(ColGroup(2),
                            Child, (IPTR)Label1("Start with the system"),
                            Child, (IPTR)(HGroup, Child, (IPTR)(start = checkmark(c->autostart > 0)), Child, (IPTR)HSpace(0), End),
                        End),
                        Child, (IPTR)(TextObject, MUIA_Text_Contents, (IPTR)"The hotkey is the CX_POPKEY ToolType of the Jumpstart icon.", End),
                        Child, (IPTR)VSpace(0),
                    End),
                    /* About */
                    Child, (IPTR)(VGroup,
                        Child, (IPTR)VSpace(4),
                        Child, (IPTR)(TextObject, MUIA_Text_Contents, (IPTR)title, MUIA_Font, MUIV_Font_Big, End),
                        Child, (IPTR)(TextObject, MUIA_Text_Contents, (IPTR)"\33cType a few letters, press Return.", End),
                        Child, (IPTR)VSpace(4),
                        Child, (IPTR)(HGroup, GroupFrameT("Now"),
                            Child, (IPTR)HSpace(0),
                            Child, (IPTR)(TextObject, MUIA_Text_Contents, (IPTR)known, End),
                            Child, (IPTR)(TextObject, MUIA_Text_Contents, (IPTR)values, End),
                            Child, (IPTR)HSpace(0),
                        End),
                        Child, (IPTR)(VGroup, GroupFrameT("Data and licences"),
                            Child, (IPTR)(TextObject, MUIA_Text_Contents, (IPTR)
                                "Exchange rates: European Central Bank, euro reference rates.\n"
                                "Weather: Open-Meteo (CC BY 4.0), through Dockbar's Weather widget.\n"
                                "The Currency widget contains curl and OpenSSL; their licences\n"
                                "are in Widgets/org.aros.currency/licenses.", End),
                        End),
                        Child, (IPTR)VSpace(0),
                        Child, (IPTR)(TextObject, MUIA_Text_Contents, (IPTR)"\33cMIT licence", End),
                    End),
                End),
                Child, (IPTR)(HGroup,
                    Child, (IPTR)(save = button("_Save")),
                    Child, (IPTR)HSpace(0),
                    Child, (IPTR)(cancel = button("_Cancel")),
                End),
            End),
        End),
    End;
    if (!app) { CloseLibrary(MUIMasterBase); MUIMasterBase = NULL; return 0; }

    fill(programs, c->folder, c->folders);
    fill(files, c->files, c->files_n);
    fill(exclude, c->exclude, c->exclude_n);
    set(theme, MUIA_Cycle_Active, c->dark ? 0 : 1);
    set(colours, MUIA_String_Contents, (IPTR)(c->colours ? c->colours : ""));
    if (page >= 0 && page <= 4) set(reg, MUIA_Group_ActivePage, page);
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_CANCEL);
    DoMethod(save, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_SAVE);
    DoMethod(cancel, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_CANCEL);
    DoMethod(add_p, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_ADD_PROGRAMS);
    DoMethod(rem_p, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_REMOVE_PROGRAMS);
    DoMethod(add_f, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_ADD_FILES);
    DoMethod(rem_f, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_REMOVE_FILES);
    DoMethod(add_xd, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_ADD_EXCLUDE_DRAWER);
    DoMethod(add_xp, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_ADD_EXCLUDE_PATTERN);
    DoMethod(pattern, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime, (IPTR)app, 2, MUIM_Application_ReturnID, ID_ADD_EXCLUDE_PATTERN);
    DoMethod(rem_x, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_REMOVE_EXCLUDE);
    set(win, MUIA_Window_Open, TRUE);
    if (!xget(win, MUIA_Window_Open)) running = 0;

    while (running) {
        switch (DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs)) {
        case ID_CANCEL: running = 0; break;
        case ID_ADD_PROGRAMS: add_drawer(app, win, programs, "A folder with programs"); break;
        case ID_REMOVE_PROGRAMS: DoMethod(programs, MUIM_List_Remove, MUIV_List_Remove_Active); break;
        case ID_ADD_FILES: add_drawer(app, win, files, "A folder to find files in"); break;
        case ID_REMOVE_FILES: DoMethod(files, MUIM_List_Remove, MUIV_List_Remove_Active); break;
        case ID_ADD_EXCLUDE_DRAWER: add_drawer(app, win, exclude, "A folder to leave out"); break;
        case ID_ADD_EXCLUDE_PATTERN: {
            const char *text = (const char *)xget(pattern, MUIA_String_Contents);
            if (text && *text) {
                DoMethod(exclude, MUIM_List_InsertSingle, (IPTR)text, MUIV_List_Insert_Bottom);
                set(pattern, MUIA_String_Contents, (IPTR)"");
            }
            break;
        }
        case ID_REMOVE_EXCLUDE: DoMethod(exclude, MUIM_List_Remove, MUIV_List_Remove_Active); break;
        case ID_SAVE: {
            LONG n;
            char *text;
            while (c->folders) ln_config_remove_folder(c, 0);
            while (c->files_n) ln_config_remove_files(c, 0);
            while (c->exclude_n) ln_config_remove_exclude(c, 0);
            for (n = (LONG)xget(programs, MUIA_List_Entries), i = 0; i < n; i++) {
                text = NULL; DoMethod(programs, MUIM_List_GetEntry, i, (IPTR)&text);
                if (text) ln_config_add_folder(c, text);
            }
            for (n = (LONG)xget(files, MUIA_List_Entries), i = 0; i < n; i++) {
                text = NULL; DoMethod(files, MUIM_List_GetEntry, i, (IPTR)&text);
                if (text) ln_config_add_files(c, text);
            }
            for (n = (LONG)xget(exclude, MUIA_List_Entries), i = 0; i < n; i++) {
                text = NULL; DoMethod(exclude, MUIM_List_GetEntry, i, (IPTR)&text);
                if (text) ln_config_add_exclude(c, text);
            }
            c->dark = xget(theme, MUIA_Cycle_Active) == 0;
            {
                const char *typed = (const char *)xget(colours, MUIA_String_Contents);
                char *copy = strdup(typed ? typed : "");
                if (copy) { free(c->colours); c->colours = copy; }
            }
            c->compact = xget(compact, MUIA_Selected) != 0;
            c->glass = xget(glass, MUIA_Selected) != 0;
            c->autostart = xget(start, MUIA_Selected) != 0;
            saved = 1;
            running = 0;
            break;
        }
        }
        if (running && sigs) {
            sigs = Wait(sigs | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C) { Signal(FindTask(NULL), SIGBREAKF_CTRL_C); running = 0; }
        }
    }
    set(win, MUIA_Window_Open, FALSE);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    MUIMasterBase = NULL;
    return saved;
}
