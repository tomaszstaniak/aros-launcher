#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "convert.h"

static struct ln_now now;
static char l[64], r[64];

static int is(const char *in, const char *left, const char *right)
{
    if (!ln_convert(in, strlen(in), &now, l, sizeof l, r, sizeof r)) { printf("no answer: %s\n", in); return 0; }
    if (strcmp(l, left) || strcmp(r, right)) { printf("%s: \"%s\" -> \"%s\"\n", in, l, r); return 0; }
    return 1;
}
static int none(const char *in) { return !ln_convert(in, strlen(in), &now, l, sizeof l, r, sizeof r); }

/* days since 1970 for a date, by counting (small and obviously right) */
static long days(int y, int m, int d)
{
    static const int len[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    long n = 0;
    int i;
    for (i = 1970; i < y; i++) n += 365 + ((i % 4 == 0 && i % 100 != 0) || i % 400 == 0);
    for (i = 1; i < m; i++) n += len[i - 1] + (i == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0));
    return n + d - 1;
}

int main(void)
{
    /* Warsaw in winter: 17 November 2026, 13:34, UTC+1 */
    now.days = days(2026, 11, 17); now.minute = 13 * 60 + 34; now.utc_offset = 60;

    /* units */
    assert(is("10 km to mi", "10 km", "6.21371 mi"));
    assert(is("10km in miles", "10 km", "6.21371 mi"));
    assert(is("10 km", "10 km", "6.21371 mi"));
    assert(is("5 ft", "5 ft", "1.524 m"));
    assert(is("5 in", "5 in", "12.7 cm"));
    assert(is("5 in in cm", "5 in", "12.7 cm"));
    assert(is("1 m in in", "1 m", "39.3701 in"));
    assert(is("2,5 kg lb", "2.5 kg", "5.51156 lb"));
    assert(is("72 f in c", "72 \xc2\xb0""F", "22.2222 \xc2\xb0""C"));
    assert(is("20\xc2\xb0""C", "20 \xc2\xb0""C", "68 \xc2\xb0""F"));
    assert(is("0 k to c", "0 K", "-273.15 \xc2\xb0""C"));
    assert(is("-40 c to f", "-40 \xc2\xb0""C", "-40 \xc2\xb0""F"));
    assert(is("100 mb to gb", "100 MB", "0.1 GB"));
    assert(is("1 gib to mb", "1 GiB", "1073.74 MB"));
    assert(is("90 km/h to mph", "90 km/h", "55.9234 mph"));
    assert(is("3 hours in minutes", "3 h", "180 min"));
    assert(is("2 nautical miles to km", "2 nmi", "3.704 km"));
    assert(is("1 ha", "1 ha", "2.47105 acres"));
    assert(is("10 KM TO MI", "10 km", "6.21371 mi"));
    assert(none("10 km to kg"));
    assert(none("10 km to"));
    assert(none("10 foo"));
    assert(none("km to mi"));
    assert(none("10"));
    assert(none("-500 c to k"));

    /* percentages */
    assert(is("15% of 200", "15% of 200", "30"));
    assert(is("15 % z 200", "15% of 200", "30"));
    assert(is("200 + 15%", "200 + 15%", "230"));
    assert(is("200 - 15%", "200 - 15%", "170"));
    assert(is("20% off 80", "20% off 80", "64"));
    assert(is("7.5% of 120", "7.5% of 120", "9"));
    assert(none("15% 200"));
    assert(none("200 + 15"));
    assert(none("15%"));

    /* time zones: winter, London is one hour behind Warsaw */
    assert(is("12pm in london", "12:00 here", "11:00 London"));
    assert(is("12pm here in london", "12:00 here", "11:00 London"));
    assert(is("12:30 london", "12:30 here", "11:30 London"));
    assert(is("9am to new york", "09:00 here", "03:00 New York"));
    assert(is("15:30 warsaw to new york", "15:30 Warsaw", "09:30 New York"));
    assert(is("now in tokyo", "13:34 here", "21:34 Tokyo"));
    assert(is("tokyo time", "13:34 here", "21:34 Tokyo"));
    assert(is("time in los angeles", "13:34 here", "04:34 Los Angeles"));
    assert(is("23:00 in tokyo", "23:00 here", "07:00 Tokyo (next day)"));
    assert(is("1am in new york", "01:00 here", "19:00 New York (day before)"));
    assert(is("noon in sydney", "12:00 here", "22:00 Sydney"));          /* Sydney has daylight time in November */
    assert(is("midnight utc", "00:00 here", "23:00 UTC (day before)"));
    assert(is("12pm london in warsaw", "12:00 London", "13:00 Warsaw"));
    assert(is("10:00 delhi", "10:00 here", "14:30 Delhi"));
    assert(none("12pm in atlantis"));
    assert(none("12 in london"));          /* a bare number is not a time */
    assert(none("25:00 in london"));
    assert(none("13pm in london"));

    /* summer: 1 July 2026, Warsaw UTC+2; London UTC+1, New York UTC-4 */
    now.days = days(2026, 7, 1); now.minute = 12 * 60; now.utc_offset = 120;
    assert(is("12pm in london", "12:00 here", "11:00 London"));
    assert(is("12pm in new york", "12:00 here", "06:00 New York"));
    assert(is("12pm in sydney", "12:00 here", "20:00 Sydney"));           /* no daylight time there in July */
    assert(is("12:00 london to tokyo", "12:00 London", "20:00 Tokyo"));
    assert(is("12pm in phoenix", "12:00 here", "03:00 Phoenix"));

    /* the weeks when Europe and America differ: 20 March 2026, the US is
       already on daylight time, Europe not yet (Warsaw UTC+1) */
    now.days = days(2026, 3, 20); now.utc_offset = 60;
    assert(is("12pm in new york", "12:00 here", "07:00 New York"));
    assert(is("12pm in london", "12:00 here", "11:00 London"));
    /* 30 March 2026: both on daylight time */
    now.days = days(2026, 3, 30); now.utc_offset = 120;
    assert(is("12pm in new york", "12:00 here", "06:00 New York"));

    /* without a moment, time questions have no answer; the others do */
    assert(!ln_convert("12pm in london", 14, NULL, l, sizeof l, r, sizeof r));
    assert(ln_convert("10 km", 5, NULL, l, sizeof l, r, sizeof r));

    /* currencies: none without rates; with them, through the euro */
    assert(none("100 usd to pln"));
    {
        static struct ln_rates rates = { "2026-11-16", 4, { "USD", "PLN", "GBP", "JPY" }, { 1.25, 4.0, 0.8, 160.0 } };
        ln_convert_use_rates(&rates);
        assert(is("100 usd to pln", "100 USD", "320.00 PLN"));
        assert(!strcmp(ln_convert_note(), "ECB rates of 2026-11-16"));
        assert(is("100 eur", "100 EUR", "125.00 USD"));
        assert(is("100 usd", "100 USD", "80.00 EUR"));
        assert(is("50 euro in pln", "50 EUR", "200.00 PLN"));
        assert(is("20 funtow na zlote", "20 GBP", "100.00 PLN"));
        assert(is("12,50 z\xc5\x82 to eur", "12.50 PLN", "3.12 EUR"));
        assert(is("1000 yen to usd", "1000 JPY", "7.81 USD"));
        assert(is("3 us dollars to gbp", "3 USD", "1.92 GBP"));
        assert(none("100 xyz to pln"));
        assert(none("100 usd to xyz"));
        assert(none("-5 usd"));
        /* units and the calculator's words still win where they apply */
        assert(is("10 km to mi", "10 km", "6.21371 mi"));
        assert(!strcmp(ln_convert_note(), ""));
        ln_convert_use_rates(NULL);
        assert(none("100 usd to pln"));
    }

    /* not questions */
    assert(none(""));
    assert(none("clock"));
    assert(none("2+2"));
    assert(none("time"));
    assert(none("now"));
    return 0;
}
