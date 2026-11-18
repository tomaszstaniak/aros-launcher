/* SPDX-License-Identifier: MIT */
/* A colour theme in Limpet's file format, and the panel's colours made
 * from it. Pure.
 *
 *   background=#2e3440   foreground=#d8dee9   cursor=#eceff4
 *   selection_bg=#eceff4 selection_fg=#4c566a ansi0..ansi15=#rrggbb
 *
 * A theme is dark or light by its own colours. The other variant is made
 * from it: every colour keeps its hue and saturation and takes the
 * mirrored lightness, then background and text are moved apart until
 * they read. */
#ifndef LN_THEME_H
#define LN_THEME_H

#include <stddef.h>

struct ln_style;

struct ln_theme {
    unsigned long bg, fg;           /* 0xRRGGBB */
    unsigned long cursor, sel_bg, sel_fg, accent;
    int has_cursor, has_sel_bg, has_sel_fg, has_accent;    /* accent: ansi4, the theme's blue */
};

/* 1 when the text has a background and a foreground. Lines that are not
 * colours of this format are skipped. */
int ln_theme_parse(const char *text, size_t n, struct ln_theme *t);
/* 1 when the background is darker than the text. */
int ln_theme_dark(const struct ln_theme *t);
/* The other variant of t. */
void ln_theme_flip(const struct ln_theme *t, struct ln_theme *out);
/* WCAG contrast of two colours, times 100. */
int ln_theme_contrast100(unsigned long a, unsigned long b);
/* The colours of style s from t, in the variant asked for (dark 1 or 0);
 * the measures of s stay. */
void ln_theme_apply(const struct ln_theme *t, int dark, struct ln_style *s);

#endif
