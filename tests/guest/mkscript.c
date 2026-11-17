/* Test helper: writes a script with an IconX project icon, as programs
 * that are started through a script have.
 *   mkscript <script path> <file the script creates when it runs> */
#include <string.h>

#include <dos/dos.h>
#include <workbench/workbench.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>

int main(int argc, char **argv)
{
    static STRPTR none[] = { NULL };
    struct DiskObject *icon;
    BPTR f;
    int ok = 0;
    if (argc != 3) { PutStr((CONST_STRPTR)"mkscript <script> <marker>\n"); return RETURN_FAIL; }
    if (!(f = Open((CONST_STRPTR)argv[1], MODE_NEWFILE))) return RETURN_FAIL;
    FPrintf(f, (CONST_STRPTR)"Echo \"ran\" >%s\n", (IPTR)argv[2]);
    Close(f);
    SetProtection((CONST_STRPTR)argv[1], FIBF_SCRIPT);
    if ((icon = GetDefDiskObject(WBPROJECT))) {
        STRPTR tool = icon->do_DefaultTool;
        STRPTR *tt = icon->do_ToolTypes;
        icon->do_DefaultTool = (STRPTR)"C:IconX";
        icon->do_ToolTypes = none;
        ok = PutDiskObject((CONST_STRPTR)argv[1], icon) != 0;
        icon->do_DefaultTool = tool;
        icon->do_ToolTypes = tt;
        FreeDiskObject(icon);
    }
    Printf((CONST_STRPTR)"mkscript: %s\n", (IPTR)(ok ? "ok" : "failed"));
    return ok ? RETURN_OK : RETURN_FAIL;
}
