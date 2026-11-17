/* SPDX-License-Identifier: MIT */
/* Conversions typed in words. Pure.
 *
 *   10 km to mi      72 f in c      5 ft      2.5 kg lb      100 mb to gb
 *   15% of 200       200 + 15%      200 - 15%     20% off 80
 *   12pm in london   15:30 warsaw to new york     now in tokyo     tokyo time
 *   100 usd to pln   50 eur      20 funtow na zlote
 */
#ifndef LN_CONVERT_H
#define LN_CONVERT_H

#include <stddef.h>

/* The moment and place the question is asked at. */
struct ln_now {
    long days;          /* days since 1970-01-01, local date */
    int minute;         /* minutes since local midnight */
    int utc_offset;     /* local time minus UTC, in minutes */
};

/* Exchange rates: how much of each currency one euro buys, and their date. */
#define LN_RATES_MAX 64
struct ln_rates {
    char date[12];
    int n;
    char code[LN_RATES_MAX][4];     /* upper case, without EUR */
    double rate[LN_RATES_MAX];
};
/* The rates currency questions use from now on (kept by pointer; NULL: none,
 * such questions then have no answer). */
void ln_convert_use_rates(const struct ln_rates *r);
/* A note on the last answer ("ECB rates of 2026-11-16"), "" when it has none. */
const char *ln_convert_note(void);

/* 1 when text is such a question: left gets what was asked, tidied
 * ("10 km"), right the answer ("6.21371 mi"). 0 when it is not, or has no
 * answer. Both buffers should hold 64 bytes. */
int ln_convert(const char *text, size_t n, const struct ln_now *now,
               char *left, size_t lcap, char *right, size_t rcap);

#endif
