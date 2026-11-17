/* SPDX-License-Identifier: MIT */
/* The scanner: a process of its own that walks the chosen folders, finds
 * programs (files with a tool icon) and tells the main task by message.
 * It scans every folder once at start and a folder again when DOS says it
 * changed. It never puts up a requester. */
#ifndef LN_SCAN_AROS_H
#define LN_SCAN_AROS_H

#include <exec/ports.h>

enum { LN_SCAN_BEGIN = 1, LN_SCAN_PUT, LN_SCAN_END, LN_SCAN_EXIT };

/* Sent to the main task's port; the receiver frees it with
 * ln_scan_free_msg. path and title are in the system's character set.
 * BEGIN and END carry the folder in path. */
struct ln_scan_msg {
    struct Message m;
    LONG kind;
    char *path;
    char *title;
};

struct ln_scan;

/* Starts the scanner for these folders (copied). NULL when it could not
 * be started. */
struct ln_scan *ln_scan_start(struct MsgPort *to, char *const *folders, int n);
/* Asks for every folder to be scanned again. */
void ln_scan_again(struct ln_scan *s);
/* Ends the scanner and waits until it is gone; messages still arriving at
 * the port are freed. */
void ln_scan_stop(struct ln_scan *s);
void ln_scan_free_msg(struct ln_scan_msg *m);

#endif
