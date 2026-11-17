/* SPDX-License-Identifier: MIT */
#include "match.h"

#include <stdlib.h>
#include <string.h>

#include "utf8proc.h"
#include "zk_text.h"

#define TITLE_MAX 256
#define NONE (-1000000)

struct unit {
    unsigned short off;     /* byte offset of the cluster */
    unsigned char key[8];   /* its folded form, NUL padded */
    unsigned char space, upper, lower, sep;
};

/* Letters that have no decomposition but are typed without their stroke. */
static utf8proc_int32_t unstroke(utf8proc_int32_t c)
{
    switch (c) {
    case 0x142: return 'l';   /* ł */
    case 0xf8:  return 'o';   /* ø */
    case 0x111: return 'd';   /* đ */
    case 0x127: return 'h';   /* ħ */
    case 0x131: return 'i';   /* dotless i */
    default:    return c;
    }
}

/* The folded form of one cluster: compatibility decomposition, marks
 * dropped, case folded, strokes removed. Longer forms are cut; they still
 * compare equal to themselves. */
static void fold(const char *s, size_t n, unsigned char key[8])
{
    utf8proc_int32_t cp[16];
    utf8proc_ssize_t r, i;
    size_t k = 0;
    memset(key, 0, 8);
    r = utf8proc_decompose((const utf8proc_uint8_t *)s, (utf8proc_ssize_t)n, cp, 16,
                           UTF8PROC_DECOMPOSE | UTF8PROC_COMPAT | UTF8PROC_STRIPMARK | UTF8PROC_CASEFOLD);
    if (r < 0) return;
    if (r > 16) r = 16;
    for (i = 0; i < r; i++) {
        utf8proc_uint8_t b[4];
        utf8proc_ssize_t len = utf8proc_encode_char(unstroke(cp[i]), b);
        if (k + (size_t)len > 8) break;
        memcpy(key + k, b, (size_t)len);
        k += (size_t)len;
    }
}

static int units(const char *s, size_t n, struct unit *u, int max, int skip_spaces)
{
    size_t off = 0;
    int count = 0;
    while (off < n && count < max && off <= 0xffff) {
        size_t next = zt_next_cluster(s, n, off), len;
        utf8proc_int32_t c;
        struct unit *x = &u[count];
        if (next <= off) break;
        len = next - off;
        if (utf8proc_iterate((const utf8proc_uint8_t *)s + off, (utf8proc_ssize_t)len, &c) < 0) c = 0;
        x->off = (unsigned short)off;
        x->space = (c == ' ' || c == '\t' || c == 0xa0);
        x->sep = x->space || c == '-' || c == '_' || c == '.';
        x->upper = utf8proc_isupper(c) ? 1 : 0;
        x->lower = utf8proc_islower(c) ? 1 : 0;
        fold(s + off, len, x->key);
        off = next;
        if (skip_spaces && x->space) continue;
        count++;
    }
    return count;
}

static int bonus(const struct unit *t, int j)
{
    if (j == 0) return 40;
    if (t[j - 1].sep || (t[j - 1].lower && t[j].upper)) return 20;
    return 0;
}

int ln_match(const char *query, size_t qn, const char *title, size_t tn, struct ln_match *out)
{
    struct unit *q, *t;
    int *d, *m;
    int nq, nt, i, j, ok = 0;

    out->score = 0;
    out->n = 0;
    if (!zt_valid(query, qn) || !zt_valid(title, tn)) return 0;
    q = malloc(sizeof *q * LN_MATCH_MAX);
    t = malloc(sizeof *t * TITLE_MAX);
    if (!q || !t) { free(q); free(t); return 0; }
    nq = units(query, qn, q, LN_MATCH_MAX, 1);
    nt = units(title, tn, t, TITLE_MAX, 0);
    if (nq == 0) { free(q); free(t); return 1; }
    if (nq > nt) { free(q); free(t); return 0; }

    /* d[i][j]: best score with query character i placed on title character j.
     * m[i][j]: best score for query characters 0..i within title 0..j. */
    d = malloc(sizeof *d * (size_t)nq * (size_t)nt * 2);
    if (!d) { free(q); free(t); return 0; }
    m = d + (size_t)nq * (size_t)nt;
#define D(i, j) d[(size_t)(i) * (size_t)nt + (size_t)(j)]
#define M(i, j) m[(size_t)(i) * (size_t)nt + (size_t)(j)]
    for (i = 0; i < nq; i++) {
        for (j = 0; j < nt; j++) {
            int s = NONE;
            if (j >= i && !memcmp(q[i].key, t[j].key, 8) && q[i].key[0]) {
                if (i == 0) {
                    s = bonus(t, j) - (j * 2 > 20 ? 20 : j * 2);
                } else if (j > 0) {
                    int after = D(i - 1, j - 1) > NONE ? D(i - 1, j - 1) + 15 : NONE;
                    int apart = M(i - 1, j - 1);
                    int best = after > apart ? after : apart;
                    if (best > NONE) s = best + bonus(t, j);
                }
            }
            D(i, j) = s;
            M(i, j) = (j > 0 && M(i, j - 1) > s) ? M(i, j - 1) : s;
        }
    }
    if (M(nq - 1, nt - 1) > NONE) {
        int want = M(nq - 1, nt - 1);
        ok = 1;
        out->score = want - (int)(tn / 8);
        out->n = nq;
        /* Walk back: the last title character where the wanted score was reached. */
        j = nt - 1;
        for (i = nq - 1; i >= 0; i--) {
            while (D(i, j) != want) j--;
            out->pos[i] = t[j].off;
            if (i > 0) {
                int after = D(i - 1, j - 1) > NONE ? D(i - 1, j - 1) + 15 : NONE;
                int here = want - bonus(t, j);
                if (after == here) want = D(i - 1, j - 1);      /* previous sits right before */
                else want = M(i - 1, j - 1);
                if (after == here) { j--; continue; }
                j--;
            }
        }
    }
#undef D
#undef M
    free(d); free(q); free(t);
    return ok;
}
