/* SPDX-License-Identifier: MIT */
#include "ilbm.h"

#include <stdlib.h>
#include <string.h>

static unsigned long be32(const unsigned char *p) { return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3]; }
static unsigned be16(const unsigned char *p) { return ((unsigned)p[0] << 8) | p[1]; }

int ln_ilbm_parse(const unsigned char *data, size_t n, struct ln_ilbm *out)
{
    size_t at = 12, end;
    int have_head = 0;
    memset(out, 0, sizeof *out);
    if (n < 12 || memcmp(data, "FORM", 4) || memcmp(data + 8, "ILBM", 4)) return 0;
    end = 8 + (size_t)be32(data + 4);
    if (end > n) end = n;
    while (at + 8 <= end) {
        const unsigned char *id = data + at;
        size_t size = (size_t)be32(data + at + 4), body = at + 8;
        if (size > end - body) size = end - body;       /* a chunk cut short keeps what is there */
        if (!memcmp(id, "BMHD", 4) && size >= 20) {
            out->w = (int)be16(data + body);
            out->h = (int)be16(data + body + 2);
            out->depth = data[body + 8];
            out->masking = data[body + 9];
            out->compression = data[body + 10];
            have_head = 1;
        } else if (!memcmp(id, "CMAP", 4)) {
            out->cmap = data + body;
            out->colours = (int)(size / 3);
        } else if (!memcmp(id, "BODY", 4)) {
            out->body = data + body;
            out->body_n = size;
        }
        at = body + size + (size & 1);
    }
    if (!have_head || !out->body || out->w <= 0 || out->h <= 0 || out->w > 32767 || out->h > 32767) return 0;
    if (out->compression != 0 && out->compression != 1) return 0;
    if (!((out->depth >= 1 && out->depth <= 8) || out->depth == 24 || out->depth == 32)) return 0;
    return 1;
}

/* One row of one plane into row (rowbytes bytes), or past it when row is
 * NULL. Returns the new place in the body, NULL when the body ran out. */
static const unsigned char *plane_row(const struct ln_ilbm *im, const unsigned char *p, const unsigned char *end,
                                      unsigned char *row, size_t rowbytes)
{
    size_t got = 0;
    if (!im->compression) {
        if ((size_t)(end - p) < rowbytes) return NULL;
        if (row) memcpy(row, p, rowbytes);
        return p + rowbytes;
    }
    while (got < rowbytes) {
        int c;
        size_t k;
        if (p >= end) return NULL;
        c = (signed char)*p++;
        if (c >= 0) {
            k = (size_t)c + 1;
            if ((size_t)(end - p) < k || got + k > rowbytes) return NULL;
            if (row) memcpy(row + got, p, k);
            p += k;
            got += k;
        } else if (c != -128) {
            k = (size_t)(-c) + 1;
            if (p >= end || got + k > rowbytes) return NULL;
            if (row) memset(row + got, *p, k);
            p++;
            got += k;
        }
    }
    return p;
}

int ln_ilbm_thumb(const struct ln_ilbm *im, int size, unsigned char *argb)
{
    int big = im->w > im->h ? im->w : im->h, tw, th, ox, oy, planes, y, ok = 1;
    int stepx, stepy;
    size_t rowbytes = (size_t)((im->w + 15) / 16) * 2;
    unsigned char *rows;
    unsigned long *sum;         /* per small pixel: r, g, b, a, count */
    const unsigned char *p = im->body, *end = im->body + im->body_n;

    if (size <= 0) return 0;
    memset(argb, 0, (size_t)size * (size_t)size * 4);
    tw = im->w * size / big; if (tw < 1) tw = 1;
    th = im->h * size / big; if (th < 1) th = 1;
    ox = (size - tw) / 2;
    oy = (size - th) / 2;
    planes = im->depth + (im->masking == 1 ? 1 : 0);
    /* up to four samples a side for each small pixel */
    stepx = im->w / (tw * 4); if (stepx < 1) stepx = 1;
    stepy = im->h / (th * 4); if (stepy < 1) stepy = 1;
    rows = malloc(rowbytes * (size_t)im->depth);
    sum = calloc((size_t)tw * (size_t)th * 5, sizeof *sum);
    if (!rows || !sum) { free(rows); free(sum); return 0; }

    for (y = 0; y < im->h && ok; y++) {
        int wanted = y % stepy == 0, pl, x, ty = y * th / im->h;
        for (pl = 0; pl < planes; pl++) {
            unsigned char *into = wanted && pl < im->depth ? rows + (size_t)pl * rowbytes : NULL;
            if (!(p = plane_row(im, p, end, into, rowbytes))) { ok = 0; break; }
        }
        if (!ok || !wanted) continue;
        for (x = 0; x < im->w; x += stepx) {
            unsigned long v = 0, *s = sum + ((size_t)ty * (size_t)tw + (size_t)(x * tw / im->w)) * 5;
            unsigned bit = 0x80u >> (x & 7);
            size_t byte = (size_t)x >> 3;
            for (pl = 0; pl < im->depth; pl++)
                if (rows[(size_t)pl * rowbytes + byte] & bit) v |= 1ul << pl;
            if (im->depth <= 8) {
                if (im->cmap && (int)v < im->colours) { s[0] += im->cmap[v * 3]; s[1] += im->cmap[v * 3 + 1]; s[2] += im->cmap[v * 3 + 2]; }
                else { unsigned long g = im->depth == 8 ? v : v * 255 / ((1ul << im->depth) - 1); s[0] += g; s[1] += g; s[2] += g; }
                s[3] += 255;
            } else {
                s[0] += v & 255; s[1] += (v >> 8) & 255; s[2] += (v >> 16) & 255;
                s[3] += im->depth == 32 ? (v >> 24) & 255 : 255;
            }
            s[4]++;
        }
    }
    /* a body that ended early still gives the rows that were there */
    {
        int tx, ty, any = 0;
        for (ty = 0; ty < th; ty++)
            for (tx = 0; tx < tw; tx++) {
                const unsigned long *s = sum + ((size_t)ty * (size_t)tw + (size_t)tx) * 5;
                unsigned char *o = argb + ((size_t)(oy + ty) * (size_t)size + (size_t)(ox + tx)) * 4;
                if (!s[4]) continue;
                o[0] = (unsigned char)(s[3] / s[4]); o[1] = (unsigned char)(s[0] / s[4]);
                o[2] = (unsigned char)(s[1] / s[4]); o[3] = (unsigned char)(s[2] / s[4]);
                any = 1;
            }
        if (!any) ok = 0;
    }
    free(rows);
    free(sum);
    return ok;
}
