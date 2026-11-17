#include <assert.h>
#include <string.h>
#include "../../widgets/org.aros.currency/provider/rates.h"

int main(void)
{
    struct currency_rates r;
    const char *xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<gesmes:Envelope xmlns:gesmes=\"http://www.gesmes.org/xml/2002-08-01\" xmlns=\"http://www.ecb.int/vocabulary/2002-08-01/eurofxref\">\n"
        "<gesmes:subject>Reference rates</gesmes:subject>\n<Cube>\n<Cube time='2026-11-16'>\n"
        "<Cube currency='USD' rate='1.0845'/>\n<Cube currency='JPY' rate='161.57'/>\n"
        "<Cube currency=\"PLN\" rate=\"4.3012\"/>\n<Cube currency='BAD' rate='x'/>\n<Cube currency='TOOLONG' rate='1'/>\n"
        "<Cube currency='NEG' rate='-1'/>\n</Cube>\n</Cube>\n</gesmes:Envelope>\n";
    assert(currency_parse_ecb(xml, strlen(xml), &r));
    assert(!strcmp(r.date, "2026-11-16") && r.n == 3);
    assert(!strcmp(r.code[0], "USD") && r.rate[0] > 1.0844 && r.rate[0] < 1.0846);
    assert(!strcmp(r.code[2], "PLN") && r.rate[2] > 4.3011 && r.rate[2] < 4.3013);
    /* not that file */
    assert(!currency_parse_ecb("", 0, &r));
    assert(!currency_parse_ecb("<html><body>Error</body></html>", 31, &r));
    assert(!currency_parse_ecb("<Cube time='2026-11-16'></Cube>", 31, &r));
    assert(!currency_parse_ecb("<Cube currency='USD' rate='1.08'/>", 34, &r));
    assert(!currency_parse_ecb("<Cube time='2026-11-16'><Cube currency='USD' rate='1.08'", 56, &r));
    return 0;
}
