#include <assert.h>
#include <string.h>
#include "match.h"

static int go(const char *q, const char *t, struct ln_match *m)
{
    return ln_match(q, strlen(q), t, strlen(t), m);
}

int main(void)
{
    struct ln_match a, b;
    char longt[400], longq[200];
    int i;

    assert(go("word", "WordCraft", &a));
    assert(a.n == 4 && a.pos[0] == 0 && a.pos[1] == 1 && a.pos[2] == 2 && a.pos[3] == 3);

    assert(go("wc", "WordCraft", &a));
    assert(a.n == 2 && a.pos[0] == 0 && a.pos[1] == 4);
    assert(go("wc", "Showcase", &b));
    assert(a.score > b.score);

    /* "Żółw" is Ż(2) ó(2) ł(2) w(1) bytes */
    assert(go("zolw", "\xc5\xbb\xc3\xb3\xc5\x82w", &a));
    assert(a.n == 4 && a.pos[0] == 0 && a.pos[1] == 2 && a.pos[2] == 4 && a.pos[3] == 6);
    /* and the other way round: accents typed, plain title */
    assert(go("\xc5\xbc\xc3\xb3\xc5\x82w", "Zolw", &a));

    assert(!go("xyz", "WordCraft", &a));
    assert(!go("wordcraftx", "WordCraft", &a));

    assert(go("", "Anything", &a) && a.score == 0 && a.n == 0);
    assert(go("  ", "Anything", &a) && a.n == 0);
    /* spaces in the query are ignored */
    assert(go("pass safe", "Password Safe", &a) && a.n == 8);

    /* a word start beats the middle of a word */
    assert(go("s", "Password Safe", &a));
    assert(go("s", "Pas", &b));
    assert(a.pos[0] == 9);
    (void)b;

    /* a run beats scattered letters */
    assert(go("craft", "WordCraft", &a));
    assert(go("craft", "Wxcxrxaxfxt", &b));
    assert(a.score > b.score);

    /* shorter titles win ties */
    assert(go("edit", "Editor", &a));
    assert(go("edit", "Editor of long descriptive names", &b));
    assert(a.score > b.score);

    /* a command named by its first word beats a program that only has the
       letters; a program named from its start beats a command's second word */
    assert(go("quit", "Quit Launcher", &a));
    assert(go("quit", "QuickPart", &b));
    assert(a.score > b.score);
    assert(go("se", "Serial", &a));
    assert(go("se", "Launcher Settings", &b));
    assert(a.score > b.score);

    /* limits: long title, long query */
    memset(longt, 'a', sizeof longt - 1); longt[sizeof longt - 1] = 0;
    memset(longq, 'a', sizeof longq - 1); longq[sizeof longq - 1] = 0;
    assert(go(longq, longt, &a));
    assert(a.n == LN_MATCH_MAX);
    for (i = 0; i < a.n; i++) assert(a.pos[i] < 256);

    /* not UTF-8 */
    assert(!go("\xff", "abc", &a));
    assert(!go("a", "\xff", &a));
    return 0;
}
