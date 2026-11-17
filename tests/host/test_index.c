#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "index.h"

static int q(struct ln_index *x, const char *s, const struct ln_entry **out, int max)
{
    return ln_index_query(x, s, strlen(s), out, NULL, max);
}

int main(void)
{
    struct ln_index *x = ln_index_new(), *y;
    const struct ln_entry *out[8];
    struct ln_match m[8];
    unsigned a, b, c;
    char *text;
    size_t n;
    int k;

    /* the same path in two spellings is one entry */
    a = ln_index_put(x, "SYS:Tools/Editor", "Editor");
    b = ln_index_put(x, "sys:tools/EDITOR", "Editor");
    assert(a && a == b && ln_index_count(x) == 1);

    b = ln_index_put(x, "SYS:Tools/Edit", "Edit");
    c = ln_index_put(x, "SYS:Utilities/Clock", "Clock");
    assert(ln_index_count(x) == 3);

    /* a rescan drops what vanished and keeps the neighbours */
    ln_index_begin(x, "SYS:Tools");
    ln_index_put(x, "SYS:Tools/Edit", "Edit");
    ln_index_end(x, "SYS:Tools");
    assert(ln_index_count(x) == 2);
    assert(!ln_index_find(x, a) && ln_index_find(x, b) && ln_index_find(x, c));
    /* "SYS:Tools" does not cover "SYS:ToolsExtra/..." */
    ln_index_put(x, "SYS:ToolsExtra/Thing", "Thing");
    ln_index_begin(x, "SYS:Tools");
    ln_index_put(x, "SYS:Tools/Edit", "Edit");
    ln_index_end(x, "SYS:Tools");
    assert(ln_index_count(x) == 3);
    /* a volume as the folder */
    ln_index_begin(x, "SYS:");
    ln_index_put(x, "SYS:Tools/Edit", "Edit");
    ln_index_end(x, "SYS:");
    assert(ln_index_count(x) == 1);

    /* ranking: uses lift an equal match */
    a = ln_index_put(x, "SYS:Tools/Editor", "Editor");
    c = ln_index_put(x, "SYS:Tools/Editar", "Editar");
    k = ln_index_query(x, "edita", 5, out, m, 8);
    assert(k == 1 && out[0]->id == c && m[0].n == 5);
    k = q(x, "edit", out, 8);
    assert(k == 3 && out[0]->id == b);          /* shortest title first */
    for (k = 0; k < 5; k++) ln_index_used(x, a, 100 + (unsigned)k);
    k = q(x, "edit", out, 8);
    assert(k == 3 && out[0]->id == a);
    /* max is respected and keeps the best */
    k = q(x, "edit", out, 1);
    assert(k == 1 && out[0]->id == a);

    /* empty query: most used first */
    ln_index_used(x, c, 200);
    k = q(x, "", out, 8);
    assert(k == 3 && out[0]->id == a && out[1]->id == c && out[2]->id == b);

    /* nothing matches */
    assert(q(x, "zzz", out, 8) == 0);

    /* counts survive saving and loading, also for programs not yet seen */
    text = ln_index_save_counts(x, &n);
    assert(text && n == strlen(text));
    y = ln_index_new();
    assert(ln_index_load_counts(y, text, n) == 2);
    assert(ln_index_count(y) == 0 && q(y, "", out, 8) == 0);
    ln_index_put(y, "SYS:Tools/Edit", "Edit");
    a = ln_index_put(y, "SYS:TOOLS/Editor", "Editor");
    k = q(y, "edit", out, 8);
    assert(k == 2 && out[0]->id == a && out[0]->uses == 5 && out[0]->last_use == 104);
    free(text);

    /* a removed program keeps its counts for when it returns */
    ln_index_remove(y, "SYS:Tools/Editor");
    assert(q(y, "editor", out, 8) == 0);
    a = ln_index_put(y, "SYS:Tools/Editor", "Editor");
    assert(ln_index_find(y, a)->uses == 5);

    /* lines that do not parse are skipped, the rest loads */
    {
        const char *bad = "x\ty\tSYS:A\n3\t7\tSYS:B\nno tabs here\n4\t\tSYS:C\n\n2\t9\tSYS:D";
        struct ln_index *z = ln_index_new();
        assert(ln_index_load_counts(z, bad, strlen(bad)) == 2);
        a = ln_index_put(z, "SYS:D", "D");
        assert(ln_index_find(z, a)->uses == 2 && ln_index_find(z, a)->last_use == 9);
        ln_index_free(z);
    }
    ln_index_free(x);
    ln_index_free(y);
    return 0;
}
