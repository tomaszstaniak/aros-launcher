/* SPDX-License-Identifier: MIT */
#include "autostart_aros.h"

#include <string.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>

#define START_ICON "SYS:WBStartup/Launcher"

static APTR quiet(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR was = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;
    return was;
}
static void loud(APTR was) { ((struct Process *)FindTask(NULL))->pr_WindowPtr = was; }

int ln_program_path(const struct WBStartup *wb, char *out, size_t cap)
{
    BPTR dir;
    const char *name;
    char progname[256];
    if (wb) {
        if (wb->sm_NumArgs < 1) return 0;
        dir = wb->sm_ArgList[0].wa_Lock;
        name = (const char *)wb->sm_ArgList[0].wa_Name;
    } else {
        if (!GetProgramName((STRPTR)progname, sizeof progname)) return 0;
        dir = GetProgramDir();
        name = (const char *)FilePart((CONST_STRPTR)progname);
    }
    if (!dir || !name || !*name) return 0;
    if (!NameFromLock(dir, (STRPTR)out, (LONG)cap)) return 0;
    return AddPart((STRPTR)out, (CONST_STRPTR)name, (ULONG)cap) != 0;
}

int ln_autostart_on(void)
{
    APTR was = quiet();
    BPTR lock = Lock((CONST_STRPTR)START_ICON ".info", SHARED_LOCK);
    loud(was);
    if (lock) UnLock(lock);
    return lock != 0;
}

int ln_autostart_set(int on, const char *program)
{
    static STRPTR types[] = { (STRPTR)"DONOTWAIT", NULL };
    struct DiskObject *icon;
    APTR was = quiet();
    int ok = 0;
    if (!on) {
        ok = !ln_autostart_on() || DeleteDiskObject((STRPTR)START_ICON);
    } else if (program && *program && (icon = GetDefDiskObject(WBPROJECT))) {
        STRPTR tool = icon->do_DefaultTool;
        STRPTR *tt = icon->do_ToolTypes;
        LONG x = icon->do_CurrentX, y = icon->do_CurrentY;
        icon->do_DefaultTool = (STRPTR)program;
        icon->do_ToolTypes = types;
        icon->do_CurrentX = icon->do_CurrentY = NO_ICON_POSITION;
        ok = PutDiskObject((CONST_STRPTR)START_ICON, icon) != 0;
        /* the object is the library's: give it back as it was */
        icon->do_DefaultTool = tool;
        icon->do_ToolTypes = tt;
        icon->do_CurrentX = x; icon->do_CurrentY = y;
        FreeDiskObject(icon);
    }
    loud(was);
    return ok;
}
