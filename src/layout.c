/* SPDX-License-Identifier: MIT */
#include "layout.h"

/* Room for the list: LN_ROWS_MAX rows and one heading. More headings (more
 * sections on show) take the room of rows, so that no layout is ever
 * higher than ln_layout_max says. */
static int list_room(const struct ln_style *s) { return s->heading_h + LN_ROWS_MAX * s->row_h; }

void ln_layout_make(struct ln_layout *l, const struct ln_style *s, const struct ln_results *r,
                    int first, int shadow)
{
    int n = ln_results_count(r), sel = ln_results_selected(r), y, i, last_source, used;

    if (first > n - LN_ROWS_MAX) first = n - LN_ROWS_MAX;     /* no empty room below the last rows */
    if (first < 0) first = 0;
    if (sel >= 0 && sel < first) first = sel;
    l->px = shadow;
    l->py = shadow;
    l->pw = s->width;
    l->query_y = l->py;
    l->list_y = l->py + s->query_h + 1;     /* a rule under the query line */
    for (;;) {
        l->first = first;
        l->shown = 0;
        l->headings = 0;
        l->empty_y = -1;
        last_source = -1000;      /* no source has this number */
        used = 0;
        y = l->list_y + s->inset;
        for (i = first; i < n && l->shown < LN_ROWS_MAX; i++) {
            const struct ln_row *w = ln_results_row(r, i);
            int heading = s->heading_h && w->source != last_source;
            if (used + (heading ? s->heading_h : 0) + s->row_h > list_room(s)) break;
            if (heading) {
                l->heading_y[l->headings] = y;
                l->heading_source[l->headings] = w->source;
                l->headings++;
                y += s->heading_h;
                used += s->heading_h;
            }
            last_source = w->source;
            l->row_y[l->shown] = y;
            l->row_index[l->shown] = i;
            l->shown++;
            y += s->row_h;
            used += s->row_h;
        }
        /* scroll on until the selection is on show */
        if (sel < 0 || !l->shown || sel <= l->row_index[l->shown - 1]) break;
        first++;
    }
    if (!n) {
        l->empty_y = y;
        y += s->row_h;
    }
    y += s->inset;
    l->footer_y = y;
    l->ph = y + 1 + s->footer_h - l->py;
    l->w = l->pw + 2 * shadow;
    l->h = l->ph + 2 * shadow;
}

int ln_layout_inside(const struct ln_layout *l, const struct ln_style *s, int x, int y)
{
    int rx, ry, r = s->radius;
    x -= l->px; y -= l->py;
    if (x < 0 || y < 0 || x >= l->pw || y >= l->ph) return 0;
    rx = x < r ? r - x : x >= l->pw - r ? x - (l->pw - r - 1) : 0;
    ry = y < r ? r - y : y >= l->ph - r ? y - (l->ph - r - 1) : 0;
    return rx * rx + ry * ry <= r * r;
}

int ln_layout_row_at(const struct ln_layout *l, const struct ln_style *s, int x, int y)
{
    int i;
    if (x < l->px + s->inset || x >= l->px + l->pw - s->inset) return -1;
    for (i = 0; i < l->shown; i++)
        if (y >= l->row_y[i] && y < l->row_y[i] + s->row_h) return l->row_index[i];
    return -1;
}

static int max_ph(const struct ln_style *s)
{
    return s->query_h + 1 + s->inset + s->heading_h + LN_ROWS_MAX * s->row_h + s->inset + 1 + s->footer_h;
}

void ln_layout_max(const struct ln_style *s, int shadow, int *w, int *h)
{
    *w = s->width + 2 * shadow;
    *h = max_ph(s) + 2 * shadow;
}

void ln_layout_pin(struct ln_layout *l, const struct ln_style *s)
{
    int ph = max_ph(s);
    if (l->ph >= ph) return;
    l->footer_y += ph - l->ph;
    l->ph = ph;
    l->h = ph + 2 * l->py;
}
