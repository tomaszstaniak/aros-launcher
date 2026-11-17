/* SPDX-License-Identifier: MIT */
#include "draw.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct cv {
    unsigned char *p;       /* A,R,G,B */
    int w, h;
    unsigned char *cov;     /* scratch for one line of text: w x COV_H */
};
#define COV_H 96

struct range { long x0, x1; };

#define A(c) ((int)(((c) >> 24) & 255))
#define R(c) ((int)(((c) >> 16) & 255))
#define G(c) ((int)(((c) >> 8) & 255))
#define B(c) ((int)((c) & 255))

/* Source over destination, both straight alpha; a is the source's alpha. */
static void over(unsigned char *d, int a, int r, int g, int b)
{
    int da, oa;
    if (a <= 0) return;
    if (a >= 255) { d[0] = 255; d[1] = (unsigned char)r; d[2] = (unsigned char)g; d[3] = (unsigned char)b; return; }
    da = d[0] * (255 - a) / 255;
    oa = a + da;
    d[1] = (unsigned char)((r * a + d[1] * da) / oa);
    d[2] = (unsigned char)((g * a + d[2] * da) / oa);
    d[3] = (unsigned char)((b * a + d[3] * da) / oa);
    d[0] = (unsigned char)oa;
}

static void put(struct cv *c, int x, int y, unsigned long col, int cov)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return;
    over(c->p + ((size_t)y * (size_t)c->w + (size_t)x) * 4, A(col) * cov / 255, R(col), G(col), B(col));
}

/* Distance from the centre of pixel (x,y) to the outline of a rounded
 * rectangle, positive inside. */
static float edge(int x, int y, int rx, int ry, int rw, int rh, int rad)
{
    float cx = (float)x + 0.5f - (float)rx, cy = (float)y + 0.5f - (float)ry;
    float dx = cx < (float)rad ? (float)rad - cx : cx > (float)(rw - rad) ? cx - (float)(rw - rad) : 0.0f;
    float dy = cy < (float)rad ? (float)rad - cy : cy > (float)(rh - rad) ? cy - (float)(rh - rad) : 0.0f;
    float d;
    if (dx > 0.0f && dy > 0.0f) return (float)rad - sqrtf(dx * dx + dy * dy);
    d = cx;
    if ((float)rw - cx < d) d = (float)rw - cx;
    if (cy < d) d = cy;
    if ((float)rh - cy < d) d = (float)rh - cy;
    return d;
}

static int coverage(float d)
{
    d += 0.5f;
    return d <= 0.0f ? 0 : d >= 1.0f ? 255 : (int)(d * 255.0f);
}

static void fill_round(struct cv *c, int rx, int ry, int rw, int rh, int rad, unsigned long col)
{
    int x, y;
    for (y = ry; y < ry + rh; y++)
        for (x = rx; x < rx + rw; x++)
            put(c, x, y, col, coverage(edge(x, y, rx, ry, rw, rh, rad)));
}

static void fill(struct cv *c, int rx, int ry, int rw, int rh, unsigned long col)
{
    int x, y;
    for (y = ry; y < ry + rh; y++)
        for (x = rx; x < rx + rw; x++) put(c, x, y, col, 255);
}

/* One piece of text in a box box_h high starting at top, centred on its
 * line. Columns outside [clip0, clip1) are not drawn. Columns inside one
 * of the hl ranges take hlcol. Returns the text's width. */
static long text(struct cv *c, zt_font *f, const char *s, size_t n, long x, int top, int box_h,
                 unsigned long col, int clip0, int clip1, const struct range *hl, int nhl, unsigned long hlcol)
{
    int asc = zt_font_ascent(f), desc = zt_font_descent(f), bh = asc + desc + 6, row, px, y0, k;
    long w;
    if (!n) return 0;
    if (bh > COV_H) bh = COV_H;
    memset(c->cov, 0, (size_t)c->w * (size_t)bh);
    w = zt_draw(f, s, n, 0, c->cov, c->w, bh, c->w, x, asc + 3);
    y0 = top + (box_h - (asc + desc)) / 2 - 3;
    if (clip0 < 0) clip0 = 0;
    if (clip1 > c->w) clip1 = c->w;
    for (row = 0; row < bh; row++) {
        const unsigned char *cr = c->cov + (size_t)row * (size_t)c->w;
        for (px = clip0; px < clip1; px++) {
            unsigned long use = col;
            if (!cr[px]) continue;
            for (k = 0; k < nhl; k++)
                if (px >= hl[k].x0 && px < hl[k].x1) { use = hlcol; break; }
            put(c, px, y0 + row, use, cr[px]);
        }
    }
    return w;
}

/* Text that must end before limit: cut at a character boundary with an
 * ellipsis when it is too long. hl are byte offsets of matched characters.
 * Returns the x after what was drawn. */
static long fitted(struct cv *c, zt_font *f, const char *s, long x, int top, int box_h, long limit,
                   unsigned long col, const struct ln_match *m, unsigned long hlcol)
{
    static const char dots[] = "\xe2\x80\xa6";
    size_t n = strlen(s), k = n;
    long avail = limit - x, w, dw = 0;
    struct range hl[LN_MATCH_MAX];
    int nhl = 0, i;
    if (avail <= 0) return x;
    w = zt_measure(f, s, n, 0);
    if (w > avail) {
        dw = zt_measure(f, dots, 3, 0);
        k = avail > dw ? zt_fit(f, s, n, avail - dw, 0) : 0;
        w = k ? zt_measure(f, s, k, 0) : 0;
    }
    if (m)
        for (i = 0; i < m->n; i++) {
            size_t p = m->pos[i];
            if (p >= k) continue;
            hl[nhl].x0 = x + zt_x(f, s, k, p, 0);
            hl[nhl].x1 = x + zt_x(f, s, k, zt_next_cluster(s, k, p), 0);
            nhl++;
        }
    text(c, f, s, k, x, top, box_h, col, (int)x, (int)limit, hl, nhl, hlcol);
    if (k < n) {
        text(c, f, dots, 3, x + w, top, box_h, col, (int)x, (int)limit, NULL, 0, 0);
        w += dw;
    }
    return x + w;
}

static void right_text(struct cv *c, zt_font *f, const char *s, long right, int top, int box_h, unsigned long col)
{
    size_t n = strlen(s);
    long w = zt_measure(f, s, n, 0);
    text(c, f, s, n, right - w, top, box_h, col, 0, c->w, NULL, 0, 0);
}

const char *ln_source_name(int source)
{
    switch (source) {
    case LN_SRC_SHELL: return "Shell";
    case LN_SRC_CALC: return "Calculator";
    case LN_SRC_PROGRAMS: return "Results";
    case LN_SRC_PACKAGES: return "Packages";
    case LN_SRC_ACTIONS: return "Actions";
    case LN_SRC_FILES: return "Files";
    case LN_SRC_REXX: return "ARexx";
    default: return "";
    }
}

static void body(struct cv *c, const struct ln_layout *l, const struct ln_style *s, const unsigned char *backdrop)
{
    int x, y;
    /* The shadow: soft, a little below the panel. */
    if (l->px > 0) {
        int room = l->px, drop = room / 4;
        for (y = 0; y < c->h; y++)
            for (x = 0; x < c->w; x++) {
                float d = -edge(x, y - drop, l->px, l->py, l->pw, l->ph, s->radius);
                float t;
                if (d >= (float)room) continue;
                if (d < 0.0f) d = 0.0f;
                t = 1.0f - d / (float)room;
                c->p[((size_t)y * (size_t)c->w + (size_t)x) * 4] = (unsigned char)(110.0f * t * t);
            }
    }
    for (y = l->py; y < l->py + l->ph; y++)
        for (x = l->px; x < l->px + l->pw; x++) {
            float d = edge(x, y, l->px, l->py, l->pw, l->ph, s->radius);
            int cov = coverage(d);
            unsigned char *p = c->p + ((size_t)y * (size_t)c->w + (size_t)x) * 4;
            if (!cov) continue;
            if (backdrop) {
                const unsigned char *b = backdrop + ((size_t)(y - l->py) * (size_t)l->pw + (size_t)(x - l->px)) * 4;
                int a = A(s->tint);
                int r = (R(s->tint) * a + b[1] * (255 - a)) / 255;
                int g = (G(s->tint) * a + b[2] * (255 - a)) / 255;
                int bl = (B(s->tint) * a + b[3] * (255 - a)) / 255;
                if (cov == 255) { p[0] = 255; p[1] = (unsigned char)r; p[2] = (unsigned char)g; p[3] = (unsigned char)bl; }
                else over(p, cov, r, g, bl);
            } else {
                if (cov == 255) { p[0] = (unsigned char)A(s->tint_plain); p[1] = (unsigned char)R(s->tint_plain);
                                  p[2] = (unsigned char)G(s->tint_plain); p[3] = (unsigned char)B(s->tint_plain); }
                else over(p, A(s->tint_plain) * cov / 255, R(s->tint_plain), G(s->tint_plain), B(s->tint_plain));
            }
            /* a hairline just inside the outline */
            if (d < 1.0f) over(p, A(s->edge) * cov / 255, R(s->edge), G(s->edge), B(s->edge));
        }
}

static void placeholder_icon(struct cv *c, const struct ln_row *w, int x, int y, int size, zt_font *f)
{
    static const unsigned long tones[] = { 0xff3f7de0ul, 0xffe08a3ful, 0xff4aa56bul, 0xff8a62d6ul, 0xff888888ul, 0xffc9546bul };
    unsigned h = w->key * 2654435761u;
    const char *mark = w->source == LN_SRC_CALC ? "=" : w->title;
    size_t n = mark ? zt_next_cluster(mark, strlen(mark), 0) : 0;
    fill_round(c, x, y, size, size, size / 4, tones[(h >> 16) % (sizeof tones / sizeof tones[0])]);
    if (n && size >= 20) {
        long tw = zt_measure(f, mark, n, 0);
        text(c, f, mark, n, x + (size - tw) / 2, y, size, 0xfffffffful, x, x + size, NULL, 0, 0);
    }
}

static void icon(struct cv *c, const struct ln_view *v, const struct ln_row *w, int x, int y, int size, zt_font *f)
{
    const unsigned char *pic = v->icon ? v->icon(v->icon_ctx, w, size) : NULL;
    int i, j;
    if (!pic) { placeholder_icon(c, w, x, y, size, f); return; }
    for (j = 0; j < size; j++)
        for (i = 0; i < size; i++) {
            const unsigned char *s = pic + ((size_t)j * (size_t)size + (size_t)i) * 4;
            if (x + i < 0 || y + j < 0 || x + i >= c->w || y + j >= c->h) continue;
            over(c->p + ((size_t)(y + j) * (size_t)c->w + (size_t)(x + i)) * 4, s[0], s[1], s[2], s[3]);
        }
}

static void query_line(struct cv *c, const struct ln_layout *l, const struct ln_style *s,
                       const struct ln_view *v, zt_font *f)
{
    int x0 = l->px + s->pad, x1 = l->px + l->pw - s->pad, top = l->query_y, h = s->query_h;
    int asc = zt_font_ascent(f), desc = zt_font_descent(f), ty = top + (h - (asc + desc)) / 2;
    long caret_x, shift = 0;
    if (!v->qn) {
        static const char hint[] = "Search programs";
        text(c, f, hint, sizeof hint - 1, x0, top, h, s->dim, x0, x1, NULL, 0, 0);
        if (v->caret_on) fill(c, x0, ty, 2, asc + desc, s->text);
        return;
    }
    caret_x = zt_x(f, v->query, v->qn, v->caret, 0);
    if (caret_x > x1 - x0 - 2) shift = caret_x - (x1 - x0 - 2);   /* keep the caret in view */
    if (v->sel_end > v->sel_start) {
        long a = x0 - shift + zt_x(f, v->query, v->qn, v->sel_start, 0);
        long b = x0 - shift + zt_x(f, v->query, v->qn, v->sel_end, 0);
        if (a < x0) a = x0;
        if (b > x1) b = x1;
        if (b > a) fill(c, (int)a, ty - 1, (int)(b - a), asc + desc + 2, (s->match & 0xfffffful) | 0x59000000ul);
    }
    text(c, f, v->query, v->qn, x0 - shift, top, h, s->text, x0, x1, NULL, 0, 0);
    if (v->caret_on) fill(c, (int)(x0 - shift + caret_x), ty, 2, asc + desc, s->text);
}

/* The calculator's result as a card: what was typed on the left, an arrow,
 * the value on the right, both large. */
static void calc_card(struct cv *c, const struct ln_layout *l, const struct ln_style *s, const struct ln_fonts *f,
                      const struct ln_row *w, int y, int h, int selected)
{
    zt_font *big = f->big ? f->big : f->query;
    int x0 = l->px + s->inset, x1 = l->px + l->pw - s->inset, mid = (x0 + x1) / 2, gap = 28;
    const char *left = w->subtitle ? w->subtitle : "", *right = w->title ? w->title : "";
    unsigned long col = selected ? s->sel_text : s->text, dim = selected ? s->sel_dim : s->dim;
    long lw, rw;
    size_t ln = strlen(left), rn = strlen(right);
    fill_round(c, x0, y, x1 - x0, h, s->sel_radius + 3, selected ? s->sel_fill : (s->rule & 0xfffffful) | 0x12000000ul);
    /* the divider, open in the middle for the arrow */
    fill(c, mid, y + 8, 1, h / 2 - 8 - 14, s->rule);
    fill(c, mid, y + h / 2 + 14, 1, h / 2 - 8 - 14, s->rule);
    /* the arrow is drawn, not typed: not every font has one */
    {
        int cy = y + h / 2, k;
        fill(c, mid - 8, cy - 1, 17, 2, dim);
        for (k = 0; k < 6; k++) {
            fill(c, mid + 8 - k, cy - 1 - k, 2, 2, dim);
            fill(c, mid + 8 - k, cy - 1 + k, 2, 2, dim);
        }
    }
    /* each side centred in its half; too long a text starts at the half's edge and is cut */
    lw = zt_measure(big, left, ln, 0);
    rw = zt_measure(big, right, rn, 0);
    if (lw > mid - gap - (x0 + 12)) fitted(c, big, left, x0 + 12, y, h, mid - gap, dim, NULL, 0);
    else text(c, big, left, ln, x0 + (mid - gap / 2 - x0 - lw) / 2, y, h, dim, x0, mid - gap / 2, NULL, 0, 0);
    if (rw > x1 - 12 - (mid + gap)) fitted(c, big, right, mid + gap, y, h, x1 - 12, col, NULL, 0);
    else text(c, big, right, rn, mid + gap / 2 + (x1 - mid - gap / 2 - rw) / 2, y, h, col, mid + gap / 2, x1, NULL, 0, 0);
    /* a note under the answer (where the rates are from), when the row has one */
    if (w->kind && *w->kind && h >= 90) {
        long nw = zt_measure(f->small, w->kind, strlen(w->kind), 0);
        text(c, f->small, w->kind, strlen(w->kind), mid + gap / 2 + (x1 - mid - gap / 2 - nw) / 2, y + h - 26, 20, dim,
             mid + gap / 2, x1, NULL, 0, 0);
    }
}

static void result_row(struct cv *c, const struct ln_layout *l, const struct ln_style *s, const struct ln_view *v,
                       const struct ln_fonts *f, const struct ln_row *w, int y, int selected)
{
    unsigned long col = selected ? s->sel_text : s->text, dim = selected ? s->sel_dim : s->dim;
    unsigned long hl = selected ? s->sel_match : s->match;
    int x = l->px + s->pad, right = l->px + l->pw - s->pad;
    long limit = right, after;
    if (selected) fill_round(c, l->px + s->inset, y, l->pw - 2 * s->inset, s->row_h, s->sel_radius, s->sel_fill);
    icon(c, v, w, x, y + (s->row_h - s->icon) / 2, s->icon, f->title);
    x += s->icon + 12;
    if (w->kind && *w->kind) {
        long kw = zt_measure(f->small, w->kind, strlen(w->kind), 0);
        right_text(c, f->small, w->kind, right, y, s->row_h, dim);
        limit = right - kw - 12;
    }
    if (s->two_lines && w->subtitle && *w->subtitle) {
        int half = s->row_h / 2;
        fitted(c, f->title, w->title, x, y + 3, half, limit, col, &w->m, hl);
        fitted(c, f->small, w->subtitle, x, y + half - 1, half - 3, limit, dim, NULL, 0);
    } else {
        after = fitted(c, f->title, w->title, x, y, s->row_h, limit, col, &w->m, hl);
        if (w->subtitle && *w->subtitle)
            fitted(c, f->small, w->subtitle, after + 10, y + 1, s->row_h, limit, dim, NULL, 0);
    }
}

void ln_draw_cache_reset(struct ln_draw_cache *c)
{
    free(c->base);
    memset(c, 0, sizeof *c);
}

int ln_draw(unsigned char *argb, const struct ln_layout *l, const struct ln_style *s,
            const struct ln_view *v, const struct ln_fonts *f)
{
    return ln_draw_cached(argb, l, s, v, f, NULL);
}

int ln_draw_cached(unsigned char *argb, const struct ln_layout *l, const struct ln_style *s,
                   const struct ln_view *v, const struct ln_fonts *f, struct ln_draw_cache *cache)
{
    struct cv c;
    int i, sel = ln_results_selected(v->results);
    const struct ln_row *srow = ln_results_row(v->results, sel);
    const char *left;

    size_t bytes = (size_t)l->w * (size_t)l->h * 4;

    c.p = argb; c.w = l->w; c.h = l->h;
    if (!(c.cov = malloc((size_t)c.w * COV_H))) { memset(argb, 0, bytes); return 0; }

    if (cache && cache->base && cache->w == l->w && cache->h == l->h && cache->px == l->px && cache->py == l->py
        && cache->pw == l->pw && cache->ph == l->ph && cache->style == s && cache->backdrop == v->backdrop) {
        memcpy(argb, cache->base, bytes);
    } else {
        memset(argb, 0, bytes);
        body(&c, l, s, v->backdrop);
        if (cache) {
            unsigned char *keep = realloc(cache->base, bytes);
            if (keep) {
                memcpy(keep, argb, bytes);
                cache->base = keep;
                cache->w = l->w; cache->h = l->h; cache->px = l->px; cache->py = l->py;
                cache->pw = l->pw; cache->ph = l->ph; cache->style = s; cache->backdrop = v->backdrop;
            } else {
                ln_draw_cache_reset(cache);
            }
        }
    }
    query_line(&c, l, s, v, f->query);
    fill(&c, l->px, l->list_y - 1, l->pw, 1, s->rule);

    for (i = 0; i < l->headings; i++) {
        const char *name = ln_source_name(l->heading_source[i]);
        text(&c, f->small, name, strlen(name), l->px + s->pad, l->heading_y[i], s->heading_h, s->dim,
             l->px, l->px + l->pw, NULL, 0, 0);
    }
    for (i = 0; i < l->shown; i++) {
        const struct ln_row *w = ln_results_row(v->results, l->row_index[i]);
        if (w->source == LN_SRC_CALC) calc_card(&c, l, s, f, w, l->row_y[i], l->row_h[i], l->row_index[i] == sel);
        else result_row(&c, l, s, v, f, w, l->row_y[i], l->row_index[i] == sel);
    }
    if (l->empty_y >= 0) {
        const char *none = v->qn ? "No results" : "No programs yet";
        text(&c, f->title, none, strlen(none), l->px + s->pad, l->empty_y, s->row_h, s->dim,
             l->px, l->px + l->pw, NULL, 0, 0);
    }

    fill(&c, l->px, l->footer_y, l->pw, 1, s->rule);
    left = v->footer ? v->footer : srow ? srow->title : "";
    right_text(&c, f->small, !srow ? "Esc  Close"
               : srow->source == LN_SRC_ACTIONS ? "Return  Run     Esc  Back"
               : srow->source == LN_SRC_CALC ? "Tab  Actions     Return  Copy     Esc  Close"
               : srow->source == LN_SRC_SHELL ? "Tab  Actions     Return  Run     Esc  Close"
               : srow->source == LN_SRC_REXX ? "Return  Send     Esc  Close"
               : srow->source == LN_SRC_PACKAGES ? "Tab  Actions     Return  Install     Esc  Close"
               : "Tab  Actions     Return  Open     Esc  Close",
               l->px + l->pw - s->pad, l->footer_y + 1, s->footer_h, s->dim);
    fitted(&c, f->small, left, l->px + s->pad, l->footer_y + 1, s->footer_h, l->px + l->pw - s->pad - 300,
           v->footer ? s->text : s->dim, NULL, 0);
    free(c.cov);
    return 1;
}
