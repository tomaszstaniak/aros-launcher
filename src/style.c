/* SPDX-License-Identifier: MIT */
#include "style.h"

/* The colours are Dockbar's, so that the two look alike beside each other:
 * its popup fill, text, muted text, accent (its insertion mark), popup
 * hover, separator and popup border, dark and light. Order: tint,
 * tint_plain, text, dim, match, sel_fill, sel_text, sel_dim, sel_match,
 * rule, edge. */
#define DARK_COLOURS \
    0xb830353aul, 0xf230353aul, 0xffedf3faul, 0xffaabbcdul, 0xff9bdbd4ul, \
    0xff485159ul, 0xffedf3faul, 0xffaabbcdul, 0xff9bdbd4ul, 0x80555d63ul, 0x80767d82ul
#define LIGHT_COLOURS \
    0xc8f2f4f6ul, 0xf2f2f4f6ul, 0xff1d2328ul, 0xff4d5964ul, 0xff1f7f73ul, \
    0xffd3dae0ul, 0xff1d2328ul, 0xff4d5964ul, 0xff1f7f73ul, 0x80a8b0b7ul, 0x809aa3abul

static const struct ln_style styles[4] = {
    /* dark, full */
    { 620, 14, 50, 46, 108, 24, 34, 6, 7, 16, 32, 1, 19, 14, 12, 30, DARK_COLOURS },
    /* dark, compact */
    { 560, 10, 40, 28, 64, 0, 28, 6, 5, 14, 20, 0, 16, 13, 11, 22, DARK_COLOURS },
    /* light, full */
    { 620, 14, 50, 46, 108, 24, 34, 6, 7, 16, 32, 1, 19, 14, 12, 30, LIGHT_COLOURS },
    /* light, compact */
    { 560, 10, 40, 28, 64, 0, 28, 6, 5, 14, 20, 0, 16, 13, 11, 22, LIGHT_COLOURS },
};

const struct ln_style *ln_style_get(int dark, int compact)
{
    return &styles[(dark ? 0 : 2) + (compact ? 1 : 0)];
}
