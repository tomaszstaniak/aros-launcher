/* SPDX-License-Identifier: MIT */
/* Tells the caller when the clipboard's content changed. */
#ifndef LN_CLIPWATCH_AROS_H
#define LN_CLIPWATCH_AROS_H

#include <exec/tasks.h>
#include <exec/types.h>

/* From now on task gets the signals of mask after every change of
 * clipboard unit 0. 0 when the clipboard cannot be watched. */
int ln_clipwatch_start(struct Task *task, ULONG mask);
void ln_clipwatch_stop(void);

#endif
