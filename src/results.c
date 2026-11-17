/* SPDX-License-Identifier: MIT */
#include "results.h"

#include <stdlib.h>
#include <string.h>

struct ln_results {
    struct ln_row *row;
    int n;
    unsigned query;
    int have_sel;         /* a selection identity exists */
    int user_moved;       /* the user chose it; otherwise it follows the first row */
    int sel_source;
    unsigned sel_key;
    int sel_pos;          /* where it was last, for when its row vanishes */
};

static char *copy(const char *s)
{
    size_t n;
    char *c;
    if (!s) return NULL;
    n = strlen(s) + 1;
    if ((c = malloc(n))) memcpy(c, s, n);
    return c;
}

static void free_row(struct ln_row *w)
{
    free((char *)w->title);
    free((char *)w->subtitle);
    free((char *)w->kind);
}

static int find(const struct ln_results *r, int source, unsigned key)
{
    int i;
    for (i = 0; i < r->n; i++)
        if (r->row[i].source == source && r->row[i].key == key) return i;
    return -1;
}

/* Puts the selection where it belongs after the rows changed. */
static void settle(struct ln_results *r)
{
    int i;
    if (!r->n) { r->have_sel = 0; r->sel_pos = 0; return; }
    if (r->have_sel && r->user_moved) {
        i = find(r, r->sel_source, r->sel_key);
        if (i < 0) i = r->sel_pos < r->n ? r->sel_pos : r->n - 1;
    } else if (r->have_sel) {
        /* untouched: stays with its row while that row exists */
        i = find(r, r->sel_source, r->sel_key);
        if (i < 0) i = 0;
    } else {
        i = 0;
    }
    r->have_sel = 1;
    r->sel_source = r->row[i].source;
    r->sel_key = r->row[i].key;
    r->sel_pos = i;
}

struct ln_results *ln_results_new(void)
{
    return calloc(1, sizeof(struct ln_results));
}

void ln_results_free(struct ln_results *r)
{
    int i;
    if (!r) return;
    for (i = 0; i < r->n; i++) free_row(&r->row[i]);
    free(r->row);
    free(r);
}

unsigned ln_results_query(struct ln_results *r)
{
    int i;
    for (i = 0; i < r->n; i++) free_row(&r->row[i]);
    r->n = 0;
    r->have_sel = r->user_moved = 0;
    r->sel_pos = 0;
    if (!++r->query) r->query = 1;
    return r->query;
}

unsigned ln_results_current(const struct ln_results *r) { return r->query; }

static int by_order(const void *a, const void *b)
{
    const struct ln_row *x = a, *y = b;
    if (x->source != y->source) return x->source - y->source;
    if (x->score != y->score) return y->score > x->score ? 1 : -1;
    return x->key < y->key ? -1 : x->key > y->key;
}

int ln_results_answer(struct ln_results *r, unsigned q, int source, const struct ln_row *rows, int n)
{
    struct ln_row *fresh;
    int i, kept = 0, total;
    if (q != r->query || n < 0) return 0;
    for (i = 0; i < r->n; i++) kept += r->row[i].source != source;
    total = kept + n;
    fresh = calloc((size_t)(total ? total : 1), sizeof *fresh);
    if (!fresh) return 0;
    /* copy the new rows first: a failure leaves the list untouched */
    for (i = 0; i < n; i++) {
        struct ln_row *w = &fresh[kept + i];
        *w = rows[i];
        w->source = source;
        w->title = copy(rows[i].title ? rows[i].title : "");
        w->subtitle = copy(rows[i].subtitle);
        w->kind = copy(rows[i].kind);
        if (!w->title || (rows[i].subtitle && !w->subtitle) || (rows[i].kind && !w->kind)) {
            int j;
            for (j = 0; j <= i; j++) free_row(&fresh[kept + j]);
            free(fresh);
            return 0;
        }
    }
    kept = 0;
    for (i = 0; i < r->n; i++) {
        if (r->row[i].source != source) fresh[kept++] = r->row[i];
        else free_row(&r->row[i]);
    }
    free(r->row);
    r->row = fresh;
    r->n = total;
    qsort(r->row, (size_t)r->n, sizeof *r->row, by_order);
    settle(r);
    return 1;
}

int ln_results_count(const struct ln_results *r) { return r->n; }

const struct ln_row *ln_results_row(const struct ln_results *r, int i)
{
    return i >= 0 && i < r->n ? &r->row[i] : NULL;
}

int ln_results_selected(const struct ln_results *r)
{
    return r->n && r->have_sel ? r->sel_pos : -1;
}

void ln_results_select(struct ln_results *r, int i)
{
    if (!r->n) return;
    if (i < 0) i = 0;
    if (i >= r->n) i = r->n - 1;
    r->have_sel = r->user_moved = 1;
    r->sel_source = r->row[i].source;
    r->sel_key = r->row[i].key;
    r->sel_pos = i;
}

void ln_results_move(struct ln_results *r, int delta)
{
    if (!r->n) return;
    ln_results_select(r, r->sel_pos + delta);
}
