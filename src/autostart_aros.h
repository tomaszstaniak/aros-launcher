/* SPDX-License-Identifier: MIT */
/* Starting with the system, through SYS:WBStartup.
 *
 * Wanderer opens every icon in SYS:WBStartup. A project icon without a
 * file of its own is enough: its default tool is started from the tool's
 * own drawer. So the start icon is SYS:WBStartup/Jumpstart.info, a project
 * icon whose default tool is the full path of this program; the program
 * stays where it was installed and an update there is what starts next. */
#ifndef LN_AUTOSTART_AROS_H
#define LN_AUTOSTART_AROS_H

#include <stddef.h>

struct WBStartup;

/* The full path of the running program into out; wb is the Workbench's
 * start message, NULL when started from a Shell. 0 when it cannot be told. */
int ln_program_path(const struct WBStartup *wb, char *out, size_t cap);
/* 1 when a start icon is there (whichever program it starts). */
int ln_autostart_on(void);
/* Writes the start icon for program, or removes it. 1 done. */
int ln_autostart_set(int on, const char *program);

#endif
