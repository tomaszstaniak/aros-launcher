/* SPDX-License-Identifier: MIT */
/* The settings window (Zune). */
#ifndef LN_PREFS_AROS_H
#define LN_PREFS_AROS_H

#include "config.h"

/* Shows the settings and waits until the window is closed. 1 when the
 * user saved: c then holds the new settings (c->autostart is what the
 * user chose). 0 when cancelled or when the window could not be made. */
int ln_prefs_run(struct ln_config *c);

#endif
