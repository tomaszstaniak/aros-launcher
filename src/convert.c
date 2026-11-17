/* SPDX-License-Identifier: MIT */
#include "convert.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* --- text helpers -------------------------------------------------------- */

static int space(int c) { return c == ' ' || c == '\t'; }

/* A lower-case copy of text without the degree sign and with single spaces. */
static size_t tidy(const char *text, size_t n, char *out, size_t cap)
{
    size_t i, k = 0;
    int gap = 0;
    for (i = 0; i < n && k + 1 < cap; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == 0xc2 && i + 1 < n && (unsigned char)text[i + 1] == 0xb0) { i++; continue; }
        if (space(c)) { gap = k > 0; continue; }
        if (gap) { out[k++] = ' '; gap = 0; if (k + 1 >= cap) break; }
        out[k++] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
    }
    out[k] = 0;
    return k;
}

/* A number at *s ("12", "2.5", "2,5", "-3"); moves *s past it. */
static int number(const char **s, double *v)
{
    const char *p = *s;
    double x = 0, scale = 1;
    int digits = 0, neg = 0;
    if (*p == '-') { neg = 1; p++; }
    for (; *p >= '0' && *p <= '9'; p++, digits++) x = x * 10 + (*p - '0');
    if ((*p == '.' || *p == ',') && p[1] >= '0' && p[1] <= '9')
        for (p++; *p >= '0' && *p <= '9'; p++, digits++) { scale /= 10; x += (*p - '0') * scale; }
    if (!digits) return 0;
    *v = neg ? -x : x;
    *s = p;
    return 1;
}

static void fmt(double v, char *out, size_t cap)
{
    if (fabs(v) < 1e-12) v = 0;
    if (v == floor(v) && fabs(v) < 1e15) snprintf(out, cap, "%.0f", v);
    else snprintf(out, cap, "%.6g", v);
}

/* whether word (length wn) is one of the names in "a|b|c" */
static int named(const char *names, const char *word, size_t wn)
{
    while (*names) {
        const char *e = strchr(names, '|');
        size_t len = e ? (size_t)(e - names) : strlen(names);
        if (len == wn && !memcmp(names, word, wn)) return 1;
        names += len + (e ? 1 : 0);
    }
    return 0;
}

/* --- units --------------------------------------------------------------- */

enum { LENGTH, MASS, TEMP, VOLUME, AREA, SPEED, DATA, TIME };

struct unit {
    const char *names;      /* the first is what is shown */
    int kind;
    double factor;          /* to the kind's base unit */
    const char *other;      /* what it converts to when nothing is named */
};

static const struct unit units[] = {
    { "mm|millimeter|millimeters|millimetre|millimetres", LENGTH, 0.001, "in" },
    { "cm|centimeter|centimeters|centimetre|centimetres", LENGTH, 0.01, "in" },
    { "m|meter|meters|metre|metres", LENGTH, 1, "ft" },
    { "km|kilometer|kilometers|kilometre|kilometres", LENGTH, 1000, "mi" },
    { "in|inch|inches|\"", LENGTH, 0.0254, "cm" },
    { "ft|foot|feet|'", LENGTH, 0.3048, "m" },
    { "yd|yard|yards", LENGTH, 0.9144, "m" },
    { "mi|mile|miles", LENGTH, 1609.344, "km" },
    { "nmi|nautical mile|nautical miles", LENGTH, 1852, "km" },
    { "mg|milligram|milligrams", MASS, 0.000001, "g" },
    { "g|gram|grams", MASS, 0.001, "oz" },
    { "kg|kilogram|kilograms|kilo|kilos", MASS, 1, "lb" },
    { "t|tonne|tonnes|ton|tons", MASS, 1000, "lb" },
    { "oz|ounce|ounces", MASS, 0.028349523125, "g" },
    { "lb|lbs|pound|pounds", MASS, 0.45359237, "kg" },
    { "st|stone|stones", MASS, 6.35029318, "kg" },
    { "c|celsius", TEMP, 0, "f" },
    { "f|fahrenheit", TEMP, 0, "c" },
    { "k|kelvin", TEMP, 0, "c" },
    { "ml|milliliter|milliliters|millilitre|millilitres", VOLUME, 0.001, "fl oz" },
    { "l|liter|liters|litre|litres", VOLUME, 1, "gal" },
    { "fl oz|floz|fluid ounce|fluid ounces", VOLUME, 0.0295735295625, "ml" },
    { "pt|pint|pints", VOLUME, 0.473176473, "l" },
    { "gal|gallon|gallons", VOLUME, 3.785411784, "l" },
    { "m2|sqm|square meter|square meters|square metre|square metres", AREA, 1, "ft2" },
    { "km2|square kilometer|square kilometers|square kilometre|square kilometres", AREA, 1000000, "ha" },
    { "ha|hectare|hectares", AREA, 10000, "acres" },
    { "ft2|sqft|square foot|square feet", AREA, 0.09290304, "m2" },
    { "acres|acre", AREA, 4046.8564224, "ha" },
    { "km/h|kmh|kph", SPEED, 1 / 3.6, "mph" },
    { "mph", SPEED, 0.44704, "km/h" },
    { "m/s", SPEED, 1, "km/h" },
    { "kn|knot|knots", SPEED, 1852.0 / 3600, "km/h" },
    { "b|byte|bytes", DATA, 1, "kb" },
    { "kb|kilobyte|kilobytes", DATA, 1e3, "mb" },
    { "mb|megabyte|megabytes", DATA, 1e6, "gb" },
    { "gb|gigabyte|gigabytes", DATA, 1e9, "mb" },
    { "tb|terabyte|terabytes", DATA, 1e12, "gb" },
    { "kib", DATA, 1024, "kb" },
    { "mib", DATA, 1048576, "mb" },
    { "gib", DATA, 1073741824, "gb" },
    { "s|sec|second|seconds", TIME, 1, "min" },
    { "min|minute|minutes", TIME, 60, "h" },
    { "h|hr|hour|hours", TIME, 3600, "min" },
    { "d|day|days", TIME, 86400, "h" },
    { "wk|week|weeks", TIME, 604800, "d" },
};
#define UNITS (sizeof units / sizeof units[0])

static const struct unit *unit(const char *word, size_t wn)
{
    size_t i;
    for (i = 0; i < UNITS; i++)
        if (named(units[i].names, word, wn)) return &units[i];
    return NULL;
}

/* the shown name: up to the first '|' */
static void shown(const struct unit *u, char *out, size_t cap)
{
    const char *e = strchr(u->names, '|');
    size_t len = e ? (size_t)(e - u->names) : strlen(u->names);
    if (len + 1 > cap) len = cap - 1;
    memcpy(out, u->names, len);
    out[len] = 0;
    if (u->kind == TEMP) { out[0] = (char)(out[0] - 32); }
    if (u->kind == DATA) { size_t i; for (i = 0; out[i]; i++) if (out[i] >= 'a' && out[i] <= 'z' && !(i == 1 && out[1] == 'i')) out[i] = (char)(out[i] - 32); }
}

static double to_kelvin(const struct unit *u, double v)
{
    switch (u->names[0]) { case 'c': return v + 273.15; case 'f': return (v - 32) * 5 / 9 + 273.15; default: return v; }
}
static double from_kelvin(const struct unit *u, double k)
{
    switch (u->names[0]) { case 'c': return k - 273.15; case 'f': return (k - 273.15) * 9 / 5 + 32; default: return k; }
}

static int is_word(const char *s, size_t n, const char *names) { return named(names, s, n); }

static int units_question(const char *t, char *left, size_t lcap, char *right, size_t rcap)
{
    const char *p = t, *rest, *sp;
    const struct unit *from = NULL, *to = NULL;
    double v, out;
    char num[32], name[32];
    size_t len, best = 0;
    if (!number(&p, &v)) return 0;
    while (*p == ' ') p++;
    if (!*p) return 0;
    /* the source unit: the longest run of words that names one */
    len = strlen(p);
    for (;;) {
        if ((from = unit(p, len))) { best = len; break; }
        while (len && p[len - 1] != ' ') len--;
        if (!len) return 0;
        len--;                                      /* drop the space too */
        if (!len) return 0;
    }
    rest = p + best;
    while (*rest == ' ') rest++;
    if (*rest) {
        /* an optional "to", "in", "as", "na", "w", "do", "=", "->" before the target */
        sp = strchr(rest, ' ');
        if (sp && is_word(rest, (size_t)(sp - rest), "to|in|as|into|na|w|do|=|->")) rest = sp + 1;
        if (!(to = unit(rest, strlen(rest)))) return 0;
    } else {
        to = unit(from->other, strlen(from->other));
    }
    if (!to || to->kind != from->kind) return 0;
    out = from->kind == TEMP ? from_kelvin(to, to_kelvin(from, v)) : v * from->factor / to->factor;
    if (from->kind == TEMP && to_kelvin(from, v) < 0) return 0;
    fmt(v, num, sizeof num); shown(from, name, sizeof name);
    snprintf(left, lcap, from->kind == TEMP && from->names[0] != 'k' ? "%s \xc2\xb0%s" : "%s %s", num, name);
    fmt(out, num, sizeof num); shown(to, name, sizeof name);
    snprintf(right, rcap, to->kind == TEMP && to->names[0] != 'k' ? "%s \xc2\xb0%s" : "%s %s", num, name);
    return 1;
}

/* --- percentages --------------------------------------------------------- */

static int percent_question(const char *t, char *left, size_t lcap, char *right, size_t rcap)
{
    const char *p = t, *w;
    double a, b, out;
    char sa[32], sb[32], so[32];
    if (!number(&p, &a)) return 0;
    while (*p == ' ') p++;
    if (*p == '%') {
        /* "15% of 200", "15% z 200", "20% off 80" */
        int off;
        p++;
        while (*p == ' ') p++;
        w = p;
        while (*p && *p != ' ') p++;
        off = is_word(w, (size_t)(p - w), "off");
        if (!off && !is_word(w, (size_t)(p - w), "of|z|from|ze")) return 0;
        while (*p == ' ') p++;
        if (!number(&p, &b) || *p) return 0;
        out = off ? b - b * a / 100 : b * a / 100;
        fmt(a, sa, sizeof sa); fmt(b, sb, sizeof sb); fmt(out, so, sizeof so);
        snprintf(left, lcap, off ? "%s%% off %s" : "%s%% of %s", sa, sb);
        snprintf(right, rcap, "%s", so);
        return 1;
    }
    if (*p == '+' || *p == '-') {
        /* "200 + 15%", "200 - 15%" */
        int minus = *p == '-';
        p++;
        while (*p == ' ') p++;
        if (!number(&p, &b)) return 0;
        while (*p == ' ') p++;
        if (*p != '%' || p[1]) return 0;
        out = minus ? a - a * b / 100 : a + a * b / 100;
        fmt(a, sa, sizeof sa); fmt(b, sb, sizeof sb); fmt(out, so, sizeof so);
        snprintf(left, lcap, "%s %c %s%%", sa, minus ? '-' : '+', sb);
        snprintf(right, rcap, "%s", so);
        return 1;
    }
    return 0;
}

/* --- time zones ---------------------------------------------------------- */

enum { NO_DST, EU, US, AU, NZ };

struct zone { const char *names; const char *label; int offset; int dst; };

static const struct zone zones[] = {
    { "utc|gmt|zulu", "UTC", 0, NO_DST },
    { "london|uk|england|edinburgh|manchester", "London", 0, EU },
    { "dublin|ireland", "Dublin", 0, EU },
    { "lisbon|lizbona|portugal", "Lisbon", 0, EU },
    { "reykjavik|iceland", "Reykjavik", 0, NO_DST },
    { "paris|france|paryz", "Paris", 60, EU },
    { "berlin|germany|munich|frankfurt|hamburg", "Berlin", 60, EU },
    { "warsaw|warszawa|poland|polska|krakow|gdansk|wroclaw|poznan", "Warsaw", 60, EU },
    { "madrid|spain|barcelona", "Madrid", 60, EU },
    { "rome|italy|milan|rzym", "Rome", 60, EU },
    { "amsterdam|netherlands", "Amsterdam", 60, EU },
    { "brussels|belgium", "Brussels", 60, EU },
    { "vienna|austria|wieden", "Vienna", 60, EU },
    { "prague|praga|czechia", "Prague", 60, EU },
    { "stockholm|sweden", "Stockholm", 60, EU },
    { "oslo|norway", "Oslo", 60, EU },
    { "copenhagen|denmark", "Copenhagen", 60, EU },
    { "zurich|switzerland|geneva", "Zurich", 60, EU },
    { "budapest|hungary", "Budapest", 60, EU },
    { "athens|greece|ateny", "Athens", 120, EU },
    { "helsinki|finland", "Helsinki", 120, EU },
    { "kyiv|kiev|ukraine|kijow", "Kyiv", 120, EU },
    { "bucharest|romania", "Bucharest", 120, EU },
    { "sofia|bulgaria", "Sofia", 120, EU },
    { "vilnius|riga|tallinn", "Vilnius", 120, EU },
    { "cairo|egypt|johannesburg|cape town", "Cairo", 120, NO_DST },
    { "istanbul|turkey", "Istanbul", 180, NO_DST },
    { "moscow|moskwa|russia", "Moscow", 180, NO_DST },
    { "dubai|uae|abu dhabi", "Dubai", 240, NO_DST },
    { "karachi|pakistan", "Karachi", 300, NO_DST },
    { "delhi|new delhi|mumbai|india|bangalore|kolkata", "Delhi", 330, NO_DST },
    { "dhaka", "Dhaka", 360, NO_DST },
    { "bangkok|jakarta|hanoi|thailand|vietnam", "Bangkok", 420, NO_DST },
    { "beijing|shanghai|china|hong kong|singapore|taipei|manila|perth|pekin", "Beijing", 480, NO_DST },
    { "tokyo|japan|tokio|osaka", "Tokyo", 540, NO_DST },
    { "seoul|korea", "Seoul", 540, NO_DST },
    { "sydney|melbourne|canberra", "Sydney", 600, AU },
    { "brisbane", "Brisbane", 600, NO_DST },
    { "auckland|wellington|new zealand", "Auckland", 720, NZ },
    { "new york|nyc|ny|toronto|boston|miami|washington|atlanta|nowy jork", "New York", -300, US },
    { "chicago|dallas|houston|austin", "Chicago", -360, US },
    { "denver|salt lake city", "Denver", -420, US },
    { "phoenix|arizona", "Phoenix", -420, NO_DST },
    { "los angeles|la|san francisco|sf|seattle|vancouver|california|portland", "Los Angeles", -480, US },
    { "anchorage|alaska", "Anchorage", -540, US },
    { "honolulu|hawaii", "Honolulu", -600, NO_DST },
    { "sao paulo|rio|rio de janeiro|brazil|buenos aires|argentina", "Sao Paulo", -180, NO_DST },
};
#define ZONES (sizeof zones / sizeof zones[0])

static const struct zone *zone(const char *w, size_t n)
{
    size_t i;
    for (i = 0; i < ZONES; i++)
        if (named(zones[i].names, w, n)) return &zones[i];
    return NULL;
}

/* days since 1970-01-01 of a date, and back (Gregorian) */
static long days_of(long y, int m, int d)
{
    long era, yoe, doy, doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static long year_of(long days)
{
    long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097, yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    return y + (mp >= 10);
}
static int weekday(long days) { return (int)(((days % 7) + 11) % 7); }      /* 0 Sunday */

/* the nth Sunday of a month (n 1..4), or the last one (n 0), as days */
static long sunday(long year, int month, int n)
{
    if (n) {
        long first = days_of(year, month, 1);
        return first + (7 - weekday(first)) % 7 + 7 * (n - 1);
    } else {
        long last = days_of(month == 12 ? year + 1 : year, month == 12 ? 1 : month + 1, 1) - 1;
        return last - weekday(last);
    }
}

/* Whether daylight time is on in a zone at a moment given in UTC minutes
 * since 1970. */
static int daylight(const struct zone *z, long long utc)
{
    long year = year_of((long)((utc + z->offset) / 1440));
    long long start, end;
    switch (z->dst) {
    case EU:
        start = (long long)sunday(year, 3, 0) * 1440 + 60;
        end = (long long)sunday(year, 10, 0) * 1440 + 60;
        return utc >= start && utc < end;
    case US:
        start = (long long)sunday(year, 3, 2) * 1440 + 120 - z->offset;
        end = (long long)sunday(year, 11, 1) * 1440 + 60 - z->offset;
        return utc >= start && utc < end;
    case AU:
        start = (long long)sunday(year, 10, 1) * 1440 + 120 - z->offset;
        end = (long long)sunday(year, 4, 1) * 1440 + 120 - z->offset;
        return utc >= start || utc < end;
    case NZ:
        start = (long long)sunday(year, 9, 0) * 1440 + 120 - z->offset;
        end = (long long)sunday(year, 4, 1) * 1440 + 120 - z->offset;
        return utc >= start || utc < end;
    default: return 0;
    }
}

/* a clock time: "12pm", "9:30am", "15:00", "noon", "midnight"; moves *s */
static int clock_time(const char **s, int *minute)
{
    const char *p = *s;
    int h = 0, m = 0, digits = 0, colon = 0;
    if (!strncmp(p, "noon", 4)) { *minute = 720; *s = p + 4; return 1; }
    if (!strncmp(p, "midnight", 8)) { *minute = 0; *s = p + 8; return 1; }
    for (; *p >= '0' && *p <= '9' && digits < 2; p++, digits++) h = h * 10 + (*p - '0');
    if (!digits) return 0;
    if (*p == ':' || *p == '.') {
        if (!(p[1] >= '0' && p[1] <= '5' && p[2] >= '0' && p[2] <= '9')) return 0;
        m = (p[1] - '0') * 10 + (p[2] - '0');
        p += 3;
        colon = 1;
    }
    if (*p == ' ' && (p[1] == 'a' || p[1] == 'p') && p[2] == 'm') p++;
    if ((*p == 'a' || *p == 'p') && p[1] == 'm') {
        if (h < 1 || h > 12) return 0;
        h = h % 12 + (*p == 'p' ? 12 : 0);
        p += 2;
    } else if (!colon || h > 23) {
        return 0;                           /* a bare number is not a time */
    }
    if (*p && *p != ' ') return 0;
    *minute = h * 60 + m;
    *s = p;
    return 1;
}

static void clock_text(char *out, size_t cap, int minute, const char *where, long dayshift)
{
    snprintf(out, cap, "%02d:%02d %s%s", minute / 60, minute % 60, where,
             dayshift > 0 ? " (next day)" : dayshift < 0 ? " (day before)" : "");
}

static int time_question(const char *t, const struct ln_now *now, char *left, size_t lcap, char *right, size_t rcap)
{
    const char *p = t, *sep;
    const struct zone *from = NULL, *to = NULL;
    int minute = now->minute, have_time = 0, from_off, to_off;
    long long utc, there;
    long dayshift;
    size_t n = strlen(t);

    /* "tokyo time" */
    if (n > 5 && !strcmp(t + n - 5, " time") && (to = zone(t, n - 5))) goto answer;
    if (!strncmp(p, "now ", 4)) { p += 4; have_time = 1; }
    else if (!strncmp(p, "time ", 5)) { p += 5; have_time = 1; }
    else if (clock_time(&p, &minute)) { have_time = 1; while (*p == ' ') p++; }
    if (!have_time) return 0;
    if (!strncmp(p, "here ", 5)) p += 5;
    /* "<city> in|to <city>", "in|to <city>", "<city>" */
    if (!strncmp(p, "in ", 3) || !strncmp(p, "to ", 3) || !strncmp(p, "at ", 3) || !strncmp(p, "w ", 2)) {
        p = strchr(p, ' ') + 1;
        to = zone(p, strlen(p));
    } else {
        sep = strstr(p, " in ");
        if (!sep) sep = strstr(p, " to ");
        if (sep) {
            from = zone(p, (size_t)(sep - p));
            to = zone(sep + 4, strlen(sep + 4));
            if (!from) return 0;
        } else {
            to = zone(p, strlen(p));
        }
    }
    if (!to) return 0;
answer:
    /* the asked time, on today's date where it is asked from, as UTC */
    utc = (long long)now->days * 1440 + minute;
    if (from) {
        from_off = from->offset;
        utc -= from_off;
        if (daylight(from, utc)) { utc -= 60; from_off += 60; }
    } else {
        from_off = now->utc_offset;
        utc -= from_off;
    }
    to_off = to->offset + (daylight(to, utc) ? 60 : 0);
    there = utc + to_off;
    dayshift = (long)(there >= 0 ? there / 1440 : -((-there + 1439) / 1440)) - now->days;
    clock_text(left, lcap, minute, from ? from->label : "here", 0);
    clock_text(right, rcap, (int)(((there % 1440) + 1440) % 1440), to->label, dayshift);
    return 1;
}

/* --- currencies ---------------------------------------------------------- */

static const struct ln_rates *rates;
static char note[48];

void ln_convert_use_rates(const struct ln_rates *r) { rates = r; }
const char *ln_convert_note(void) { return note; }

/* names people type for a currency, beside its three letters */
static const struct { const char *code, *names; } money[] = {
    { "EUR", "euro|euros|\xe2\x82\xac" },
    { "USD", "$|dollar|dollars|us dollar|us dollars|dolar|dolary|dolarow" },
    { "GBP", "\xc2\xa3|pound sterling|pounds sterling|funt|funty|funtow" },
    { "PLN", "zl|z\xc5\x82|zloty|zlote|zlotych|z\xc5\x82oty|z\xc5\x82ote|z\xc5\x82otych" },
    { "CHF", "franc|francs|frank|franki|frankow" },
    { "JPY", "yen|\xc2\xa5|jen|jeny|jenow" },
    { "CZK", "koruna|korona|korony|koron" },
    { "SEK", "swedish krona" }, { "NOK", "norwegian krone" }, { "DKK", "danish krone" },
    { "CAD", "canadian dollar|canadian dollars" }, { "AUD", "australian dollar|australian dollars" },
    { "HUF", "forint|forints" }, { "CNY", "yuan|rmb" }, { "INR", "rupee|rupees" },
};

/* The rate of a currency named by word (euros per unit inverted: units per
 * euro), and its code; 0 when the word names none or the rates lack it. */
static int currency(const char *word, size_t wn, char code[4], double *per_euro)
{
    char up[4];
    size_t i;
    int k;
    code[0] = 0;
    if (wn == 3) {
        for (i = 0; i < 3; i++) up[i] = (char)(word[i] >= 'a' && word[i] <= 'z' ? word[i] - 32 : word[i]);
        up[3] = 0;
        memcpy(code, up, 4);
    }
    for (i = 0; i < sizeof money / sizeof money[0]; i++)
        if (named(money[i].names, word, wn)) memcpy(code, money[i].code, 4);
    if (!code[0]) return 0;
    if (!strcmp(code, "EUR")) { *per_euro = 1; return 1; }
    for (k = 0; rates && k < rates->n; k++)
        if (!strcmp(rates->code[k], code)) { *per_euro = rates->rate[k]; return 1; }
    return 0;
}

static int money_question(const char *t, char *left, size_t lcap, char *right, size_t rcap)
{
    const char *p = t, *rest, *sp;
    char from[4], to[4];
    double v, rf, rt, out;
    size_t len;
    if (!rates || !number(&p, &v) || v < 0) return 0;
    while (*p == ' ') p++;
    if (!*p) return 0;
    /* the source currency: the longest run of words that names one */
    len = strlen(p);
    for (;;) {
        if (currency(p, len, from, &rf)) break;
        while (len && p[len - 1] != ' ') len--;
        if (len < 2) return 0;
        len--;
    }
    rest = p + len;
    while (*rest == ' ') rest++;
    if (*rest) {
        sp = strchr(rest, ' ');
        if (sp && is_word(rest, (size_t)(sp - rest), "to|in|as|into|na|w|do|=|->")) rest = sp + 1;
        if (!currency(rest, strlen(rest), to, &rt)) return 0;
    } else if (!currency(strcmp(from, "EUR") ? "eur" : "usd", 3, to, &rt)) {
        return 0;
    }
    out = v / rf * rt;
    if (v == floor(v)) snprintf(left, lcap, "%.0f %s", v, from);
    else snprintf(left, lcap, "%.2f %s", v, from);
    snprintf(right, rcap, "%.2f %s", out, to);
    snprintf(note, sizeof note, "ECB rates of %s", rates->date);
    return 1;
}

int ln_convert(const char *text, size_t n, const struct ln_now *now,
               char *left, size_t lcap, char *right, size_t rcap)
{
    char t[160];
    if (n >= sizeof t || lcap < 8 || rcap < 8) return 0;
    note[0] = 0;
    if (!tidy(text, n, t, sizeof t)) return 0;
    if (percent_question(t, left, lcap, right, rcap)) return 1;
    if (now && time_question(t, now, left, lcap, right, rcap)) return 1;
    if (units_question(t, left, lcap, right, rcap)) return 1;
    return money_question(t, left, lcap, right, rcap);
}
