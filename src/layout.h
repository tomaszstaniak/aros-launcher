/* SPDX-License-Identifier: MIT */
/* Where things are in the panel's picture. Pure. */
#ifndef LN_LAYOUT_H
#define LN_LAYOUT_H

#include "results.h"
#include "style.h"

#define LN_ROWS_MAX 8

struct ln_layout {
    int w, h;                   /* the picture, shadow included */
    int px, py, pw, ph;         /* the panel inside it */
    int query_y;                /* top of the query line */
    int list_y;                 /* top of the list */
    int footer_y;
    int first;                  /* result shown first */
    int shown;                  /* results shown */
    int row_y[LN_ROWS_MAX];     /* top of each shown result */
    int row_index[LN_ROWS_MAX]; /* which result it is */
    int headings;               /* section headings shown */
    int heading_y[LN_ROWS_MAX];
    int heading_source[LN_ROWS_MAX];
    int empty_y;                /* top of the "No results" line, -1 when rows are shown */
};

/* first is where the list was scrolled to before (0 at a new query); the
 * layout moves it only as far as needed to show the selection. shadow is
 * the room kept around the panel for its shadow, 0 for none. */
void ln_layout_make(struct ln_layout *l, const struct ln_style *s, const struct ln_results *r,
                    int first, int shadow);
/* The size of the largest picture this look can need (LN_ROWS_MAX rows):
 * a window of this size holds every layout. */
void ln_layout_max(const struct ln_style *s, int shadow, int *w, int *h);
/* Stretches the panel to its largest height, the footer at the bottom,
 * for a window whose outline cannot change while it is open. */
void ln_layout_pin(struct ln_layout *l, const struct ln_style *s);
/* The result under a point of the picture, -1 when there is none. */
int ln_layout_row_at(const struct ln_layout *l, const struct ln_style *s, int x, int y);
/* Whether a point of the picture is on the panel (not its shadow or corners). */
int ln_layout_inside(const struct ln_layout *l, const struct ln_style *s, int x, int y);

#endif
