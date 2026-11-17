/* SPDX-License-Identifier: MIT */
#include "calc.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

struct parse {
    const char *s, *end;
    int ops;        /* operators and functions met: a number alone is no calculation */
    int bad;
    int depth;
};

static double expr(struct parse *p);

static void skip(struct parse *p)
{
    while (p->s < p->end && (*p->s == ' ' || *p->s == '\t')) p->s++;
}

static int word(struct parse *p, const char *w)
{
    size_t n = strlen(w);
    if ((size_t)(p->end - p->s) < n || memcmp(p->s, w, n)) return 0;
    /* a longer name is another word */
    if (p->s + n < p->end && ((p->s[n] >= 'a' && p->s[n] <= 'z') || (p->s[n] >= 'A' && p->s[n] <= 'Z'))) return 0;
    p->s += n;
    return 1;
}

static double number(struct parse *p)
{
    double v = 0, scale = 1;
    int digits = 0;
    if (p->end - p->s > 2 && p->s[0] == '0' && (p->s[1] == 'x' || p->s[1] == 'X')) {
        p->s += 2;
        for (; p->s < p->end; p->s++, digits++) {
            int c = *p->s, d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if ((c | 32) >= 'a' && (c | 32) <= 'f') d = (c | 32) - 'a' + 10;
            else break;
            v = v * 16 + d;
        }
        if (!digits) p->bad = 1;
        return v;
    }
    for (; p->s < p->end && *p->s >= '0' && *p->s <= '9'; p->s++, digits++) v = v * 10 + (*p->s - '0');
    if (p->s < p->end && (*p->s == '.' || *p->s == ',')) {
        p->s++;
        for (; p->s < p->end && *p->s >= '0' && *p->s <= '9'; p->s++, digits++) { scale /= 10; v += (*p->s - '0') * scale; }
    }
    if (!digits) p->bad = 1;
    return v;
}

static double primary(struct parse *p)
{
    static const struct { const char *name; int id; } fn[] = {
        { "sqrt", 0 }, { "sin", 1 }, { "cos", 2 }, { "tan", 3 }, { "ln", 4 }, { "log", 5 }, { "abs", 6 }, { "round", 7 }
    };
    size_t i;
    double v;
    skip(p);
    if (p->s >= p->end) { p->bad = 1; return 0; }
    if (*p->s == '(') {
        if (++p->depth > 64) { p->bad = 1; return 0; }
        p->s++;
        v = expr(p);
        skip(p);
        if (p->s < p->end && *p->s == ')') p->s++; else p->bad = 1;
        p->depth--;
        return v;
    }
    for (i = 0; i < sizeof fn / sizeof fn[0]; i++)
        if (word(p, fn[i].name)) {
            skip(p);
            if (p->s >= p->end || *p->s != '(') { p->bad = 1; return 0; }
            p->ops++;
            v = primary(p);
            switch (fn[i].id) {
            case 0: return sqrt(v);
            case 1: return sin(v);
            case 2: return cos(v);
            case 3: return tan(v);
            case 4: return log(v);
            case 5: return log10(v);
            case 6: return fabs(v);
            default: return floor(v + 0.5);
            }
        }
    if (word(p, "pi")) { p->ops++; return 3.14159265358979323846; }
    if (word(p, "e")) { p->ops++; return 2.71828182845904523536; }
    return number(p);
}

static double unary(struct parse *p);

static double power(struct parse *p)
{
    double v = primary(p);
    skip(p);
    if (!p->bad && p->s < p->end && *p->s == '^') {
        p->s++; p->ops++;
        v = pow(v, unary(p));       /* right to left: 2^3^2 is 2^9 */
    }
    return v;
}

static double unary(struct parse *p)
{
    skip(p);
    if (p->s < p->end && *p->s == '-') {
        if (++p->depth > 64) { p->bad = 1; return 0; }
        p->s++; p->ops++;
        { double v = -unary(p); p->depth--; return v; }
    }
    if (p->s < p->end && *p->s == '+') { p->s++; return unary(p); }
    return power(p);
}

static double term(struct parse *p)
{
    double v = unary(p);
    for (;;) {
        skip(p);
        if (p->bad || p->s >= p->end) return v;
        if (*p->s == '*' || *p->s == 'x' || *p->s == 'X') { p->s++; p->ops++; v *= unary(p); }
        else if (*p->s == '/' || *p->s == ':') {
            double d;
            p->s++; p->ops++;
            d = unary(p);
            if (d == 0) p->bad = 1; else v /= d;
        } else if (*p->s == '%') {
            double d;
            p->s++; p->ops++;
            d = unary(p);
            if (d == 0) p->bad = 1; else v = fmod(v, d);
        } else if (*p->s == '(') { p->ops++; v *= primary(p); }     /* 2(3+1) */
        else return v;
    }
}

static double expr(struct parse *p)
{
    double v = term(p);
    for (;;) {
        skip(p);
        if (p->bad || p->s >= p->end) return v;
        if (*p->s == '+') { p->s++; p->ops++; v += term(p); }
        else if (*p->s == '-') { p->s++; p->ops++; v -= term(p); }
        else return v;
    }
}

int ln_calc(const char *text, size_t n, char *out, size_t cap)
{
    struct parse p;
    double v;
    char buf[48];
    size_t len;
    p.s = text; p.end = text + n; p.ops = 0; p.bad = 0; p.depth = 0;
    /* "=2+2" as in a spreadsheet */
    skip(&p);
    if (p.s < p.end && *p.s == '=') p.s++;
    v = expr(&p);
    skip(&p);
    if (p.bad || p.s != p.end || !p.ops) return 0;
    if (v != v || v - v != 0) return 0;         /* not a number, or infinite */
    if (fabs(v) < 1e-12) v = 0;
    if (v == floor(v) && fabs(v) < 1e15) snprintf(buf, sizeof buf, "%.0f", v);
    else snprintf(buf, sizeof buf, "%.12g", v);
    if (!strcmp(buf, "-0")) strcpy(buf, "0");
    len = strlen(buf);
    if (len + 1 > cap) return 0;
    memcpy(out, buf, len + 1);
    return 1;
}
