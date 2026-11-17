#include <assert.h>
#include <string.h>
#include "results.h"

static struct ln_row row(unsigned key, const char *title, int score)
{
    struct ln_row w;
    memset(&w, 0, sizeof w);
    w.key = key; w.title = title; w.score = score;
    return w;
}

static unsigned sel_key(struct ln_results *r)
{
    return ln_results_row(r, ln_results_selected(r))->key;
}

int main(void)
{
    struct ln_results *r = ln_results_new();
    struct ln_row a[4], b[4];
    unsigned q1, q2;
    enum { OTHER = 5, EARLY = 0 + 1 };
    (void)EARLY;

    assert(ln_results_selected(r) == -1 && ln_results_count(r) == 0);

    q1 = ln_results_query(r);
    q2 = ln_results_query(r);
    assert(q1 != q2);

    /* an answer for an earlier query changes nothing */
    a[0] = row(10, "Old", 5);
    assert(!ln_results_answer(r, q1, LN_SRC_PROGRAMS, a, 1));
    assert(ln_results_count(r) == 0 && ln_results_selected(r) == -1);

    /* rows are ordered by score inside a source; titles are copies */
    {
        char t[8] = "Beta";
        a[0] = row(1, "Alpha", 10); a[1] = row(2, t, 30); a[2] = row(3, "Gamma", 20);
        assert(ln_results_answer(r, q2, OTHER, a, 3));
        t[0] = 'X';
    }
    assert(ln_results_count(r) == 3);
    assert(!strcmp(ln_results_row(r, 0)->title, "Beta") && ln_results_row(r, 1)->key == 3);
    assert(ln_results_selected(r) == 0 && sel_key(r) == 2);

    /* a source that sorts earlier answers later: the untouched selection
       stays on the row it was on */
    b[0] = row(7, "Program", 99);
    assert(ln_results_answer(r, q2, LN_SRC_PROGRAMS, b, 1));
    assert(ln_results_count(r) == 4 && ln_results_row(r, 0)->key == 7);
    assert(sel_key(r) == 2 && ln_results_selected(r) == 1);

    /* after the user moved, a re-answer inserting a row above keeps the key */
    ln_results_move(r, +1);
    assert(sel_key(r) == 3);
    b[0] = row(7, "Program", 99); b[1] = row(8, "Program two", 50);
    assert(ln_results_answer(r, q2, LN_SRC_PROGRAMS, b, 2));
    assert(sel_key(r) == 3 && ln_results_selected(r) == 3);

    /* the selected row vanishing selects the row now at its position */
    a[0] = row(1, "Alpha", 10); a[1] = row(2, "Beta", 30);
    assert(ln_results_answer(r, q2, OTHER, a, 2));
    assert(ln_results_selected(r) == 3 && sel_key(r) == 1);
    /* ... or the last row when the list got shorter */
    assert(ln_results_answer(r, q2, OTHER, a, 0));
    assert(ln_results_count(r) == 2 && ln_results_selected(r) == 1 && sel_key(r) == 8);

    /* moving stops at the ends */
    ln_results_move(r, +10);
    assert(ln_results_selected(r) == 1);
    ln_results_move(r, -10);
    assert(ln_results_selected(r) == 0);

    /* all rows gone */
    assert(ln_results_answer(r, q2, LN_SRC_PROGRAMS, b, 0));
    assert(ln_results_count(r) == 0 && ln_results_selected(r) == -1);
    ln_results_move(r, 1);
    assert(ln_results_selected(r) == -1);

    /* a new query forgets the selection */
    assert(ln_results_answer(r, q2, LN_SRC_PROGRAMS, b, 2));
    ln_results_move(r, +1);
    q1 = ln_results_query(r);
    assert(ln_results_count(r) == 0);
    assert(ln_results_answer(r, q1, LN_SRC_PROGRAMS, b, 2));
    assert(ln_results_selected(r) == 0);

    ln_results_free(r);
    return 0;
}
