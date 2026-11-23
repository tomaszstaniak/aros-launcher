/* SPDX-License-Identifier: MIT */
#include "fileindex.h"

#include <stdlib.h>
#include <string.h>

/* Text lives in blocks that are never moved, so entries can point into
 * them. Drawers are stored once: entries of one drawer follow each other
 * while a folder is walked, and the last drawer is remembered. */
struct block { struct block *next; size_t used, cap; char text[1]; };

struct entry { const char *dir; const char *name; unsigned char is_dir, len; };

struct ln_fileindex {
    struct entry *e;
    int n, cap;
    struct block *blocks;
    const char *last_dir;
    int full;
};

#define BLOCK (256 * 1024)

static const char *keep(struct ln_fileindex *x, const char *s, size_t len)
{
    struct block *b = x->blocks;
    char *at;
    if (!b || b->used + len + 1 > b->cap) {
        size_t cap = len + 1 > BLOCK ? len + 1 : BLOCK;
        if (!(b = malloc(sizeof *b + cap))) return NULL;
        b->next = x->blocks; b->used = 0; b->cap = cap;
        x->blocks = b;
    }
    at = b->text + b->used;
    memcpy(at, s, len);
    at[len] = 0;
    b->used += len + 1;
    return at;
}

struct ln_fileindex *ln_fileindex_new(void) { return calloc(1, sizeof(struct ln_fileindex)); }

void ln_fileindex_free(struct ln_fileindex *x)
{
    struct block *b, *next;
    if (!x) return;
    for (b = x->blocks; b; b = next) { next = b->next; free(b); }
    free(x->e);
    free(x);
}

int ln_fileindex_count(const struct ln_fileindex *x) { return x->n; }

int ln_fileindex_add(struct ln_fileindex *x, const char *dir, const char *name, int is_dir)
{
    size_t len = strlen(name);
    const char *d, *nm;
    if (!len) return 0;
    if (x->n >= LN_FILES_MAX) { x->full = 1; return 0; }
    if (x->n == x->cap) {
        int cap = x->cap ? x->cap * 2 : 4096;
        struct entry *g = realloc(x->e, sizeof *g * (size_t)cap);
        if (!g) return 0;
        x->e = g; x->cap = cap;
    }
    if (x->last_dir && !strcmp(x->last_dir, dir)) d = x->last_dir;
    else if (!(d = keep(x, dir, strlen(dir)))) return 0;
    if (!(nm = keep(x, name, len))) return 0;
    x->last_dir = d;
    x->e[x->n].dir = d;
    x->e[x->n].name = nm;
    x->e[x->n].is_dir = is_dir != 0;
    x->e[x->n].len = (unsigned char)(len > 255 ? 255 : len);
    x->n++;
    return 1;
}

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

int ln_fileindex_full(const struct ln_fileindex *x) { return x->full; }

/* Whether dir is folder or lies below it. */
static int under(const char *dir, const char *folder)
{
    size_t n = strlen(folder);
    size_t i;
    while (n && folder[n - 1] == '/') n--;
    for (i = 0; i < n; i++)
        if (lower((unsigned char)dir[i]) != lower((unsigned char)folder[i])) return 0;
    return !dir[n] || dir[n] == '/' || (n && folder[n - 1] == ':');
}

int ln_fileindex_keep(struct ln_fileindex *x, const struct ln_fileindex *src, const char *folder)
{
    int i, kept = 0;
    if (!src) return 0;
    for (i = 0; i < src->n; i++) {
        const struct entry *e = &src->e[i];
        if (!under(e->dir, folder)) continue;
        if (!ln_fileindex_add(x, e->dir, e->name, e->is_dir)) return -1;
        kept++;
    }
    return kept;
}

unsigned ln_path_hash(const char *path)
{
    unsigned h = 2166136261u;
    while (*path) { h ^= (unsigned char)*path++; h *= 16777619u; }
    return h;
}

/* Where word (lower case, wn bytes) occurs in name, -1 when it does not. */
static int find(const char *name, const char *word, size_t wn)
{
    const char *p;
    int first = word[0];
    for (p = name; *p; p++) {
        size_t k;
        if (lower((unsigned char)*p) != first) continue;
        for (k = 1; k < wn && p[k] && lower((unsigned char)p[k]) == word[k]; k++) ;
        if (k == wn) return (int)(p - name);
    }
    return -1;
}

#define WORDS 6

struct hit { int rank; int index; };

int ln_fileindex_query(const struct ln_fileindex *x, const char *q, size_t qn, struct ln_file *out, int max)
{
    char words[WORDS][48];
    size_t wlen[WORDS];
    struct hit *h;
    int nw = 0, n = 0, i, j, long_enough = 0;
    size_t at = 0;

    while (at < qn && nw < WORDS) {
        size_t k = 0;
        while (at < qn && q[at] == ' ') at++;
        while (at < qn && q[at] != ' ' && k + 1 < sizeof words[0]) words[nw][k++] = (char)lower((unsigned char)q[at++]);
        while (at < qn && q[at] != ' ') at++;
        if (!k) break;
        words[nw][k] = 0;
        wlen[nw] = k;
        if (k >= 2) long_enough = 1;
        nw++;
    }
    if (!nw || !long_enough || max <= 0) return 0;
    if (!(h = malloc(sizeof *h * (size_t)max))) return 0;

    for (i = 0; i < x->n; i++) {
        const struct entry *e = &x->e[i];
        int pos0 = -1, rank, w, ok = 1;
        for (w = 0; w < nw && ok; w++) {
            int pos = find(e->name, words[w], wlen[w]);
            if (pos < 0) ok = 0;
            else if (w == 0) pos0 = pos;
        }
        if (!ok) continue;
        /* lower is better */
        if (pos0 == 0) rank = 0;
        else {
            int before = (unsigned char)e->name[pos0 - 1];
            rank = (before == ' ' || before == '-' || before == '_' || before == '.') ? 1000 : 2000;
        }
        rank += e->len * 2 + (e->is_dir ? 0 : 1);
        if (n == max && rank >= h[n - 1].rank) continue;
        j = n < max ? n++ : n - 1;
        while (j > 0 && rank < h[j - 1].rank) { h[j] = h[j - 1]; j--; }
        h[j].rank = rank;
        h[j].index = i;
    }
    for (i = 0; i < n; i++) {
        const struct entry *e = &x->e[h[i].index];
        out[i].dir = e->dir; out[i].name = e->name; out[i].is_dir = e->is_dir;
    }
    free(h);
    return n;
}

char *ln_fileindex_save(const struct ln_fileindex *x, size_t *n)
{
    size_t cap = 16, len = 0;
    char *out;
    int i;
    for (i = 0; i < x->n; i++) cap += strlen(x->e[i].dir) + strlen(x->e[i].name) + 5;
    if (!(out = malloc(cap))) return NULL;
    for (i = 0; i < x->n; i++) {
        const struct entry *e = &x->e[i];
        size_t dl = strlen(e->dir), nl = strlen(e->name);
        /* a name with a tab or a line feed cannot be written in this form */
        if (strpbrk(e->dir, "\t\n") || strpbrk(e->name, "\t\n")) continue;
        out[len++] = e->is_dir ? 'D' : 'F';
        out[len++] = '\t';
        memcpy(out + len, e->dir, dl); len += dl;
        out[len++] = '\t';
        memcpy(out + len, e->name, nl); len += nl;
        out[len++] = '\n';
    }
    out[len] = 0;
    if (n) *n = len;
    return out;
}

int ln_fileindex_load(struct ln_fileindex *x, const char *text, size_t n)
{
    size_t at = 0;
    int taken = 0;
    char dir[1024], name[300];
    while (at < n) {
        size_t end = at, t1, dl, nl;
        while (end < n && text[end] != '\n') end++;
        if (end - at >= 5 && (text[at] == 'D' || text[at] == 'F') && text[at + 1] == '\t') {
            for (t1 = at + 2; t1 < end && text[t1] != '\t'; t1++) ;
            dl = t1 - (at + 2);
            nl = t1 < end ? end - t1 - 1 : 0;
            if (t1 < end && dl && nl && dl < sizeof dir && nl < sizeof name) {
                memcpy(dir, text + at + 2, dl); dir[dl] = 0;
                memcpy(name, text + t1 + 1, nl); name[nl] = 0;
                if (ln_fileindex_add(x, dir, name, text[at] == 'D')) taken++;
            }
        }
        at = end + 1;
    }
    return taken;
}
