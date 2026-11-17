/* SPDX-License-Identifier: MIT */
/* The package catalogue as apkg prints it with "apkg search --json":
 *
 *   { "rows": [
 *     { "id": "zunecalc", "version": "1.0", "revision": 1, "summary": "...",
 *       ..., "installed": false } ], "hidden": 0 }
 *
 * Pure: text in, a list out. */
#ifndef LN_PKGLIST_H
#define LN_PKGLIST_H

#include <stddef.h>

struct ln_pkg { char *id, *version, *summary; int installed; };
struct ln_pkglist { struct ln_pkg *v; int n; };

/* Reads the rows into l (zeroed or freed before). 1 when the text is such
 * a list, also an empty one; 0 when it is something else or memory ran
 * out (l is then empty). Rows without an id are left out. */
int ln_pkglist_parse(struct ln_pkglist *l, const char *json, size_t n);
void ln_pkglist_free(struct ln_pkglist *l);

#endif
