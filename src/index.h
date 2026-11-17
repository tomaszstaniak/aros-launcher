/* SPDX-License-Identifier: MIT */
/* The programs the launcher knows, with how often each was started. Pure.
 *
 * Only one task touches an index. Paths are AROS paths and compare without
 * regard to ASCII case, as the file systems do. */
#ifndef LN_INDEX_H
#define LN_INDEX_H

#include <stddef.h>
#include "match.h"

struct ln_entry {
    char *path;
    char *title;
    unsigned uses;       /* times started */
    unsigned last_use;   /* a time that only grows; 0 never */
    unsigned id;         /* stable while the index lives, never 0 */
};

struct ln_index;

struct ln_index *ln_index_new(void);
void ln_index_free(struct ln_index *x);

/* Adds a program or refreshes its title; the same path is one entry.
 * Returns its id, 0 when out of memory. */
unsigned ln_index_put(struct ln_index *x, const char *path, const char *title);
void ln_index_remove(struct ln_index *x, const char *path);
const struct ln_entry *ln_index_find(const struct ln_index *x, unsigned id);
int ln_index_count(const struct ln_index *x);

/* A rescan of dir: after ln_index_end the entries inside dir (at any
 * depth) that were not put since ln_index_begin are gone. */
void ln_index_begin(struct ln_index *x, const char *dir);
void ln_index_end(struct ln_index *x, const char *dir);

void ln_index_used(struct ln_index *x, unsigned id, unsigned now);

/* Up to max entries that match the query, best first, with how each
 * matched (m may be NULL). Order: match score plus 3 per use (at most 20
 * uses count), then the later use. An empty query lists used programs
 * first, most used first. Returns how many were written. */
int ln_index_query(const struct ln_index *x, const char *q, size_t qn,
                   const struct ln_entry **out, struct ln_match *m, int max);

/* Use counts as text, one program per line: uses TAB last_use TAB path.
 * Loading keeps counts of paths the index does not hold yet and applies
 * them when the path is put; lines that do not parse are skipped. Returns
 * the lines taken. The saved text is malloc'd; NULL when out of memory. */
int ln_index_load_counts(struct ln_index *x, const char *text, size_t n);
char *ln_index_save_counts(const struct ln_index *x, size_t *n);

#endif
