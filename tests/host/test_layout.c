#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "layout.h"

static void fill(struct ln_results *r, unsigned q, int n)
{
    struct ln_row w[32];
    char name[32][16];
    int i;
    memset(w, 0, sizeof w);
    for (i = 0; i < n; i++) {
        snprintf(name[i], sizeof name[i], "P%02d", i);
        w[i].key = (unsigned)i + 1; w[i].title = name[i]; w[i].score = 1000 - i;
    }
    assert(ln_results_answer(r, q, LN_SRC_PROGRAMS, w, n));
}

int main(void)
{
    const struct ln_style *full = ln_style_get(1, 0), *compact = ln_style_get(1, 1);
    struct ln_results *r = ln_results_new();
    struct ln_layout l;
    unsigned q = ln_results_query(r);
    int h0;

    assert(full->width == 620 && compact->width == 560 && full->two_lines && !compact->two_lines);
    assert(ln_style_get(0, 0)->width == 620 && ln_style_get(0, 0)->text != full->text);

    /* no rows: the query line, one line saying so, the footer */
    ln_layout_make(&l, full, r, 0, 0);
    assert(l.shown == 0 && l.headings == 0 && l.empty_y > l.list_y);
    assert(l.w == 620 && l.h == l.ph && l.px == 0);
    h0 = l.h;

    /* 3 rows: one heading, rows below it, no "empty" line */
    fill(r, q, 3);
    ln_layout_make(&l, full, r, 0, 0);
    assert(l.shown == 3 && l.headings == 1 && l.empty_y == -1 && l.first == 0);
    assert(l.heading_y[0] < l.row_y[0] && l.row_y[1] == l.row_y[0] + full->row_h);
    assert(l.h == h0 + full->heading_h + 2 * full->row_h);
    /* compact has no headings */
    ln_layout_make(&l, compact, r, 0, 0);
    assert(l.headings == 0 && l.shown == 3 && l.w == 560);

    /* the shadow adds room all round and moves the panel */
    ln_layout_make(&l, full, r, 0, 24);
    assert(l.px == 24 && l.py == 24 && l.w == 620 + 48 && l.h == l.ph + 48);
    assert(ln_layout_inside(&l, full, 24 + 310, 24 + 20) && !ln_layout_inside(&l, full, 5, 5));
    assert(!ln_layout_inside(&l, full, 24, 24));        /* the rounded corner */
    assert(ln_layout_row_at(&l, full, 300, l.row_y[1] + 3) == 1);
    assert(ln_layout_row_at(&l, full, 300, l.query_y + 5) == -1);
    assert(ln_layout_row_at(&l, full, 300, l.footer_y + 5) == -1);
    assert(ln_layout_row_at(&l, full, 300, l.heading_y[0] + 2) == -1);
    assert(ln_layout_row_at(&l, full, 2, l.row_y[1] + 3) == -1);

    /* 20 rows: at most 8 shown, scrolled just far enough for the selection */
    fill(r, q, 20);
    ln_layout_make(&l, full, r, 0, 0);
    assert(l.shown == LN_ROWS_MAX && l.first == 0);
    ln_results_select(r, 15);
    ln_layout_make(&l, full, r, 0, 0);
    assert(l.first == 8 && l.row_index[0] == 8 && l.row_index[7] == 15);
    /* moving up inside the window does not scroll */
    ln_results_select(r, 10);
    ln_layout_make(&l, full, r, 8, 0);
    assert(l.first == 8);
    ln_results_select(r, 3);
    ln_layout_make(&l, full, r, 8, 0);
    assert(l.first == 3);
    /* a first beyond the end is pulled back */
    ln_results_select(r, 19);
    ln_layout_make(&l, full, r, 100, 0);
    assert(l.first == 12 && l.shown == 8);
    /* a heading only when the first shown row starts the list or a new source */
    assert(l.headings == 1);

    /* the largest picture holds every layout; pinning stretches a short one */
    {
        int mw, mh, fy;
        ln_layout_max(full, 24, &mw, &mh);
        assert(mw == l.w + 48 && mh >= l.h + 48);
        q = ln_results_query(r);
        fill(r, q, 2);
        ln_layout_make(&l, full, r, 0, 24);
        assert(l.h < mh && l.w == mw);
        fy = l.footer_y;
        ln_layout_pin(&l, full);
        assert(l.h == mh && l.footer_y > fy && l.footer_y + 1 + full->footer_h == l.py + l.ph);
        fill(r, q, 20);
        ln_layout_make(&l, full, r, 0, 24);
        assert(l.h == mh);
    }
    ln_results_free(r);
    return 0;
}
