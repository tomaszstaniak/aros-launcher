/* SPDX-License-Identifier: MIT */
/* The scanner: a process of its own that walks the chosen folders, finds
 * programs (files with a tool icon, and scripts with an IconX project icon) and tells the main task by message.
 * It scans every folder once at start and a folder again when DOS says it
 * changed. It never puts up a requester. */
#ifndef LN_SCAN_AROS_H
#define LN_SCAN_AROS_H

#include <exec/ports.h>

enum { LN_SCAN_BEGIN = 1, LN_SCAN_PUT, LN_SCAN_END, LN_SCAN_EXIT, LN_SCAN_PACKAGES, LN_SCAN_RAN,
       LN_SCAN_FILES_BEGIN, LN_SCAN_FILES, LN_SCAN_FILES_END };

/* Where the scanner leaves apkg's list of packages for the main task. */
#define LN_PACKAGES_FILE "T:Jumpstart-packages.json"

/* Sent to the main task's port; the receiver frees it with
 * ln_scan_free_msg. path and title are in the system's character set.
 * FILES carries a drawer in path and its entries in title: one per line,
 * 'D' or 'F' and the name. FILES_BEGIN starts a walk of all file folders,
 * FILES_END ends it (not sent when the walk was cut short).
 * BEGIN and END carry the folder in path. PACKAGES says that apkg's list
 * was written to LN_PACKAGES_FILE and carries apkg's own path. */
struct ln_scan_msg {
    struct Message m;
    LONG kind;
    char *path;
    char *title;
};

struct ln_scan;

/* Starts the scanner for these folders (copied). NULL when it could not
 * be started. */
/* exclude: folders (a path, with ':' or '/') left out of the file walk with
 * all below them, and AmigaDOS name patterns for entries left out anywhere. */
struct ln_scan *ln_scan_start(struct MsgPort *to, char *const *folders, int n, char *const *files, int files_n,
                              char *const *exclude, int exclude_n);
/* Asks for every folder to be scanned again. */
void ln_scan_again(struct ln_scan *s);
/* Asks for the file folders to be walked again. */
void ln_scan_files(struct ln_scan *s);
/* Asks for apkg's list of packages again. */
void ln_scan_packages(struct ln_scan *s);
/* Runs a command line in the background (a widget's provider); when it
 * has ended a RAN message comes, with the command in path. One at a time:
 * 0 when one is still running or the line is too long. */
int ln_scan_run(struct ln_scan *s, const char *command);
/* Ends the scanner and waits until it is gone; messages still arriving at
 * the port are freed. */
void ln_scan_stop(struct ln_scan *s);
void ln_scan_free_msg(struct ln_scan_msg *m);

#endif
