/* SPDX-License-Identifier: MIT */
/* rxsend PORT COMMAND: sends one ARexx command message to a public port,
 * the way a Rexx script's "address PORT; 'COMMAND'" does, and prints the
 * return code and the result string. A stand-in for a script on a system
 * without a Rexx interpreter. */
#include <exec/ports.h>
#include <rexx/errors.h>
#include <rexx/storage.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/rexxsyslib.h>
#include <stdio.h>
#include <string.h>

struct RxsLib *RexxSysBase;

int main(int argc, char **argv)
{
    struct MsgPort *reply, *port;
    struct RexxMsg *m;
    int rc = 20;
    if (argc != 3) { printf("usage: rxsend PORT COMMAND\n"); return 5; }
    if (!(RexxSysBase = (struct RxsLib *)OpenLibrary((CONST_STRPTR)"rexxsyslib.library", 0))) { printf("no rexxsyslib.library\n"); return 20; }
    if ((reply = CreateMsgPort())) {
        if ((m = CreateRexxMsg(reply, NULL, NULL))) {
            if ((m->rm_Args[0] = (IPTR)CreateArgstring((STRPTR)argv[2], (ULONG)strlen(argv[2])))) {
                m->rm_Action = RXCOMM | RXFF_RESULT;
                Forbid();
                if ((port = FindPort((CONST_STRPTR)argv[1]))) PutMsg(port, &m->rm_Node);
                Permit();
                if (port) {
                    WaitPort(reply);
                    GetMsg(reply);
                    printf("rc=%ld result=%s\n", (long)m->rm_Result1, m->rm_Result1 == 0 && m->rm_Result2 ? (char *)m->rm_Result2 : "(none)");
                    if (m->rm_Result1 == 0 && m->rm_Result2) DeleteArgstring((STRPTR)m->rm_Result2);
                    rc = (int)m->rm_Result1;
                } else printf("no port %s\n", argv[1]);
                DeleteArgstring((STRPTR)m->rm_Args[0]);
            }
            DeleteRexxMsg(m);
        }
        DeleteMsgPort(reply);
    }
    CloseLibrary((struct Library *)RexxSysBase);
    return rc;
}
