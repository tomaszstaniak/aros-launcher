/* SPDX-License-Identifier: MIT */
#include "cliphist.h"

#include <stdlib.h>
#include <string.h>

static int blank(const char *text, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if ((unsigned char)text[i] > 32) return 0;
    return 1;
}

static void drop(struct ln_cliphist *h, int i)
{
    free(h->v[i].text);
    memmove(&h->v[i], &h->v[i + 1], (size_t)(h->n - i - 1) * sizeof h->v[0]);
    h->n--;
    memset(&h->v[h->n], 0, sizeof h->v[0]);
}

int ln_cliphist_add(struct ln_cliphist *h, const char *text, size_t n, unsigned when)
{
    struct ln_clip c;
    int i;
    if (!text || !n || n > LN_CLIP_TEXT_MAX || blank(text, n)) return 0;
    for (i = 0; i < h->n; i++)
        if (h->v[i].n == n && !memcmp(h->v[i].text, text, n)) {
            if (!i) { h->v[0].when = when; return 0; }
            c = h->v[i];
            memmove(&h->v[1], &h->v[0], (size_t)i * sizeof h->v[0]);
            c.when = when;
            h->v[0] = c;
            return 1;
        }
    if (!(c.text = malloc(n + 1))) return 0;
    memcpy(c.text, text, n);
    c.text[n] = 0;
    c.n = n;
    c.when = when;
    if (!++h->next_id) h->next_id = 1;
    c.id = h->next_id;
    if (h->n == LN_CLIP_MAX) drop(h, LN_CLIP_MAX - 1);
    memmove(&h->v[1], &h->v[0], (size_t)h->n * sizeof h->v[0]);
    h->v[0] = c;
    h->n++;
    return 1;
}

const struct ln_clip *ln_cliphist_find(const struct ln_cliphist *h, unsigned id)
{
    int i;
    for (i = 0; i < h->n; i++)
        if (h->v[i].id == id) return &h->v[i];
    return NULL;
}

void ln_cliphist_remove(struct ln_cliphist *h, unsigned id)
{
    int i;
    for (i = 0; i < h->n; i++)
        if (h->v[i].id == id) { drop(h, i); return; }
}

void ln_cliphist_clear(struct ln_cliphist *h)
{
    while (h->n) drop(h, h->n - 1);
}

void ln_cliphist_title(const struct ln_clip *c, char *out, size_t cap)
{
    size_t i = 0, k = 0;
    int space = 0;
    if (!cap) return;
    while (i < c->n && (unsigned char)c->text[i] <= 32) i++;
    while (i < c->n) {
        unsigned char ch = (unsigned char)c->text[i];
        size_t len, need;
        if (ch == '\n' || ch == '\r') break;
        if (ch <= 32 || ch == 127) { space = 1; i++; continue; }
        len = ch < 0x80 ? 1 : (ch & 0xe0) == 0xc0 ? 2 : (ch & 0xf0) == 0xe0 ? 3 : (ch & 0xf8) == 0xf0 ? 4 : 1;
        if (i + len > c->n) len = c->n - i;
        need = len + (space ? 1 : 0);
        /* a character goes in whole or not at all */
        if (k + need + 1 > cap) break;
        if (space) { out[k++] = ' '; space = 0; }
        memcpy(out + k, c->text + i, len);
        k += len;
        i += len;
    }
    out[k] = 0;
}

static int fold(int ch) { return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch; }

int ln_cliphist_search(const struct ln_clip *c, const char *q, size_t qn, char *line, size_t cap)
{
    size_t i, k, from, to;
    struct ln_clip part;
    if (!qn || qn > c->n) return 0;
    for (i = 0; i + qn <= c->n; i++) {
        for (k = 0; k < qn && fold((unsigned char)c->text[i + k]) == fold((unsigned char)q[k]); k++) ;
        if (k == qn) break;
    }
    if (i + qn > c->n) return 0;
    for (from = i; from > 0 && c->text[from - 1] != '\n' && c->text[from - 1] != '\r'; from--) ;
    for (to = i; to < c->n && c->text[to] != '\n' && c->text[to] != '\r'; to++) ;
    /* a long line: start a little before the place, on a character's first byte */
    if (cap > 24 && i - from > cap / 3) {
        from = i - cap / 3;
        while (from < i && ((unsigned char)c->text[from] & 0xc0) == 0x80) from++;
    }
    part.text = c->text + from;
    part.n = to - from;
    part.id = c->id;
    part.when = c->when;
    ln_cliphist_title(&part, line, cap);
    return 1;
}
