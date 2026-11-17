/* SPDX-License-Identifier: MIT */
/* The launcher's settings as a small text file. Pure.
 *
 *   folder=SYS:Tools
 *   theme=dark | light
 *   compact=0 | 1
 *   glass=0 | 1
 *
 * One setting per line; lines that are not understood are kept and
 * written back, so a file from a later version survives an older one. */
#ifndef LN_CONFIG_H
#define LN_CONFIG_H

#include <stddef.h>

#define LN_FOLDERS_MAX 32

struct ln_config {
    char *folder[LN_FOLDERS_MAX];
    int folders;
    int dark, compact, glass;
    char *other;        /* lines kept as they were, each ending in a line feed */
};

/* The settings a first start has: the system's program drawers and the
 * drawer apkg installs packages into (SYS:Packages), dark,
 * full rows, frosted glass. 0 when out of memory. */
int ln_config_defaults(struct ln_config *c);
/* Reads text into c (which must be zeroed or freed first). A file without
 * any folder line keeps no folders: the caller decides whether that means
 * "use the defaults". 0 when out of memory. */
int ln_config_parse(struct ln_config *c, const char *text, size_t n);
/* The settings as text, malloc'd; NULL when out of memory. */
char *ln_config_text(const struct ln_config *c, size_t *n);
/* Adds a folder unless it is there already (compared without case, a
 * trailing '/' ignored). 1 added, 0 not (already there, full, no memory). */
int ln_config_add_folder(struct ln_config *c, const char *path);
void ln_config_remove_folder(struct ln_config *c, int i);
void ln_config_free(struct ln_config *c);

#endif
