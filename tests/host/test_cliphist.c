#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/cliphist.h"

int main(void)
{
    struct ln_cliphist h;
    char line[32], big[16];
    unsigned first;
    int i;
    memset(&h, 0, sizeof h);
    assert(!ln_cliphist_add(&h, "", 0, 1) && !ln_cliphist_add(&h, "  \n\t", 4, 1) && h.n == 0);
    assert(ln_cliphist_add(&h, "one", 3, 1) && ln_cliphist_add(&h, "two", 3, 2) && h.n == 2);
    assert(!strcmp(h.v[0].text, "two") && h.v[0].id != h.v[1].id && h.v[0].id && h.v[1].id);
    first = h.v[1].id;
    /* the newest again changes nothing but its time; an older one moves up and keeps its id */
    assert(!ln_cliphist_add(&h, "two", 3, 3) && h.v[0].when == 3);
    assert(ln_cliphist_add(&h, "one", 3, 4) && h.n == 2 && !strcmp(h.v[0].text, "one") && h.v[0].id == first && h.v[0].when == 4);
    assert(ln_cliphist_find(&h, first) == &h.v[0] && !ln_cliphist_find(&h, 9999));
    ln_cliphist_remove(&h, first);
    assert(h.n == 1 && !strcmp(h.v[0].text, "two"));
    ln_cliphist_remove(&h, first);
    assert(h.n == 1);

    assert(ln_cliphist_add(&h, "\n  first\tline  here\nsecond", 26, 5));
    ln_cliphist_title(&h.v[0], line, sizeof line);
    assert(!strcmp(line, "first line here"));
    ln_cliphist_title(&h.v[0], big, 8);
    assert(!strcmp(big, "first l"));
    /* a cut never splits a UTF-8 character: z, a with ogonek (2 bytes), ... */
    assert(ln_cliphist_add(&h, "zz\xc4\x85\xc4\x85", 6, 6));
    ln_cliphist_title(&h.v[0], big, 4);
    assert(!strcmp(big, "zz"));
    ln_cliphist_title(&h.v[0], big, 5);
    assert(!strcmp(big, "zz\xc4\x85"));

    ln_cliphist_clear(&h);
    assert(h.n == 0);
    for (i = 0; i < LN_CLIP_MAX + 5; i++) {
        char t[16];
        snprintf(t, sizeof t, "text %d", i);
        assert(ln_cliphist_add(&h, t, strlen(t), (unsigned)i));
    }
    assert(h.n == LN_CLIP_MAX && !strcmp(h.v[0].text, "text 54") && !strcmp(h.v[LN_CLIP_MAX - 1].text, "text 5"));
    {
        char *huge = malloc(LN_CLIP_TEXT_MAX + 1);
        assert(huge);
        memset(huge, 'x', LN_CLIP_TEXT_MAX + 1);
        assert(!ln_cliphist_add(&h, huge, LN_CLIP_TEXT_MAX + 1, 9) && ln_cliphist_add(&h, huge, LN_CLIP_TEXT_MAX, 9));
        free(huge);
    }
    ln_cliphist_clear(&h);
    {
        /* a word in a later line is found, and that line is shown */
        struct ln_cliphist h2;
        char line[64];
        const char *t = "Dear Anna,\n\nthe Invoice 2026-11 is attached.\nRegards";
        memset(&h2, 0, sizeof h2);
        assert(ln_cliphist_add(&h2, t, strlen(t), 1));
        assert(ln_cliphist_search(&h2.v[0], "invoice", 7, line, sizeof line));
        assert(!strcmp(line, "the Invoice 2026-11 is attached."));
        assert(ln_cliphist_search(&h2.v[0], "dear", 4, line, sizeof line) && !strcmp(line, "Dear Anna,"));
        assert(!ln_cliphist_search(&h2.v[0], "missing", 7, line, sizeof line));
        assert(!ln_cliphist_search(&h2.v[0], "", 0, line, sizeof line));
        ln_cliphist_clear(&h2);
    }
    {
        /* pictures: kept byte for byte, named by their size, the oldest leave for the budget */
        struct ln_cliphist h3;
        char title[40], line[20];
        size_t big = 30u * 1024u * 1024u;
        char *a = malloc(big), *b = malloc(big), *c = malloc(big);
        memset(&h3, 0, sizeof h3);
        assert(a && b && c);
        memset(a, 1, big); memset(b, 2, big); memset(c, 3, big);
        assert(ln_cliphist_add(&h3, "note", 4, 1));
        assert(ln_cliphist_add_picture(&h3, a, big, 1920, 1080, 24, 2) && h3.n == 2);
        ln_cliphist_title(&h3.v[0], title, sizeof title);
        assert(!strcmp(title, "Picture 1920 x 1080"));
        assert(h3.v[0].kind == LN_CLIP_PICTURE && h3.v[0].n == big && !memcmp(h3.v[0].text, a, big));
        assert(!ln_cliphist_search(&h3.v[0], "\1\1", 2, line, sizeof line));
        assert(!ln_cliphist_add_picture(&h3, a, big, 1920, 1080, 24, 3));           /* the newest again */
        assert(ln_cliphist_add_picture(&h3, b, big, 10, 10, 24, 4) && h3.n == 3);
        assert(ln_cliphist_add_picture(&h3, c, big, 10, 10, 24, 5));                /* 90 MB would not fit */
        assert(h3.n == 2 && h3.bytes == 2 * big && h3.v[0].text[0] == 3 && h3.v[1].text[0] == 2);
        assert(!ln_cliphist_add_picture(&h3, a, LN_CLIP_PICTURE_MAX + 1u, 1, 1, 24, 6));
        ln_cliphist_clear(&h3);
        assert(h3.bytes == 0);
        free(a); free(b); free(c);
    }
    return 0;
}
