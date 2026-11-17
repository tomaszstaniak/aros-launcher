/* SPDX-License-Identifier: MIT */
#include "index.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct slot {
    struct ln_entry e;
    int present;    /* a program; 0: only counts known so far */
    int seen;       /* put since the last ln_index_begin that covers it */
};

struct ln_index {
    struct slot *s;
    int n, cap;
    unsigned next_id;
};

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int same_path(const char *a, const char *b)
{
    while (*a && *b) {
        if (lower((unsigned char)*a) != lower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return !*a && !*b;
}

/* path lies in dir: dir ends in ':' or '/', or the next character is '/'. */
static int inside(const char *path, const char *dir)
{
    size_t n = strlen(dir), i;
    if (!n) return 0;
    for (i = 0; i < n; i++)
        if (lower((unsigned char)path[i]) != lower((unsigned char)dir[i])) return 0;
    if (dir[n - 1] == ':' || dir[n - 1] == '/') return path[n] != 0;
    return path[n] == '/';
}

static char *copy(const char *s)
{
    size_t n = strlen(s) + 1;
    char *c = malloc(n);
    if (c) memcpy(c, s, n);
    return c;
}

static struct slot *lookup(const struct ln_index *x, const char *path)
{
    int i;
    for (i = 0; i < x->n; i++)
        if (same_path(x->s[i].e.path, path)) return &x->s[i];
    return NULL;
}

static struct slot *add(struct ln_index *x, const char *path)
{
    struct slot *s;
    if (x->n == x->cap) {
        int cap = x->cap ? x->cap * 2 : 64;
        struct slot *g = realloc(x->s, sizeof *g * (size_t)cap);
        if (!g) return NULL;
        x->s = g;
        x->cap = cap;
    }
    s = &x->s[x->n];
    memset(s, 0, sizeof *s);
    if (!(s->e.path = copy(path))) return NULL;
    s->e.id = ++x->next_id;
    x->n++;
    return s;
}

struct ln_index *ln_index_new(void)
{
    return calloc(1, sizeof(struct ln_index));
}

void ln_index_free(struct ln_index *x)
{
    int i;
    if (!x) return;
    for (i = 0; i < x->n; i++) { free(x->s[i].e.path); free(x->s[i].e.title); }
    free(x->s);
    free(x);
}

unsigned ln_index_put(struct ln_index *x, const char *path, const char *title)
{
    struct slot *s = lookup(x, path);
    char *t = copy(title);
    if (!t) return 0;
    if (!s && !(s = add(x, path))) { free(t); return 0; }
    free(s->e.title);
    s->e.title = t;
    s->present = 1;
    s->seen = 1;
    return s->e.id;
}

void ln_index_remove(struct ln_index *x, const char *path)
{
    struct slot *s = lookup(x, path);
    /* The counts stay: the program may come back. */
    if (s) s->present = 0;
}

const struct ln_entry *ln_index_find(const struct ln_index *x, unsigned id)
{
    int i;
    for (i = 0; i < x->n; i++)
        if (x->s[i].e.id == id && x->s[i].present) return &x->s[i].e;
    return NULL;
}

int ln_index_count(const struct ln_index *x)
{
    int i, c = 0;
    for (i = 0; i < x->n; i++) c += x->s[i].present;
    return c;
}

void ln_index_begin(struct ln_index *x, const char *dir)
{
    int i;
    for (i = 0; i < x->n; i++)
        if (inside(x->s[i].e.path, dir)) x->s[i].seen = 0;
}

void ln_index_end(struct ln_index *x, const char *dir)
{
    int i;
    for (i = 0; i < x->n; i++)
        if (!x->s[i].seen && inside(x->s[i].e.path, dir)) x->s[i].present = 0;
}

void ln_index_used(struct ln_index *x, unsigned id, unsigned now)
{
    int i;
    for (i = 0; i < x->n; i++)
        if (x->s[i].e.id == id) { x->s[i].e.uses++; x->s[i].e.last_use = now; return; }
}

struct hit { const struct ln_entry *e; struct ln_match m; int rank; };

static int better(const struct hit *a, const struct hit *b)
{
    if (a->rank != b->rank) return a->rank > b->rank;
    if (a->e->last_use != b->e->last_use) return a->e->last_use > b->e->last_use;
    return strcmp(a->e->title, b->e->title) < 0;
}

int ln_index_query(const struct ln_index *x, const char *q, size_t qn,
                   const struct ln_entry **out, struct ln_match *m, int max)
{
    struct hit *h, cur;
    int i, j, n = 0;
    if (max <= 0) return 0;
    if (!(h = malloc(sizeof *h * (size_t)max))) return 0;
    for (i = 0; i < x->n; i++) {
        const struct slot *s = &x->s[i];
        unsigned uses;
        if (!s->present) continue;
        if (!ln_match(q, qn, s->e.title, strlen(s->e.title), &cur.m)) continue;
        uses = s->e.uses > 20 ? 20 : s->e.uses;
        cur.e = &s->e;
        cur.rank = cur.m.score + (int)uses * 3;
        /* insertion into the best max so far */
        if (n == max && !better(&cur, &h[n - 1])) continue;
        j = n < max ? n++ : n - 1;
        while (j > 0 && better(&cur, &h[j - 1])) { h[j] = h[j - 1]; j--; }
        h[j] = cur;
    }
    for (i = 0; i < n; i++) {
        out[i] = h[i].e;
        if (m) m[i] = h[i].m;
    }
    free(h);
    return n;
}

int ln_index_load_counts(struct ln_index *x, const char *text, size_t n)
{
    size_t at = 0;
    int taken = 0;
    while (at < n) {
        size_t end = at, len;
        char line[1024], *p, *tab1, *tab2;
        unsigned long uses, last;
        while (end < n && text[end] != '\n') end++;
        len = end - at;
        if (len && len < sizeof line) {
            memcpy(line, text + at, len);
            line[len] = 0;
            if (len && line[len - 1] == '\r') line[len - 1] = 0;
            tab1 = strchr(line, '\t');
            tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
            if (tab2 && tab2[1]) {
                uses = strtoul(line, &p, 10);
                if (p == tab1 && p != line) {
                    last = strtoul(tab1 + 1, &p, 10);
                    if (p == tab2 && p != tab1 + 1) {
                        struct slot *s = lookup(x, tab2 + 1);
                        if (!s) s = add(x, tab2 + 1);
                        if (s) {
                            s->e.uses = (unsigned)uses;
                            s->e.last_use = (unsigned)last;
                            taken++;
                        }
                    }
                }
            }
        }
        at = end + 1;
    }
    return taken;
}

char *ln_index_save_counts(const struct ln_index *x, size_t *n)
{
    size_t cap = 64, len = 0;
    char *out;
    int i;
    for (i = 0; i < x->n; i++)
        if (x->s[i].e.uses) cap += strlen(x->s[i].e.path) + 32;
    if (!(out = malloc(cap))) return NULL;
    out[0] = 0;
    for (i = 0; i < x->n; i++) {
        const struct ln_entry *e = &x->s[i].e;
        /* a path with a tab or line feed cannot be written in this form */
        if (!e->uses || strpbrk(e->path, "\t\n")) continue;
        len += (size_t)snprintf(out + len, cap - len, "%u\t%u\t%s\n", e->uses, e->last_use, e->path);
    }
    if (n) *n = len;
    return out;
}
