#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pkglist.h"

static int parse(struct ln_pkglist *l, const char *s) { memset(l, 0, sizeof *l); return ln_pkglist_parse(l, s, strlen(s)); }

int main(void)
{
    struct ln_pkglist l;
    const char *two =
        "{ \"rows\": [\n"
        "  { \"id\": \"zunecalc\", \"version\": \"1.0\", \"revision\": 1, \"summary\": \"A \\\"small\\\" calculator\", "
        "\"category\": \"utility\", \"arch\": \"x86_64\", \"abi\": \"v1\", \"kind\": \"app\", \"size\": 12345, \"ours\": true, \"installed\": false },\n"
        "  { \"id\": \"limpet\", \"version\": \"0.12.1\", \"revision\": 2, \"summary\": \"Terminal \\u017c\\u00f3\\u0142w\", "
        "\"category\": \"system\", \"arch\": \"x86_64\", \"abi\": \"v1\", \"kind\": \"app\", \"size\": 99, \"ours\": true, \"installed\": true }\n"
        "], \"hidden\": 0 }\n";

    assert(parse(&l, two) && l.n == 2);
    assert(!strcmp(l.v[0].id, "zunecalc") && !strcmp(l.v[0].version, "1.0") && !l.v[0].installed);
    assert(!strcmp(l.v[0].summary, "A \"small\" calculator"));
    assert(!strcmp(l.v[1].id, "limpet") && l.v[1].installed);
    assert(!strcmp(l.v[1].summary, "Terminal \xc5\xbc\xc3\xb3\xc5\x82w"));
    ln_pkglist_free(&l);
    assert(l.n == 0 && !l.v);

    /* an empty catalogue is a list */
    assert(parse(&l, "{ \"rows\": [], \"hidden\": 3 }") && l.n == 0);
    /* a row without an id is left out, the others stay */
    assert(parse(&l, "{\"rows\":[{\"version\":\"1\"},{\"id\":\"a\",\"installed\":true}]}") && l.n == 1 && l.v[0].installed && !l.v[0].summary);
    ln_pkglist_free(&l);

    /* not such a list */
    assert(!parse(&l, ""));
    assert(!parse(&l, "apkg: no catalogue; run apkg update\n"));
    assert(!parse(&l, "{ \"error\": \"x\" }"));
    assert(!parse(&l, "{ \"rows\": [ { \"id\": \"cut"));
    assert(!parse(&l, "{ \"rows\": [ { \"id\": \"a\" }"));
    assert(!parse(&l, "{ \"rows\": [ { \"id\": \"a\", \"x\": { } } ] }"));
    assert(!parse(&l, "{ \"rows\": [ { \"id\": \"bad \\u12\" } ] }"));
    assert(!parse(&l, "{ \"rows\": [ 5 ] }"));
    assert(l.n == 0 && !l.v);

    /* many rows */
    {
        char big[40000];
        size_t k = 0;
        int i;
        k += (size_t)snprintf(big + k, sizeof big - k, "{ \"rows\": [");
        for (i = 0; i < 300; i++) k += (size_t)snprintf(big + k, sizeof big - k, "%s{ \"id\": \"p%d\", \"version\": \"1\", \"summary\": \"s\", \"installed\": %s }", i ? "," : "", i, i % 2 ? "true" : "false");
        k += (size_t)snprintf(big + k, sizeof big - k, "] }");
        assert(parse(&l, big) && l.n == 300 && !strcmp(l.v[299].id, "p299") && l.v[299].installed);
        ln_pkglist_free(&l);
    }
    return 0;
}
