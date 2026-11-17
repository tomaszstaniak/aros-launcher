/* SPDX-License-Identifier: MIT */
#include "style.h"

#define DARK_COLOURS \
    0xb81c1e24ul, 0xf21c1e24ul, 0xffeef0f4ul, 0x8ceef0f4ul, 0xff7fb4fful, \
    0x21fffffful, 0xffeef0f4ul, 0x8ceef0f4ul, 0xff7fb4fful, 0x1afffffful, 0x1ffffffful
#define LIGHT_COLOURS \
    0xc8f4f5f7ul, 0xf2f4f5f7ul, 0xff1d2330ul, 0x991d2330ul, 0xff1f5fc4ul, \
    0xff3f7de0ul, 0xfffffffful, 0xd9fffffful, 0xfffffffful, 0x1a000000ul, 0x24000000ul

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
