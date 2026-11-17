/* SPDX-License-Identifier: MIT */
/* Conversions typed in words. Pure.
 *
 *   10 km to mi      72 f in c      5 ft      2.5 kg lb      100 mb to gb
 *   15% of 200       200 + 15%      200 - 15%     20% off 80
 *   12pm in london   15:30 warsaw to new york     now in tokyo     tokyo time
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

/* 1 when text is such a question: left gets what was asked, tidied
 * ("10 km"), right the answer ("6.21371 mi"). 0 when it is not, or has no
 * answer. Both buffers should hold 64 bytes. */
int ln_convert(const char *text, size_t n, const struct ln_now *now,
               char *left, size_t lcap, char *right, size_t rcap);

#endif
