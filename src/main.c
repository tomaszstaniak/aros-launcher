/* SPDX-License-Identifier: MIT */
/* The launcher's program. For now it only proves the build: it opens
 * commodities.library and uses the shared text layer. */
#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "zk_text.h"

static const char version[] __attribute__((used)) = "$VER: Launcher 0.1 (17.11.2026)";

int main(void)
{
    struct Library *cx = OpenLibrary("commodities.library", 37);
    if (!cx) {
        PutStr("Launcher: no commodities.library\n");
        return RETURN_FAIL;
    }
    Printf("%s\n", (IPTR)(version + 6));
    Printf("text layer: %s\n", (IPTR)(zt_valid("\xc5\xbc", 2) ? "ok" : "broken"));
    CloseLibrary(cx);
    return RETURN_OK;
}
