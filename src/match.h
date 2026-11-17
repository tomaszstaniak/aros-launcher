/* SPDX-License-Identifier: MIT */
/* Fuzzy matching of a typed query against a title. Pure.
 *
 * A query matches when each of its characters occurs in the title in
 * order. Case and accents do not count ("zolw" finds "Żółw"); spaces in
 * the query are ignored. Among the ways a query fits a title the one with
 * the highest score is reported. */
#ifndef LN_MATCH_H
#define LN_MATCH_H

#include <stddef.h>

#define LN_MATCH_MAX 64

struct ln_match {
    int score;
    int n;                              /* matched characters reported in pos */
    unsigned short pos[LN_MATCH_MAX];   /* their byte offsets in the title */
};

/* 1 when query matches title, 0 when it does not (also: not UTF-8, out of
 * memory). An empty query matches everything with score 0 and n 0. Only the
 * first LN_MATCH_MAX characters of the query and the first 256 of the title
 * take part. Higher scores are better:
 *   +40 a match at the start of the title
 *   +20 a match at the start of a word (after space - _ . or at a capital
 *       that follows a small letter)
 *   +15 a match right after the previous one
 *   -2  per character skipped before the first match, at most -20
 *   -1  per 8 bytes of title, so that shorter titles win ties */
int ln_match(const char *query, size_t qn, const char *title, size_t tn, struct ln_match *out);

#endif
