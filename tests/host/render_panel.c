/* Writes pictures of the panel over a stand-in desktop, to look at:
 *   render_panel <font.ttf> <outdir>
 * dark.png, light.png, compact.png (with a blurred backdrop and shadow) and
 * plain.png (no backdrop, no shadow, alpha cut as a window shape would). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "draw.h"
#include "theme.h"

static unsigned long crc_table[256];

static unsigned long crc(unsigned long c, const unsigned char *p, size_t n)
{
    size_t i;
    if (!crc_table[1]) {
        unsigned long k; int j;
        for (i = 0; i < 256; i++) { k = (unsigned long)i; for (j = 0; j < 8; j++) k = k & 1 ? 0xedb88320ul ^ (k >> 1) : k >> 1; crc_table[i] = k; }
    }
    c ^= 0xfffffffful;
    for (i = 0; i < n; i++) c = crc_table[(c ^ p[i]) & 255] ^ (c >> 8);
    return c ^ 0xfffffffful;
}

static void be32(unsigned char *p, unsigned long v) { p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16); p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v; }

static void chunk(FILE *f, const char *type, const unsigned char *data, size_t n)
{
    unsigned char h[8], t[4];
    unsigned long c;
    be32(h, (unsigned long)n); memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n) fwrite(data, 1, n, f);
    c = crc(0, (const unsigned char *)type, 4);
    if (n) { /* crc over type then data */
        unsigned char *all = malloc(n + 4);
        memcpy(all, type, 4); memcpy(all + 4, data, n);
        c = crc(0, all, n + 4);
        free(all);
    }
    be32(t, c);
    fwrite(t, 1, 4, f);
}

/* RGB picture as PNG with stored (uncompressed) deflate blocks. */
static void png(const char *path, const unsigned char *rgb, int w, int h)
{
    static const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    size_t raw = (size_t)h * ((size_t)w * 3 + 1), blocks = raw / 65535 + 1, zn = 2 + raw + blocks * 5 + 4, at = 0, left = raw, i;
    unsigned char *r = malloc(raw), *z = malloc(zn), ihdr[13];
    unsigned long a = 1, b = 0;
    FILE *f = fopen(path, "wb");
    int y;
    if (!f || !r || !z) { perror(path); exit(1); }
    for (y = 0; y < h; y++) { r[(size_t)y * ((size_t)w * 3 + 1)] = 0; memcpy(r + (size_t)y * ((size_t)w * 3 + 1) + 1, rgb + (size_t)y * (size_t)w * 3, (size_t)w * 3); }
    z[at++] = 0x78; z[at++] = 0x01;
    i = 0;
    while (left) {
        size_t n = left > 65535 ? 65535 : left;
        z[at++] = left == n; z[at++] = (unsigned char)n; z[at++] = (unsigned char)(n >> 8); z[at++] = (unsigned char)~n; z[at++] = (unsigned char)(~n >> 8);
        memcpy(z + at, r + i, n); at += n; i += n; left -= n;
    }
    for (i = 0; i < raw; i++) { a = (a + r[i]) % 65521; b = (b + a) % 65521; }
    be32(z + at, (b << 16) | a); at += 4;
    fwrite(sig, 1, 8, f);
    be32(ihdr, (unsigned long)w); be32(ihdr + 4, (unsigned long)h); ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, at);
    chunk(f, "IEND", NULL, 0);
    fclose(f); free(r); free(z);
}

/* A desktop to stand behind the panel: a gradient with some "text" stripes. */
static void desktop(unsigned char *rgb, int w, int h)
{
    int x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            unsigned char *p = rgb + ((size_t)y * (size_t)w + (size_t)x) * 3;
            int t = (x * 255 / w + y * 255 / h) / 2;
            p[0] = (unsigned char)(59 + t * 140 / 255); p[1] = (unsigned char)(110 + t * 50 / 255); p[2] = (unsigned char)(165 - t * 40 / 255);
            if (y % 18 < 9 && (x / 7 + y / 18) % 3 && x > 60 && x < w - 200) { p[0] = p[1] = p[2] = 235; }
        }
}

static void blur(unsigned char *argb, int w, int h, int rad)
{
    unsigned char *t = malloc((size_t)w * (size_t)h * 4);
    int pass, x, y, k, c;
    for (pass = 0; pass < 4; pass++) {
        int horiz = !(pass & 1);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                for (c = 1; c < 4; c++) {
                    int sum = 0, n = 0;
                    for (k = -rad; k <= rad; k++) {
                        int xx = horiz ? x + k : x, yy = horiz ? y : y + k;
                        if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                        sum += argb[((size_t)yy * (size_t)w + (size_t)xx) * 4 + (size_t)c]; n++;
                    }
                    t[((size_t)y * (size_t)w + (size_t)x) * 4 + (size_t)c] = (unsigned char)(sum / n);
                }
        for (x = 0; x < w * h; x++) { argb[x * 4 + 1] = t[x * 4 + 1]; argb[x * 4 + 2] = t[x * 4 + 2]; argb[x * 4 + 3] = t[x * 4 + 3]; }
    }
    free(t);
}

static int calc_mode;

/* When set, the pictures take their colours from this theme. */
static const struct ln_theme *render_theme;

static void one(const char *dir, const char *name, const char *font, int dark, int compact, int glass)
{
    struct ln_style themed = *ln_style_get(dark, compact);
    const struct ln_style *s = &themed;
    enum { W = 900, H = 520 };
    int shadow = glass ? 28 : 0, x, y, ox, oy, i;
    struct ln_fonts f;
    struct ln_results *r = ln_results_new();
    struct ln_view v;
    struct ln_layout l;
    struct ln_row w[5];
    unsigned char *rgb = malloc(W * H * 3), *pic, *back = NULL;
    char path[512];
    unsigned q = ln_results_query(r);
    if (render_theme) ln_theme_apply(render_theme, dark, &themed);
    static const char *titles[5] = { "WordCraft", "Password Safe", "Wordbook", "Sword of Fargoal", "Crossword" };
    static const char *subs[5] = { "SYS:Apps/WordCraft", "SYS:Utilities", "Work:Tools/Wordbook", "Work:Games/Fargoal", "Work:Games" };
    static const unsigned short at[5] = { 0, 4, 0, 1, 5 };

    memset(&f, 0, sizeof f);
    f.query = zt_font_open(&font, 1, s->font_query);
    f.big = zt_font_open(&font, 1, s->font_big);
    f.title = zt_font_open(&font, 1, s->font_title);
    f.small = zt_font_open(&font, 1, s->font_small);
    if (!f.query || !f.title || !f.small) { fprintf(stderr, "cannot open %s\n", font); exit(1); }
    memset(w, 0, sizeof w);
    for (i = 0; i < 5; i++) {
        w[i].key = (unsigned)i + 1; w[i].title = titles[i]; w[i].subtitle = subs[i]; w[i].kind = "Program"; w[i].score = 100 - i;
        w[i].m.n = 4; w[i].m.pos[0] = at[i]; w[i].m.pos[1] = (unsigned short)(at[i] + 1); w[i].m.pos[2] = (unsigned short)(at[i] + 2); w[i].m.pos[3] = (unsigned short)(at[i] + 3);
    }
    if (calc_mode) {
        struct ln_row cr;
        memset(&cr, 0, sizeof cr);
        cr.key = 1; cr.title = "13.3333333333"; cr.subtitle = "(2+3)*4/1.5"; cr.kind = "Result"; cr.score = 1;
        ln_results_answer(r, q, LN_SRC_CALC, &cr, 1);
        ln_results_answer(r, q, LN_SRC_PROGRAMS, w, 2);
    } else
    ln_results_answer(r, q, LN_SRC_PROGRAMS, w, 5);
    memset(&v, 0, sizeof v);
    v.results = r; v.query = "word"; v.qn = 4; v.caret = 4; v.caret_on = 1;
    ln_layout_make(&l, s, r, 0, shadow);
    ox = (W - l.w) / 2; oy = 60;
    desktop(rgb, W, H);
    if (glass) {
        back = malloc((size_t)l.pw * (size_t)l.ph * 4);
        for (y = 0; y < l.ph; y++)
            for (x = 0; x < l.pw; x++) {
                const unsigned char *d = rgb + ((size_t)(oy + l.py + y) * W + (size_t)(ox + l.px + x)) * 3;
                unsigned char *b = back + ((size_t)y * (size_t)l.pw + (size_t)x) * 4;
                b[0] = 255; b[1] = d[0]; b[2] = d[1]; b[3] = d[2];
            }
        blur(back, l.pw, l.ph, 9);
        v.backdrop = back;
    }
    pic = malloc((size_t)l.w * (size_t)l.h * 4);
    ln_draw(pic, &l, s, &v, &f);
    for (y = 0; y < l.h; y++)
        for (x = 0; x < l.w; x++) {
            const unsigned char *p = pic + ((size_t)y * (size_t)l.w + (size_t)x) * 4;
            unsigned char *d = rgb + ((size_t)(oy + y) * W + (size_t)(ox + x)) * 3;
            int a = p[0];
            if (!glass) a = a >= 128 ? 255 : 0;       /* a window shape has no partial pixels */
            d[0] = (unsigned char)((p[1] * a + d[0] * (255 - a)) / 255);
            d[1] = (unsigned char)((p[2] * a + d[1] * (255 - a)) / 255);
            d[2] = (unsigned char)((p[3] * a + d[2] * (255 - a)) / 255);
        }
    snprintf(path, sizeof path, "%s/%s.png", dir, name);
    png(path, rgb, W, H);
    printf("%s\n", path);
    free(rgb); free(pic); free(back);
    zt_font_close(f.query); zt_font_close(f.title); zt_font_close(f.small); zt_font_close(f.big);
    ln_results_free(r);
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "render_panel font.ttf outdir\n"); return 2; }
    one(argv[2], "dark", argv[1], 1, 0, 1);
    one(argv[2], "light", argv[1], 0, 0, 1);
    one(argv[2], "compact", argv[1], 1, 1, 1);
    one(argv[2], "plain", argv[1], 1, 0, 0);
    calc_mode = 1;
    one(argv[2], "calc", argv[1], 1, 0, 1);
    one(argv[2], "calc-light", argv[1], 0, 0, 1);
    one(argv[2], "calc-compact", argv[1], 1, 1, 1);
    calc_mode = 0;
    /* LN_THEMES names theme files (Limpet's format), separated by ':':
       each gives <name>-dark.png and <name>-light.png, one of them made
       from the other */
    if (getenv("LN_THEMES")) {
        char *list = strdup(getenv("LN_THEMES")), *file;
        for (file = strtok(list, ":"); file; file = strtok(NULL, ":")) {
            static char text[16384];
            struct ln_theme t;
            char name[160], *base = strrchr(file, '/'), *dot;
            FILE *f = fopen(file, "rb");
            size_t n = f ? fread(text, 1, sizeof text, f) : 0;
            if (f) fclose(f);
            if (!ln_theme_parse(text, n, &t)) { fprintf(stderr, "not a theme: %s\n", file); continue; }
            snprintf(name, sizeof name - 8, "theme-%s", base ? base + 1 : file);
            if ((dot = strrchr(name, '.'))) *dot = 0;
            dot = name + strlen(name);
            render_theme = &t;
            strcpy(dot, ln_theme_dark(&t) ? "-dark" : "-dark-made");
            one(argv[2], name, argv[1], 1, 0, 1);
            strcpy(dot, ln_theme_dark(&t) ? "-light-made" : "-light");
            one(argv[2], name, argv[1], 0, 0, 1);
            render_theme = NULL;
        }
        free(list);
    }
    return 0;
}
