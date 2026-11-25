/* SPDX-License-Identifier: MIT */
/* clipimg WRITE <width> <height> <seed>: puts a 24-bit ILBM on the clipboard,
 * the way a paint program's Copy does. clipimg READ: says what the
 * clipboard holds (FORM type, size, a checksum of all bytes), the way a
 * program's Paste would read it. Both go through iffparse.library, not
 * through the code the launcher uses. */
#include <exec/memory.h>
#include <libraries/iffparse.h>
#include <proto/exec.h>
#include <proto/iffparse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_ILBM MAKE_ID('I','L','B','M')
#define ID_BMHD MAKE_ID('B','M','H','D')
#define ID_BODY MAKE_ID('B','O','D','Y')

struct Library *IFFParseBase;

static int do_write(int w, int h, unsigned seed)
{
    struct IFFHandle *iff = AllocIFF();
    UBYTE bmhd[20];
    ULONG rowbytes = (ULONG)((w + 15) / 16) * 2, sum = 0, total = 0;
    UBYTE *row = malloc(rowbytes);
    int ok = 0, y, pl, x;
    if (!iff || !row) return 0;
    memset(bmhd, 0, sizeof bmhd);
    bmhd[0] = (UBYTE)(w >> 8); bmhd[1] = (UBYTE)w; bmhd[2] = (UBYTE)(h >> 8); bmhd[3] = (UBYTE)h;
    bmhd[8] = 24; bmhd[14] = 1; bmhd[15] = 1;
    bmhd[16] = (UBYTE)(w >> 8); bmhd[17] = (UBYTE)w; bmhd[18] = (UBYTE)(h >> 8); bmhd[19] = (UBYTE)h;
    if ((iff->iff_Stream = (IPTR)OpenClipboard(0))) {
        InitIFFasClip(iff);
        if (!OpenIFF(iff, IFFF_WRITE)) {
            ok = !PushChunk(iff, ID_ILBM, ID_FORM, IFFSIZE_UNKNOWN) && !PushChunk(iff, 0, ID_BMHD, sizeof bmhd)
                 && WriteChunkBytes(iff, bmhd, sizeof bmhd) == sizeof bmhd && !PopChunk(iff)
                 && !PushChunk(iff, 0, ID_BODY, IFFSIZE_UNKNOWN);
            for (y = 0; ok && y < h; y++)
                for (pl = 0; ok && pl < 24; pl++) {
                    memset(row, 0, rowbytes);
                    for (x = 0; x < w; x++) {
                        /* three bands of the seed's colours with a diagonal through them */
                        ULONG band = (ULONG)(x * 3 / w), v = ((seed >> (band * 8)) & 255) << (band * 8);
                        if (x == y || x == y + 1) v = 0xffffff;
                        if (v & (1UL << pl)) row[x >> 3] |= (UBYTE)(0x80 >> (x & 7));
                    }
                    for (x = 0; x < (int)rowbytes; x++) sum = sum * 31 + row[x];
                    total += rowbytes;
                    ok = WriteChunkBytes(iff, row, (LONG)rowbytes) == (LONG)rowbytes;
                }
            ok = ok && !PopChunk(iff) && !PopChunk(iff);
            CloseIFF(iff);
        }
        CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
    }
    FreeIFF(iff);
    free(row);
    printf("wrote=%d %dx%d body=%lu bodysum=%08lx\n", ok, w, h, (unsigned long)total, (unsigned long)sum);
    return ok;
}

static int do_read(void)
{
    struct IFFHandle *iff = AllocIFF();
    int ok = 0;
    if (!iff) return 0;
    if ((iff->iff_Stream = (IPTR)OpenClipboard(0))) {
        InitIFFasClip(iff);
        if (!OpenIFF(iff, IFFF_READ)) {
            if (!StopChunk(iff, ID_ILBM, ID_BODY) && !PropChunk(iff, ID_ILBM, ID_BMHD) && !ParseIFF(iff, IFFPARSE_SCAN)) {
                struct StoredProperty *sp = FindProp(iff, ID_ILBM, ID_BMHD);
                struct ContextNode *cn = CurrentChunk(iff);
                UBYTE buf[4096];
                ULONG sum = 0, total = 0;
                LONG got, i;
                while ((got = ReadChunkBytes(iff, buf, sizeof buf)) > 0) {
                    for (i = 0; i < got; i++) sum = sum * 31 + buf[i];
                    total += (ULONG)got;
                }
                if (sp && sp->sp_Size >= 20) {
                    const UBYTE *b = sp->sp_Data;
                    printf("picture %dx%d depth=%d body=%lu bodysum=%08lx (chunk says %ld)\n", b[0] << 8 | b[1], b[2] << 8 | b[3], b[8],
                           (unsigned long)total, (unsigned long)sum, cn ? (long)cn->cn_Size : -1L);
                    ok = 1;
                }
            } else printf("no picture on the clipboard\n");
            CloseIFF(iff);
        }
        CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
    }
    FreeIFF(iff);
    return ok;
}

int main(int argc, char **argv)
{
    int ok = 0;
    if (!(IFFParseBase = OpenLibrary((CONST_STRPTR)"iffparse.library", 39))) return 20;
    if (argc >= 5 && !strcmp(argv[1], "WRITE")) ok = do_write(atoi(argv[2]), atoi(argv[3]), (unsigned)strtoul(argv[4], NULL, 16));
    else if (argc >= 2 && !strcmp(argv[1], "READ")) ok = do_read();
    else printf("clipimg WRITE <width> <height> <hex seed> | READ\n");
    CloseLibrary(IFFParseBase);
    return ok ? 0 : 10;
}
