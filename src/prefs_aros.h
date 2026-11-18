/* SPDX-License-Identifier: MIT */
/* The settings window and the About window (Zune). */
#ifndef LN_PREFS_AROS_H
#define LN_PREFS_AROS_H

#include "config.h"

/* What the About page shows beside the fixed text. */
struct ln_about {
    const char *version;        /* "Jumpstart 0.2 (17.11.2026)" */
    const char *hotkey;
    int programs, files, packages;
    const char *rates_date;     /* "" when there are none */
};

/* Shows the settings and waits until the window is closed. page is the tab
 * shown first (0 programs, 1 files, 2 look, 3 general, 4 about). 1 when
 * the user saved: c then holds the new settings (c->autostart is what the
 * user chose). 0 when cancelled or when the window could not be made. */
int ln_prefs_run(struct ln_config *c, const struct ln_about *about, int page);

#endif
