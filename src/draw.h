/* SPDX-License-Identifier: MIT */
/* The panel's picture. Pure: it draws into memory and knows no window. */
#ifndef LN_DRAW_H
#define LN_DRAW_H

#include <stddef.h>
#include "layout.h"
#include "zk_text.h"

struct ln_view {
    const char *query;              /* UTF-8 */
    size_t qn;
    size_t caret, sel_start, sel_end;   /* byte offsets; no selection when equal */
    int caret_on;                   /* the caret is in its visible phase */
    const struct ln_results *results;
    const char *footer;             /* a message at the bottom left; NULL: the selected title */
    /* The desktop behind the panel, already blurred: pw x ph pixels of
       A,R,G,B bytes, or NULL for a plain body. */
    const unsigned char *backdrop;
    /* An icon for a row as size x size pixels of A,R,G,B bytes (straight
       alpha), or NULL when there is none yet; may be NULL itself. */
    const unsigned char *(*icon)(void *ctx, const struct ln_row *row, int size);
    void *icon_ctx;
};

struct ln_fonts { zt_font *query, *title, *small; };

/* The name of a source's section ("Programs and commands", "Calculator"). */
const char *ln_source_name(int source);

/* Draws the whole picture into argb: l->w x l->h pixels, four bytes each
 * (A,R,G,B, straight alpha), rows of l->w pixels. Alpha is 0 outside the
 * panel's rounded outline and its shadow. 0 when out of memory (argb is
 * then cleared). */
int ln_draw(unsigned char *argb, const struct ln_layout *l, const struct ln_style *s,
            const struct ln_view *v, const struct ln_fonts *f);

#endif
