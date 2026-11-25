#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/ilbm.h"

static size_t put32(unsigned char *p, unsigned long v) { p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16); p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v; return 4; }

/* A w x h picture of `depth` planes: the left half red (or colour 1), the
 * right half blue (or colour 2); packed with ByteRun1 when asked. */
static size_t make(unsigned char *out, int w, int h, int depth, int packed)
{
    size_t at = 12, rowbytes = (size_t)((w + 15) / 16) * 2, body_at, y;
    int pl, x;
    unsigned char row[64];
    memcpy(out, "FORM", 4); memcpy(out + 8, "ILBM", 4);
    memcpy(out + at, "BMHD", 4); at += 4; at += put32(out + at, 20);
    memset(out + at, 0, 20);
    out[at] = (unsigned char)(w >> 8); out[at + 1] = (unsigned char)w; out[at + 2] = (unsigned char)(h >> 8); out[at + 3] = (unsigned char)h;
    out[at + 8] = (unsigned char)depth; out[at + 10] = (unsigned char)packed;
    at += 20;
    if (depth <= 8) {
        static const unsigned char cmap[] = { 0,0,0, 255,0,0, 0,0,255, 9,9,9 };
        memcpy(out + at, "CMAP", 4); at += 4; at += put32(out + at, 12);
        memcpy(out + at, cmap, 12); at += 12;
    }
    memcpy(out + at, "BODY", 4); at += 4; body_at = at; at += 4;
    for (y = 0; y < (size_t)h; y++)
        for (pl = 0; pl < depth; pl++) {
            memset(row, 0, sizeof row);
            for (x = 0; x < w; x++) {
                unsigned long v = depth <= 8 ? (x < w / 2 ? 1ul : 2ul) : (x < w / 2 ? 0x0000fful : 0xff0000ul);
                if (v & (1ul << pl)) row[x >> 3] |= (unsigned char)(0x80u >> (x & 7));
            }
            if (!packed) { memcpy(out + at, row, rowbytes); at += rowbytes; }
            else {
                /* each byte as a run of one literal, and a no-op now and then */
                size_t i;
                for (i = 0; i < rowbytes; i++) {
                    if (i && row[i] == row[i - 1] && i + 1 < rowbytes && row[i + 1] == row[i]) { out[at++] = (unsigned char)-1; out[at++] = row[i]; i++; }
                    else { out[at++] = 0; out[at++] = row[i]; }
                }
                if (y == 0 && pl == 0) out[at++] = 0x80;
            }
        }
    put32(out + body_at, (unsigned long)(at - body_at - 4));
    if (at & 1) out[at++] = 0;
    put32(out + 4, (unsigned long)(at - 8));
    return at;
}

int main(void)
{
    static unsigned char data[64 * 1024], thumb[16 * 16 * 4];
    struct ln_ilbm im;
    size_t n;
    int packed, depth;
    for (packed = 0; packed < 2; packed++)
        for (depth = 2; depth <= 24; depth += 22) {
            const unsigned char *l, *r;
            n = make(data, 40, 20, depth, packed);
            assert(ln_ilbm_parse(data, n, &im));
            assert(im.w == 40 && im.h == 20 && im.depth == depth && im.compression == packed);
            assert(ln_ilbm_thumb(&im, 16, thumb));
            /* 40 x 20 into 16: 16 x 8, four rows clear above and below */
            assert(thumb[0] == 0 && thumb[(3 * 16 + 5) * 4] == 0);
            l = thumb + (7 * 16 + 2) * 4; r = thumb + (7 * 16 + 13) * 4;
            assert(l[0] == 255 && l[1] == 255 && l[2] == 0 && l[3] == 0);
            assert(r[0] == 255 && r[1] == 0 && r[2] == 0 && r[3] == 255);
            assert(thumb[(12 * 16 + 5) * 4] == 0);
        }
    /* not a picture, and one cut short */
    assert(!ln_ilbm_parse((const unsigned char *)"FORM\0\0\0\4FTXT", 12, &im));
    n = make(data, 40, 20, 24, 0);
    assert(ln_ilbm_parse(data, n - 400, &im));
    assert(ln_ilbm_thumb(&im, 16, thumb) == 0);     /* says so */
    assert(thumb[(5 * 16 + 2) * 4] == 255);         /* and still shows the rows that were there */
    n = make(data, 40, 20, 24, 0);
    data[12 + 8 + 10] = 7;                          /* a compression it does not know */
    assert(!ln_ilbm_parse(data, n, &im));
    puts("ilbm ok");
    return 0;
}
