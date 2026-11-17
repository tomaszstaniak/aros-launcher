/* SPDX-License-Identifier: MIT */
/* The names of files and drawers in the folders the user chose, for
 * finding them by name. Pure.
 *
 * The index is filled while a folder is walked (a new one beside the one
 * in use, swapped in when the walk is complete) and asked with the words
 * typed: every word has to occur in the name, without regard to case.
 * Names are bytes as the file system gives them; comparing folds ASCII
 * letters only, which is what the file systems do. */
#ifndef LN_FILEINDEX_H
#define LN_FILEINDEX_H

#include <stddef.h>

#define LN_FILES_MAX 300000         /* entries; more are not taken */

struct ln_fileindex;

struct ln_file {
    const char *dir;        /* the drawer, as a path */
    const char *name;       /* the entry in it */
    int is_dir;
};

struct ln_fileindex *ln_fileindex_new(void);
void ln_fileindex_free(struct ln_fileindex *x);
int ln_fileindex_count(const struct ln_fileindex *x);

/* Adds one entry of drawer dir. 1 added; 0 when full or out of memory. */
int ln_fileindex_add(struct ln_fileindex *x, const char *dir, const char *name, int is_dir);

/* Up to max entries whose name holds every word of the query (words are
 * separated by spaces), best first: a name that starts with the first word,
 * then one where a word starts with it, then the others; shorter names
 * first among equals; drawers before files among equals. Returns how many
 * were written. A query without a word of at least two bytes finds nothing. */
int ln_fileindex_query(const struct ln_fileindex *x, const char *q, size_t qn, struct ln_file *out, int max);

/* The index as text, one entry per line: "D" or "F", a tab, the drawer, a
 * tab, the name. malloc'd; NULL when out of memory. Loading adds the lines
 * to x and returns how many were taken; lines that do not parse are skipped. */
char *ln_fileindex_save(const struct ln_fileindex *x, size_t *n);
int ln_fileindex_load(struct ln_fileindex *x, const char *text, size_t n);

#endif
