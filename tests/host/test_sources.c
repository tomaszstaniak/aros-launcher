/* Several sources in one list, as the program fills it: calculator,
 * programs, the launcher's commands. LN_TEST_FONT names a TrueType file. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "calc.h"
#include "draw.h"

static struct ln_row row(unsigned key, const char *title, const char *sub, const char *kind, int score)
{
    struct ln_row w;
    memset(&w, 0, sizeof w);
    w.key = key; w.title = title; w.subtitle = sub; w.kind = kind; w.score = score;
    return w;
}

static void draw(const struct ln_style *s, struct ln_results *r, const struct ln_fonts *f, const char *q)
{
    struct ln_layout l;
    struct ln_view v;
    unsigned char *pic;
    int mw, mh;
    memset(&v, 0, sizeof v);
    v.results = r; v.query = q; v.qn = strlen(q); v.caret = v.qn;
    ln_layout_max(s, 28, &mw, &mh);
    ln_layout_make(&l, s, r, 0, 28);
    assert(l.w == mw && l.h <= mh);
    pic = malloc((size_t)mw * (size_t)mh * 4);
    assert(pic && ln_draw(pic, &l, s, &v, f));
    free(pic);
}

int main(void)
{
    const char *font = getenv("LN_TEST_FONT");
    struct ln_fonts f;
    struct ln_results *r = ln_results_new();
    struct ln_row w[8];
    char value[64];
    unsigned q;
    int look, i;

    assert(font);
    for (look = 0; look < 4; look++) {
        const struct ln_style *s = ln_style_get(look & 1, look >> 1);
        f.query = zt_font_open(&font, 1, s->font_query);
        f.title = zt_font_open(&font, 1, s->font_title);
        f.small = zt_font_open(&font, 1, s->font_small);
        assert(f.query && f.title && f.small);

        /* "sett": no calculation, no program, one command */
        q = ln_results_query(r);
        assert(ln_results_answer(r, q, LN_SRC_CALC, w, 0));
        w[0] = row(1, "Launcher Settings", "Folders, look, start with the system", "Command", 50);
        w[0].m.n = 4; w[0].m.pos[0] = 9; w[0].m.pos[1] = 10; w[0].m.pos[2] = 11; w[0].m.pos[3] = 12;
        assert(ln_results_answer(r, q, LN_SRC_COMMANDS, w, 1));
        assert(ln_results_answer(r, q, LN_SRC_PROGRAMS, w, 0));
        assert(ln_results_count(r) == 1 && ln_results_selected(r) == 0);
        assert(ln_results_row(r, 0)->source == LN_SRC_COMMANDS);
        draw(s, r, &f, "sett");

        /* a calculation above programs and a command */
        q = ln_results_query(r);
        assert(ln_calc("2+2", 3, value, sizeof value));
        w[0] = row(1, value, "2+2", "Result", 1);
        assert(ln_results_answer(r, q, LN_SRC_CALC, w, 1));
        assert(ln_results_answer(r, q, LN_SRC_COMMANDS, w, 0));
        for (i = 0; i < 8; i++) w[i] = row((unsigned)i + 10, "Program", "SYS:Tools", "Program", 100 - i);
        assert(ln_results_answer(r, q, LN_SRC_PROGRAMS, w, 8));
        assert(ln_results_count(r) == 9 && ln_results_row(r, 0)->source == LN_SRC_CALC);
        assert(ln_results_selected(r) == 0);        /* the calculator's row stays chosen */
        draw(s, r, &f, "2+2");
        /* the last row chosen: it is on show, whatever the headings take */
        ln_results_move(r, 8);
        {
            struct ln_layout l;
            ln_layout_make(&l, s, r, 0, 0);
            assert(l.shown >= 1 && l.row_index[l.shown - 1] == 8);
        }
        draw(s, r, &f, "2+2");

        /* all three sections at once */
        w[0] = row(3, "Quit Launcher", "Stop the launcher until it is started again", "Command", 5);
        assert(ln_results_answer(r, q, LN_SRC_COMMANDS, w, 1));
        assert(ln_results_count(r) == 10);
        draw(s, r, &f, "2+2");

        zt_font_close(f.query); zt_font_close(f.title); zt_font_close(f.small);
    }
    ln_results_free(r);
    return 0;
}
