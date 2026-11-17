/* SPDX-License-Identifier: MIT */
#include "pkglist.h"

#include <stdlib.h>
#include <string.h>

struct in { const char *s, *end; };

static void ws(struct in *p)
{
    while (p->s < p->end && (*p->s == ' ' || *p->s == '\n' || *p->s == '\r' || *p->s == '\t')) p->s++;
}

static int hex(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c |= 32;
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

/* A JSON string at p->s (on its opening quote) as malloc'd UTF-8; NULL
 * when it is not one or memory ran out. */
static char *string(struct in *p)
{
    size_t cap = (size_t)(p->end - p->s) + 1, k = 0;
    char *out;
    if (p->s >= p->end || *p->s != '"' || !(out = malloc(cap))) return NULL;
    p->s++;
    while (p->s < p->end && *p->s != '"') {
        unsigned char c = (unsigned char)*p->s++;
        if (c != '\\') { out[k++] = (char)c; continue; }
        if (p->s >= p->end) break;
        c = (unsigned char)*p->s++;
        switch (c) {
        case 'n': out[k++] = '\n'; break;
        case 't': out[k++] = '\t'; break;
        case 'r': out[k++] = '\r'; break;
        case 'b': case 'f': out[k++] = ' '; break;
        case 'u': {
            unsigned long cp = 0;
            int i, d;
            if (p->end - p->s < 4) { free(out); return NULL; }
            for (i = 0; i < 4; i++) { if ((d = hex(p->s[i])) < 0) { free(out); return NULL; } cp = cp * 16 + (unsigned long)d; }
            p->s += 4;
            if (cp >= 0xd800 && cp < 0xe000) cp = 0xfffd;       /* surrogates: not put together here */
            if (cp < 0x80) out[k++] = (char)cp;
            else if (cp < 0x800) { out[k++] = (char)(0xc0 | cp >> 6); out[k++] = (char)(0x80 | (cp & 63)); }
            else { out[k++] = (char)(0xe0 | cp >> 12); out[k++] = (char)(0x80 | ((cp >> 6) & 63)); out[k++] = (char)(0x80 | (cp & 63)); }
            break;
        }
        default: out[k++] = (char)c;
        }
    }
    if (p->s >= p->end) { free(out); return NULL; }
    p->s++;
    out[k] = 0;
    return out;
}

/* Skips one value that is not a string: a number, true, false, null. */
static void bare(struct in *p)
{
    while (p->s < p->end && *p->s != ',' && *p->s != '}' && *p->s != ']') p->s++;
}

void ln_pkglist_free(struct ln_pkglist *l)
{
    int i;
    for (i = 0; i < l->n; i++) { free(l->v[i].id); free(l->v[i].version); free(l->v[i].summary); }
    free(l->v);
    l->v = NULL;
    l->n = 0;
}

static int row(struct in *p, struct ln_pkg *g)
{
    memset(g, 0, sizeof *g);
    p->s++;                                     /* { */
    for (;;) {
        char *key, *val = NULL;
        ws(p);
        if (p->s < p->end && *p->s == '}') { p->s++; return 1; }
        if (!(key = string(p))) return 0;
        ws(p);
        if (p->s >= p->end || *p->s != ':') { free(key); return 0; }
        p->s++;
        ws(p);
        if (p->s < p->end && *p->s == '"') {
            if (!(val = string(p))) { free(key); return 0; }
            if (!strcmp(key, "id") && !g->id) { g->id = val; val = NULL; }
            else if (!strcmp(key, "version") && !g->version) { g->version = val; val = NULL; }
            else if (!strcmp(key, "summary") && !g->summary) { g->summary = val; val = NULL; }
            free(val);
        } else {
            const char *at = p->s;
            if (p->s < p->end && (*p->s == '{' || *p->s == '[')) { free(key); return 0; }      /* not in these rows */
            bare(p);
            if (!strcmp(key, "installed")) g->installed = p->s - at >= 4 && !memcmp(at, "true", 4);
        }
        free(key);
        ws(p);
        if (p->s < p->end && *p->s == ',') p->s++;
    }
}

int ln_pkglist_parse(struct ln_pkglist *l, const char *json, size_t n)
{
    struct in p;
    char *key;
    int cap = 0;
    p.s = json; p.end = json + n;
    l->v = NULL; l->n = 0;
    ws(&p);
    if (p.s >= p.end || *p.s != '{') return 0;
    p.s++;
    ws(&p);
    if (!(key = string(&p))) return 0;
    if (strcmp(key, "rows")) { free(key); return 0; }
    free(key);
    ws(&p);
    if (p.s >= p.end || *p.s != ':') return 0;
    p.s++;
    ws(&p);
    if (p.s >= p.end || *p.s != '[') return 0;
    p.s++;
    for (;;) {
        struct ln_pkg g;
        memset(&g, 0, sizeof g);
        ws(&p);
        if (p.s < p.end && *p.s == ']') return 1;
        if (p.s >= p.end || *p.s != '{' || !row(&p, &g)) {
            free(g.id); free(g.version); free(g.summary);
            ln_pkglist_free(l);
            return 0;
        }
        if (g.id && *g.id) {
            if (l->n == cap) {
                struct ln_pkg *grown = realloc(l->v, sizeof *grown * (size_t)(cap ? cap * 2 : 64));
                if (!grown) { free(g.id); free(g.version); free(g.summary); ln_pkglist_free(l); return 0; }
                l->v = grown;
                cap = cap ? cap * 2 : 64;
            }
            l->v[l->n++] = g;
        } else {
            free(g.id); free(g.version); free(g.summary);
        }
        ws(&p);
        if (p.s < p.end && *p.s == ',') p.s++;
    }
}
