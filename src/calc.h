/* SPDX-License-Identifier: MIT */
/* The calculator: a typed expression and its value. Pure.
 *
 *   2+2    1.5*4    (3+4)^2    10/4    7%3    -5+2    2(3+1)
 *   sqrt(2)  sin(pi/2)  ln(e)  log(100)  abs(-3)  round(2.5)  0x1f+1
 *
 * A comma is a decimal point as well ("2,5*2"). Angles are in radians. */
#ifndef LN_CALC_H
#define LN_CALC_H

#include <stddef.h>

/* 1 when text is a calculation with a value, written to out as text
 * (whole numbers in full up to 15 digits, others with at most 12
 * significant digits, no trailing zeros). 0 when it is not a
 * calculation: plain text, a number alone, something that does not parse,
 * or a value that does not exist (division by zero, a root of a negative
 * number). */
int ln_calc(const char *text, size_t n, char *out, size_t cap);

#endif
