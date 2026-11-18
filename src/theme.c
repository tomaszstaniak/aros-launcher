/* SPDX-License-Identifier: MIT */
#include "theme.h"
#include "style.h"

#include <math.h>
#include <string.h>

static int hex(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

static int colour(const char *v, size_t n, unsigned long *out)
{
    unsigned long c = 0;
    size_t i;
    while (n && (*v == ' ' || *v == '\t')) v++, n--;
    while (n && (v[n - 1] == ' ' || v[n - 1] == '\t' || v[n - 1] == '\r')) n--;
    if (n != 7 || v[0] != '#') return 0;
    for (i = 1; i < 7; i++) {
        int d = hex((unsigned char)v[i]);
        if (d < 0) return 0;
        c = c << 4 | (unsigned long)d;
    }
    *out = c;
    return 1;
}

int ln_theme_parse(const char *text, size_t n, struct ln_theme *t)
{
    const char *p = text, *end = text + n;
    int bg = 0, fg = 0;
    memset(t, 0, sizeof *t);
    while (p < end) {
        const char *line = p, *stop = memchr(p, '\n', (size_t)(end - p)), *eq;
        size_t k;
        unsigned long c;
        if (!stop) stop = end;
        p = stop < end ? stop + 1 : end;
        while (line < stop && (*line == ' ' || *line == '\t')) line++;
        if (line == stop || *line == '#') continue;
        if (!(eq = memchr(line, '=', (size_t)(stop - line)))) continue;
        k = (size_t)(eq - line);
        while (k && (line[k - 1] == ' ' || line[k - 1] == '\t')) k--;
        if (!colour(eq + 1, (size_t)(stop - eq - 1), &c)) continue;
        if (k == 10 && !memcmp(line, "background", 10)) { t->bg = c; bg = 1; }
        else if (k == 10 && !memcmp(line, "foreground", 10)) { t->fg = c; fg = 1; }
        else if (k == 6 && !memcmp(line, "cursor", 6)) { t->cursor = c; t->has_cursor = 1; }
        else if (k == 12 && !memcmp(line, "selection_bg", 12)) { t->sel_bg = c; t->has_sel_bg = 1; }
        else if (k == 12 && !memcmp(line, "selection_fg", 12)) { t->sel_fg = c; t->has_sel_fg = 1; }
        else if (k == 5 && !memcmp(line, "ansi4", 5)) { t->accent = c; t->has_accent = 1; }
    }
    return bg && fg;
}

static double lin(unsigned long v)
{
    double c = (double)v / 255.0;
    return c <= 0.03928 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

static double lum(unsigned long c)
{
    return 0.2126 * lin((c >> 16) & 255) + 0.7152 * lin((c >> 8) & 255) + 0.0722 * lin(c & 255);
}

int ln_theme_contrast100(unsigned long a, unsigned long b)
{
    double la = lum(a), lb = lum(b), hi = la > lb ? la : lb, lo = la > lb ? lb : la;
    return (int)((hi + 0.05) / (lo + 0.05) * 100.0 + 0.5);
}

int ln_theme_dark(const struct ln_theme *t) { return lum(t->bg) < lum(t->fg); }

static void to_hsl(unsigned long c, double *h, double *s, double *l)
{
    double r = ((c >> 16) & 255) / 255.0, g = ((c >> 8) & 255) / 255.0, b = (c & 255) / 255.0;
    double mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), d = mx - mn;
    *l = (mx + mn) / 2;
    *h = 0; *s = 0;
    if (d > 0) {
        *s = d / (1 - fabs(2 * *l - 1));
        if (mx == r) *h = fmod((g - b) / d, 6.0);
        else if (mx == g) *h = (b - r) / d + 2;
        else *h = (r - g) / d + 4;
        *h *= 60;
        if (*h < 0) *h += 360;
    }
}

static unsigned long from_hsl(double h, double s, double l)
{
    double c = (1 - fabs(2 * l - 1)) * s, x = c * (1 - fabs(fmod(h / 60.0, 2.0) - 1)), m = l - c / 2, r = 0, g = 0, b = 0;
    int k = (int)(h / 60.0) % 6;
    unsigned long R, G, B;
    switch (k) {
    case 0: r = c; g = x; break;
    case 1: r = x; g = c; break;
    case 2: g = c; b = x; break;
    case 3: g = x; b = c; break;
    case 4: r = x; b = c; break;
    default: r = c; b = x; break;
    }
    R = (unsigned long)((r + m) * 255.0 + 0.5); G = (unsigned long)((g + m) * 255.0 + 0.5); B = (unsigned long)((b + m) * 255.0 + 0.5);
    if (R > 255) R = 255;
    if (G > 255) G = 255;
    if (B > 255) B = 255;
    return R << 16 | G << 8 | B;
}

static unsigned long with_light(unsigned long c, double l)
{
    double h, s, old;
    to_hsl(c, &h, &s, &old);
    return from_hsl(h, s, l < 0 ? 0 : l > 1 ? 1 : l);
}

static unsigned long mirrored(unsigned long c)
{
    double h, s, l;
    to_hsl(c, &h, &s, &l);
    return from_hsl(h, s, 1 - l);
}

static unsigned long mix(unsigned long a, unsigned long b, double t)
{
    unsigned long r = (unsigned long)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t + 0.5);
    unsigned long g = (unsigned long)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t + 0.5);
    unsigned long bl = (unsigned long)((a & 255) * (1 - t) + (b & 255) * t + 0.5);
    return r << 16 | g << 8 | bl;
}

void ln_theme_flip(const struct ln_theme *t, struct ln_theme *out)
{
    double h, s, l;
    int dark, i;
    *out = *t;
    out->bg = mirrored(t->bg);
    out->fg = mirrored(t->fg);
    out->cursor = mirrored(t->cursor);
    out->sel_bg = mirrored(t->sel_bg);
    out->sel_fg = mirrored(t->sel_fg);
    out->accent = mirrored(t->accent);
    dark = !ln_theme_dark(t);
    /* a background that is a panel's: not black, not blinding; a pale
       theme's background would mirror to near black */
    to_hsl(out->bg, &h, &s, &l);
    if (dark) { if (l < 0.11) l = 0.11; if (l > 0.24) l = 0.24; if (s > 0.35) s = 0.35; }
    else { if (l < 0.90) l = 0.90; if (l > 0.97) l = 0.97; }
    out->bg = from_hsl(h, s, l);
    /* text moves away from the background until it reads */
    to_hsl(out->fg, &h, &s, &l);
    for (i = 0; i < 40 && ln_theme_contrast100(from_hsl(h, s, l), out->bg) < 700; i++) {
        l += dark ? 0.02 : -0.02;
        if (l > 1) l = 1;
        if (l < 0) l = 0;
    }
    out->fg = from_hsl(h, s, l);
}

void ln_theme_apply(const struct ln_theme *theme, int dark, struct ln_style *st)
{
    struct ln_theme other, t = *theme;
    unsigned long accent, sel, sel_text;
    int c;
    if (ln_theme_dark(&t) != (dark != 0)) { ln_theme_flip(&t, &other); t = other; }
    /* the accent: the theme's blue, else its cursor, else the text; made to read on the background */
    accent = t.has_accent ? t.accent : t.has_cursor ? t.cursor : t.fg;
    if (ln_theme_contrast100(accent, t.bg) < 300) accent = with_light(accent, dark ? 0.72 : 0.36);
    if (ln_theme_contrast100(accent, t.bg) < 300) accent = t.fg;
    /* the selected row: the theme's selection when it is a quiet step from
       the background, else a step made from background and text */
    c = t.has_sel_bg ? ln_theme_contrast100(t.sel_bg, t.bg) : 0;
    if (c >= 112 && c <= 300) { sel = t.sel_bg; sel_text = t.has_sel_fg && ln_theme_contrast100(t.sel_fg, sel) >= 450 ? t.sel_fg : t.fg; }
    else { sel = mix(t.bg, t.fg, 0.16); sel_text = t.fg; }
    if (ln_theme_contrast100(sel_text, sel) < 450) sel_text = t.fg;
    st->tint = (dark ? 0xb8000000ul : 0xc8000000ul) | t.bg;
    st->tint_plain = 0xf2000000ul | t.bg;
    st->text = 0xff000000ul | t.fg;
    st->dim = 0xff000000ul | mix(t.fg, t.bg, 0.38);
    st->match = 0xff000000ul | accent;
    st->sel_fill = 0xff000000ul | sel;
    st->sel_text = 0xff000000ul | sel_text;
    st->sel_dim = 0xff000000ul | mix(sel_text, sel, 0.38);
    st->sel_match = 0xff000000ul | (ln_theme_contrast100(accent, sel) >= 250 ? accent : sel_text);
    st->rule = 0x2a000000ul | t.fg;
    st->edge = 0x38000000ul | t.fg;
}
