/* SPDX-License-Identifier: MIT */
/* IFF ILBM pictures as the clipboard holds them: enough to say what a
 * picture is and to make a small one of it. Plain C, no system calls. */
#ifndef LN_ILBM_H
#define LN_ILBM_H

#include <stddef.h>

struct ln_ilbm {
    int w, h, depth;                /* depth: bit planes, 1..8 with a palette, 24 or 32 */
    int masking, compression;       /* as in BMHD; compression 0 or 1 (ByteRun1) */
    const unsigned char *cmap;      /* R,G,B triples, NULL without CMAP */
    int colours;
    const unsigned char *body;
    size_t body_n;
};

/* Reads the chunks of a FORM ILBM. 0 when data is not one this code can
 * draw (another FORM, a depth or compression it does not know, no BODY). */
int ln_ilbm_parse(const unsigned char *data, size_t n, struct ln_ilbm *out);

/* Draws the picture into size x size pixels of A,R,G,B bytes, whole and
 * centred, the rest transparent. 0 when the BODY ends too early or there
 * is no memory. */
int ln_ilbm_thumb(const struct ln_ilbm *im, int size, unsigned char *argb);

#endif
