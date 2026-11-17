/* SPDX-License-Identifier: MIT */
#include "clipwatch_aros.h"

#include <aros/asmcall.h>
#include <devices/clipboard.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <utility/hooks.h>
#include <proto/exec.h>

static struct MsgPort *port;
static struct IOClipReq *req;
static struct Hook hook;
static struct Task *told;
static ULONG told_mask;
static int open;

/* Called by clipboard.device in the context of whoever wrote. */
AROS_UFH3(static IPTR, changed,
          AROS_UFHA(struct Hook *, h, A0),
          AROS_UFHA(APTR, object, A2),
          AROS_UFHA(struct ClipHookMsg *, msg, A1))
{
    AROS_USERFUNC_INIT
    (void)h; (void)object;
    if (msg && msg->chm_Type == 0 && told) Signal(told, told_mask);
    return 0;
    AROS_USERFUNC_EXIT
}

static LONG install(LONG on)
{
    req->io_Command = CBD_CHANGEHOOK;
    req->io_Data = (APTR)&hook;
    req->io_Length = (ULONG)on;
    return DoIO((struct IORequest *)req);
}

int ln_clipwatch_start(struct Task *task, ULONG mask)
{
    if (open) return 1;
    told = task;
    told_mask = mask;
    hook.h_Entry = (HOOKFUNC)changed;
    if (!(port = CreateMsgPort())) return 0;
    if ((req = (struct IOClipReq *)CreateIORequest(port, sizeof *req))) {
        if (!OpenDevice((CONST_STRPTR)"clipboard.device", 0, (struct IORequest *)req, 0)) {
            if (!install(1)) { open = 1; return 1; }
            CloseDevice((struct IORequest *)req);
        }
        DeleteIORequest((struct IORequest *)req);
        req = NULL;
    }
    DeleteMsgPort(port);
    port = NULL;
    return 0;
}

void ln_clipwatch_stop(void)
{
    if (!open) return;
    install(0);
    CloseDevice((struct IORequest *)req);
    DeleteIORequest((struct IORequest *)req);
    DeleteMsgPort(port);
    req = NULL; port = NULL; told = NULL; open = 0;
}
