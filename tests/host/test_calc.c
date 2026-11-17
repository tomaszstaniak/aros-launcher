#include <assert.h>
#include <string.h>
#include "calc.h"

static int is(const char *in, const char *want)
{
    char out[64];
    if (!ln_calc(in, strlen(in), out, sizeof out)) return 0;
    return !strcmp(out, want);
}
static int none(const char *in) { char out[64]; return !ln_calc(in, strlen(in), out, sizeof out); }

int main(void)
{
    assert(is("2+2", "4"));
    assert(is(" 2 + 2 ", "4"));
    assert(is("=2+2", "4"));
    assert(is("2+3*4", "14"));
    assert(is("(2+3)*4", "20"));
    assert(is("10/4", "2.5"));
    assert(is("7%3", "1"));
    assert(is("2^10", "1024"));
    assert(is("2^3^2", "512"));
    assert(is("-5+2", "-3"));
    assert(is("2*-3", "-6"));
    assert(is("-2^2", "-4"));
    assert(is("2(3+1)", "8"));
    assert(is("1.5*4", "6"));
    assert(is("2,5*2", "5"));
    assert(is("0.1+0.2", "0.3"));
    assert(is("1/3", "0.333333333333"));
    assert(is("0x1f+1", "32"));
    assert(is("sqrt(16)", "4"));
    assert(is("sqrt(2)*sqrt(2)", "2"));
    assert(is("sin(pi/2)", "1"));
    assert(is("cos(pi)", "-1"));
    assert(is("sin(pi)", "0"));
    assert(is("ln(e)", "1"));
    assert(is("log(1000)", "3"));
    assert(is("abs(-3)+round(2.5)", "6"));
    assert(is("2 x 3", "6"));
    assert(is("6:3", "2"));
    assert(is("1000000*1000000", "1000000000000"));
    assert(is("2*pi", "6.28318530718"));
    assert(is("pi", "3.14159265359"));

    /* not calculations */
    assert(none(""));
    assert(none("42"));
    assert(none("  3.5 "));
    assert(none("clock"));
    assert(none("edit"));           /* starts with e, is a word */
    assert(none("exchange"));
    assert(none("pip"));
    assert(none("2+"));
    assert(none("2+*3"));
    assert(none("(2+3"));
    assert(none("2+3)"));
    assert(none("2 3"));
    assert(none("sqrt 4"));
    assert(none("0x"));
    assert(none("2+2 apples"));
    assert(none("x"));
    /* no value */
    assert(none("1/0"));
    assert(none("5%0"));
    assert(none("sqrt(-1)"));
    assert(none("ln(0)"));
    assert(none("10^1000"));
    /* deep nesting is refused, not a crash */
    {
        char deep[400];
        memset(deep, '(', 200); memcpy(deep + 200, "1+1", 3); memset(deep + 203, ')', 196); deep[399] = 0;
        assert(none(deep));
        memset(deep, '-', 300); deep[300] = '1'; deep[301] = 0;
        assert(none(deep));
    }
    /* a small output buffer */
    { char out[2]; assert(!ln_calc("5+5", 3, out, sizeof out)); assert(ln_calc("2+2", 3, out, sizeof out)); }
    return 0;
}
