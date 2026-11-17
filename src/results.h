/* SPDX-License-Identifier: MIT */
/* The rows the panel shows for the current query. Pure.
 *
 * Sources answer at different times. Each query has a number; an answer
 * says which query it is for, and answers for an earlier query are
 * dropped. The selection is a row's identity (its source and the key the
 * source gave it), not a position, so rows arriving later never change
 * what Return acts on. */
#ifndef LN_RESULTS_H
#define LN_RESULTS_H

#include "match.h"

enum { LN_SRC_PROGRAMS = 1, LN_SRC_LAST = 8 };   /* order of sources in the list */

struct ln_row {
    int source;
    unsigned key;            /* the source's name for this row, stable across answers */
    const char *title;       /* copied on answer */
    const char *subtitle;    /* may be NULL */
    const char *kind;        /* a short word shown at the right; may be NULL */
    struct ln_match m;
    int score;               /* order inside the source, higher first */
};

struct ln_results;

struct ln_results *ln_results_new(void);
void ln_results_free(struct ln_results *r);

/* Starts a new query: returns its number and empties the list. The
 * selection is forgotten; it will sit on the first row again. */
unsigned ln_results_query(struct ln_results *r);
unsigned ln_results_current(const struct ln_results *r);

/* The rows of one source for query q, replacing that source's earlier
 * rows. 1 taken; 0 when q is not the current query or memory ran out (the
 * list is then as before). */
int ln_results_answer(struct ln_results *r, unsigned q, int source, const struct ln_row *rows, int n);

int ln_results_count(const struct ln_results *r);
const struct ln_row *ln_results_row(const struct ln_results *r, int i);
/* The selected row's position, -1 when the list is empty. */
int ln_results_selected(const struct ln_results *r);
/* Moves the selection by delta rows, stopping at the ends. */
void ln_results_move(struct ln_results *r, int delta);
void ln_results_select(struct ln_results *r, int i);

#endif
