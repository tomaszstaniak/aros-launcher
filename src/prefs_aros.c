/* SPDX-License-Identifier: MIT */
#include "prefs_aros.h"

#include <string.h>

#include <dos/dos.h>
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

/* Opened only while the window is shown: the program runs all the time
 * and has no use for Zune otherwise. */
struct Library *MUIMasterBase;

#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d) ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))
#endif

enum { ID_SAVE = 1, ID_CANCEL, ID_ADD, ID_REMOVE };

static IPTR xget(Object *o, ULONG attr)
{
    IPTR v = 0;
    get(o, attr, &v);
    return v;
}

static Object *checkmark(int on)
{
    Object *o = MUI_MakeObject(MUIO_Checkmark, (IPTR)NULL);
    if (o) set(o, MUIA_Selected, on);
    return o;
}

static void add_folder(Object *app, Object *win, Object *list)
{
    struct FileRequester *req;
    set(app, MUIA_Application_Sleep, TRUE);
    if ((req = MUI_AllocAslRequestTags(ASL_FileRequest, ASLFR_Window, (IPTR)xget(win, MUIA_Window_Window),
                                       ASLFR_TitleText, (IPTR)"Add a folder", ASLFR_DrawersOnly, TRUE,
                                       ASLFR_InitialDrawer, (IPTR)"SYS:", TAG_DONE))) {
        if (MUI_AslRequestTags(req, TAG_DONE) && req->fr_Drawer && *req->fr_Drawer)
            DoMethod(list, MUIM_List_InsertSingle, (IPTR)req->fr_Drawer, MUIV_List_Insert_Bottom);
        MUI_FreeAslRequest(req);
    }
    set(app, MUIA_Application_Sleep, FALSE);
}

int ln_prefs_run(struct ln_config *c)
{
    static const char *themes[] = { "Dark", "Light", NULL };
    Object *app, *win, *list, *add, *remove, *theme, *compact, *glass, *start, *save, *cancel;
    int saved = 0, running = 1, i;
    ULONG sigs = 0;

    if (!(MUIMasterBase = OpenLibrary((CONST_STRPTR)"muimaster.library", 19))) return 0;
    app = ApplicationObject,
        MUIA_Application_Title, (IPTR)"Launcher Settings",
        MUIA_Application_Base, (IPTR)"LAUNCHERSETTINGS",
        MUIA_Application_UseCommodities, FALSE,
        MUIA_Application_UseRexx, FALSE,
        SubWindow, (IPTR)(win = WindowObject,
            MUIA_Window_Title, (IPTR)"Launcher Settings",
            MUIA_Window_ID, MAKE_ID('L', 'N', 'S', 'T'),
            MUIA_Window_Width, MUIV_Window_Width_Visible(45),
            WindowContents, (IPTR)(VGroup,
                Child, (IPTR)(VGroup, GroupFrameT("Folders searched for programs"),
                    Child, (IPTR)(ListviewObject,
                        MUIA_Listview_List, (IPTR)(list = ListObject, InputListFrame,
                            MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
                            MUIA_List_DestructHook, MUIV_List_DestructHook_String,
                        End),
                    End),
                    Child, (IPTR)(HGroup,
                        Child, (IPTR)(add = SimpleButton("_Add...")),
                        Child, (IPTR)(remove = SimpleButton("_Remove")),
                    End),
                End),
                Child, (IPTR)(ColGroup(2), GroupFrameT("Look"),
                    Child, (IPTR)Label1("Theme"),
                    Child, (IPTR)(theme = CycleObject, MUIA_Cycle_Entries, (IPTR)themes, MUIA_CycleChain, 1, End),
                    Child, (IPTR)Label1("Compact rows"),
                    Child, (IPTR)(HGroup, Child, (IPTR)(compact = checkmark(c->compact)), Child, (IPTR)HSpace(0), End),
                    Child, (IPTR)Label1("Frosted glass"),
                    Child, (IPTR)(HGroup, Child, (IPTR)(glass = checkmark(c->glass)), Child, (IPTR)HSpace(0), End),
                End),
                Child, (IPTR)(ColGroup(2), GroupFrameT("Start"),
                    Child, (IPTR)Label1("Start with the system"),
                    Child, (IPTR)(HGroup, Child, (IPTR)(start = checkmark(c->autostart > 0)), Child, (IPTR)HSpace(0), End),
                End),
                Child, (IPTR)(HGroup,
                    Child, (IPTR)(save = SimpleButton("_Save")),
                    Child, (IPTR)HSpace(0),
                    Child, (IPTR)(cancel = SimpleButton("_Cancel")),
                End),
            End),
        End),
    End;
    if (!app) { CloseLibrary(MUIMasterBase); MUIMasterBase = NULL; return 0; }

    for (i = 0; i < c->folders; i++) DoMethod(list, MUIM_List_InsertSingle, (IPTR)c->folder[i], MUIV_List_Insert_Bottom);
    set(theme, MUIA_Cycle_Active, c->dark ? 0 : 1);
    set(compact, MUIA_CycleChain, 1); set(glass, MUIA_CycleChain, 1); set(start, MUIA_CycleChain, 1);
    set(add, MUIA_CycleChain, 1); set(remove, MUIA_CycleChain, 1); set(save, MUIA_CycleChain, 1); set(cancel, MUIA_CycleChain, 1);
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_CANCEL);
    DoMethod(save, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_SAVE);
    DoMethod(cancel, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_CANCEL);
    DoMethod(add, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_ADD);
    DoMethod(remove, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_REMOVE);
    set(win, MUIA_Window_Open, TRUE);
    if (!xget(win, MUIA_Window_Open)) running = 0;

    while (running) {
        switch (DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs)) {
        case ID_CANCEL: running = 0; break;
        case ID_ADD: add_folder(app, win, list); break;
        case ID_REMOVE: DoMethod(list, MUIM_List_Remove, MUIV_List_Remove_Active); break;
        case ID_SAVE: {
            LONG entries = (LONG)xget(list, MUIA_List_Entries);
            while (c->folders) ln_config_remove_folder(c, 0);
            for (i = 0; i < entries; i++) {
                char *path = NULL;
                DoMethod(list, MUIM_List_GetEntry, i, (IPTR)&path);
                if (path) ln_config_add_folder(c, path);
            }
            c->dark = xget(theme, MUIA_Cycle_Active) == 0;
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
