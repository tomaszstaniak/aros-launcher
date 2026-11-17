/* Pixel probes of the panel's picture. LN_TEST_FONT names a TrueType file. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "draw.h"

static unsigned char *pic;
static struct ln_layout l;

static const unsigned char *px(int x, int y) { return pic + ((size_t)y * (size_t)l.w + (size_t)x) * 4; }

static int is(const unsigned char *p, unsigned long c)
{
    return p[1] == ((c >> 16) & 255) && p[2] == ((c >> 8) & 255) && p[3] == (c & 255);
}

/* pixels in a box that have exactly colour c */
static int count(int x0, int y0, int x1, int y1, unsigned long c)
{
    int x, y, n = 0;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) n += is(px(x, y), c);
    return n;
}

static struct ln_row row(unsigned key, const char *title, const char *sub, int score)
{
    struct ln_row w;
    memset(&w, 0, sizeof w);
    w.key = key; w.title = title; w.subtitle = sub; w.kind = "Program"; w.score = score;
    return w;
}

static void draw(const struct ln_style *s, const struct ln_view *v, const struct ln_fonts *f, int first, int shadow)
{
    ln_layout_make(&l, s, v->results, first, shadow);
    free(pic);
    pic = malloc((size_t)l.w * (size_t)l.h * 4);
    assert(pic && ln_draw(pic, &l, s, v, f));
}

int main(void)
{
    const char *font = getenv("LN_TEST_FONT");
    const struct ln_style *s = ln_style_get(1, 0);
    struct ln_fonts f;
    struct ln_results *r = ln_results_new();
    struct ln_view v;
    struct ln_row w[3];
    unsigned char *back;
    unsigned q;
    int n0, n1, x_title, i;

    assert(font);
    memset(&f, 0, sizeof f);
    f.query = zt_font_open(&font, 1, s->font_query);
    f.big = zt_font_open(&font, 1, s->font_big);
    f.title = zt_font_open(&font, 1, s->font_title);
    f.small = zt_font_open(&font, 1, s->font_small);
    assert(f.query && f.title && f.small);

    memset(&v, 0, sizeof v);
    v.results = r;
    q = ln_results_query(r);

    /* no rows, a query: "No results" is written, nothing is selected */
    v.query = "zzz"; v.qn = 3; v.caret = 3;
    draw(s, &v, &f, 0, 0);
    assert(l.empty_y >= 0);
    assert(count(l.px, l.empty_y, l.px + 200, l.empty_y + s->row_h, s->text) == 0);
    {   /* some text is there: pixels that are not the plain body */
        int x, y, ink = 0;
        const unsigned char *bg = px(l.px + l.pw - 40, l.empty_y + 5);
        for (y = l.empty_y; y < l.empty_y + s->row_h; y++)
            for (x = l.px + s->pad; x < l.px + 200; x++) ink += memcmp(px(x, y) + 1, bg + 1, 3) != 0;
        assert(ink > 30);
    }

    w[0] = row(1, "WordCraft", "SYS:Apps/WordCraft", 30);
    w[0].m.n = 2; w[0].m.pos[0] = 0; w[0].m.pos[1] = 1;
    w[1] = row(2, "Password Safe", "SYS:Utilities", 20);
    w[2] = row(3, "A program whose title is far too long to fit into one row of the launcher panel at any width at all", "SYS:Long", 10);
    assert(ln_results_answer(r, q, LN_SRC_PROGRAMS, w, 3));
    v.query = "wo"; v.qn = 2; v.caret = 2;

    /* plain body: alpha 0 outside the rounded corner, the body's own alpha inside */
    draw(s, &v, &f, 0, 0);
    assert(px(0, 0)[0] == 0 && px(l.w - 1, l.h - 1)[0] == 0);
    assert(px(l.w / 2, l.query_y + 3)[0] == ((s->tint_plain >> 24) & 255));

    /* with a backdrop the body is opaque */
    back = malloc((size_t)l.pw * (size_t)l.ph * 4);
    for (i = 0; i < l.pw * l.ph; i++) { back[i * 4] = 255; back[i * 4 + 1] = 40; back[i * 4 + 2] = 90; back[i * 4 + 3] = 160; }
    v.backdrop = back;
    draw(s, &v, &f, 0, 0);
    assert(px(0, 0)[0] == 0 && px(l.w / 2, l.query_y + 3)[0] == 255);
    /* and it shows through the tint: not the tint's own colour */
    assert(!is(px(l.w / 2, l.query_y + 3), s->tint));
    v.backdrop = NULL;

    /* with a shadow: alpha rises towards the panel, the far corner is clear */
    draw(s, &v, &f, 0, 24);
    assert(px(0, 0)[0] == 0);
    assert(px(l.w / 2, l.py + l.ph + 4)[0] > px(l.w / 2, l.py + l.ph + 20)[0]);
    assert(px(l.w / 2, l.py + l.ph + 4)[0] > 0);

    /* the selected row is filled, its neighbour is not */
    draw(s, &v, &f, 0, 0);
    assert(memcmp(px(l.px + l.pw - 100, l.row_y[0] + 4), px(l.px + l.pw - 100, l.row_y[1] + 4), 4) != 0);
    assert(memcmp(px(l.px + l.pw - 100, l.row_y[1] + 4), px(l.px + l.pw - 100, l.row_y[2] + 4), 4) == 0);

    /* matched characters take the match colour: inside the first two
       characters of the first title, not after them */
    x_title = l.px + s->pad + s->icon + 12;
    n0 = (int)zt_x(f.title, "WordCraft", 9, 2, 0);
    /* only pixels the glyph covers fully have exactly that colour: a few */
    assert(count(x_title, l.row_y[0], x_title + n0, l.row_y[0] + s->row_h / 2 + 3, s->sel_match) > 2);
    assert(count(x_title + n0 + 1, l.row_y[0], x_title + 200, l.row_y[0] + s->row_h / 2 + 3, s->sel_match) == 0);
    /* the second row has no match marks */
    assert(count(x_title, l.row_y[1], x_title + 200, l.row_y[1] + s->row_h / 2 + 3, s->match) == 0);

    /* a title that is too long stops before the kind word */
    {
        long kw = zt_measure(f.small, "Program", 7, 0);
        int limit = l.px + l.pw - s->pad - (int)kw - 12, x, y, ink = 0;
        const unsigned char *bg = px(limit + 4, l.row_y[2] + 2);
        for (y = l.row_y[2] + 2; y < l.row_y[2] + s->row_h - 2; y++)
            for (x = limit; x < limit + 10; x++) ink += memcmp(px(x, y), bg, 4) != 0;
        assert(ink == 0);
    }

    /* the caret: drawn in its visible phase only */
    v.caret_on = 0;
    draw(s, &v, &f, 0, 0);
    n0 = count(l.px, l.query_y, l.px + l.pw, l.query_y + s->query_h, s->text);
    v.caret_on = 1;
    draw(s, &v, &f, 0, 0);
    n1 = count(l.px, l.query_y, l.px + l.pw, l.query_y + s->query_h, s->text);
    assert(n1 > n0 + 20);

    /* a selection in the query colours the box behind the selected text */
    v.caret_on = 0; v.sel_start = 0; v.sel_end = 2;
    draw(s, &v, &f, 0, 0);
    {
        int x0 = l.px + s->pad, x1 = x0 + (int)zt_x(f.query, "wo", 2, 2, 0);
        const unsigned char *in = px(x0 + 1, l.query_y + s->query_h / 2 + 9), *outp = px(x1 + 6, l.query_y + s->query_h / 2 + 9);
        assert(memcmp(in, outp, 4) != 0);
    }

    /* the footer message replaces the selected title */
    v.sel_start = v.sel_end = 0;
    v.footer = "Could not start WordCraft";
    draw(s, &v, &f, 0, 0);
    assert(count(l.px, l.footer_y + 1, l.px + 300, l.footer_y + s->footer_h, s->text) > 0);

    /* with the cache the picture is the same, pixel for pixel, also after
       the selection moved and after the size changed */
    {
        struct ln_draw_cache cache;
        unsigned char *plain;
        size_t bytes;
        int round;
        memset(&cache, 0, sizeof cache);
        v.footer = NULL;
        for (round = 0; round < 4; round++) {
            if (round == 1) ln_results_move(r, 1);
            if (round == 3) { unsigned q2 = ln_results_query(r); assert(ln_results_answer(r, q2, LN_SRC_PROGRAMS, w, 1)); }
            ln_layout_make(&l, s, r, 0, 24);
            bytes = (size_t)l.w * (size_t)l.h * 4;
            plain = malloc(bytes);
            free(pic); pic = malloc(bytes);
            assert(ln_draw(plain, &l, s, &v, &f));
            assert(ln_draw_cached(pic, &l, s, &v, &f, &cache));
            assert(!memcmp(plain, pic, bytes));
            if (round == 2) { assert(ln_draw_cached(pic, &l, s, &v, &f, &cache)); assert(!memcmp(plain, pic, bytes)); }
            free(plain);
        }
        ln_draw_cache_reset(&cache);
    }

    /* the other looks draw without touching memory they should not (ASan) */
    draw(ln_style_get(1, 1), &v, &f, 0, 16);
    draw(ln_style_get(0, 0), &v, &f, 0, 16);
    draw(ln_style_get(0, 1), &v, &f, 0, 0);

    free(pic); free(back);
    zt_font_close(f.query); zt_font_close(f.title); zt_font_close(f.small); zt_font_close(f.big);
    ln_results_free(r);
    return 0;
}
