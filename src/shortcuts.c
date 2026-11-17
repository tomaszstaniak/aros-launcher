/* SPDX-License-Identifier: MIT */
#include "shortcuts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int lower(int ch) { return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch; }

static int same(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (lower((unsigned char)*a) != lower((unsigned char)*b)) return 0;
    return !*a && !*b;
}

static char *copy(const char *s, size_t n)
{
    char *out = malloc(n + 1);
    if (out) { memcpy(out, s, n); out[n] = 0; }
    return out;
}

int ln_shortcut_key(const char *typed, char *out, size_t cap)
{
    size_t k = 0;
    int space = 0;
    for (; *typed; typed++) {
        unsigned char c = (unsigned char)*typed;
        if (c == ' ' || c == '\t') { space = k > 0; continue; }
        if (c < 32) return 0;
        if (space) { if (k + 1 >= cap) return 0; out[k++] = ' '; space = 0; }
        if (k + 1 >= cap) return 0;
        out[k++] = (char)lower(c);
    }
    if (cap) out[k] = 0;
    return k > 0;
}

static void drop(struct ln_shortcut *e)
{
    free(e->key); free(e->target); free(e->title);
    memset(e, 0, sizeof *e);
}

void ln_shortcuts_remove(struct ln_shortcuts *s, int i)
{
    if (i < 0 || i >= s->n) return;
    drop(&s->v[i]);
    memmove(&s->v[i], &s->v[i + 1], (size_t)(s->n - i - 1) * sizeof s->v[0]);
    s->n--;
    memset(&s->v[s->n], 0, sizeof s->v[0]);
}

const struct ln_shortcut *ln_shortcuts_by_key(const struct ln_shortcuts *s, char kind, const char *key)
{
    char k[80];
    int i;
    if (!ln_shortcut_key(key, k, sizeof k)) return NULL;
    for (i = 0; i < s->n; i++)
        if (s->v[i].kind == kind && !strcmp(s->v[i].key, k)) return &s->v[i];
    return NULL;
}

const char *ln_shortcuts_key_of(const struct ln_shortcuts *s, char kind, const char *target)
{
    int i;
    for (i = 0; i < s->n; i++)
        if (s->v[i].kind == kind && same(s->v[i].target, target)) return s->v[i].key;
    return NULL;
}

int ln_shortcuts_set(struct ln_shortcuts *s, char kind, const char *key, const char *target, const char *title)
{
    char k[80];
    char *nk, *nt, *nn;
    int i, has = ln_shortcut_key(key, k, sizeof k), mine = -1, theirs = -1;
    if (kind != LN_ALIAS && kind != LN_HOTKEY) return 0;
    if (!target || !*target) return 0;
    for (i = 0; i < s->n; i++) {
        if (s->v[i].kind != kind) continue;
        if (same(s->v[i].target, target)) mine = i;
        else if (has && !strcmp(s->v[i].key, k)) theirs = i;
    }
    if (!has) {
        if (*key) {
            /* something was typed that is not a key: nothing changes */
            const char *p;
            for (p = key; *p == ' ' || *p == '\t'; p++) ;
            if (*p) return 0;
        }
        if (mine >= 0) ln_shortcuts_remove(s, mine);
        return 1;
    }
    if (mine < 0 && theirs < 0 && s->n >= LN_SHORTCUTS_MAX) return 0;
    nk = copy(k, strlen(k));
    nt = copy(target, strlen(target));
    nn = copy(title ? title : "", title ? strlen(title) : 0);
    if (!nk || !nt || !nn) { free(nk); free(nt); free(nn); return 0; }
    if (theirs >= 0) {
        ln_shortcuts_remove(s, theirs);
        if (mine > theirs) mine--;
    }
    if (mine < 0) mine = s->n++;
    else drop(&s->v[mine]);
    s->v[mine].kind = kind;
    s->v[mine].key = nk;
    s->v[mine].target = nt;
    s->v[mine].title = nn;
    return 1;
}

void ln_shortcuts_free(struct ln_shortcuts *s)
{
    int i;
    for (i = 0; i < s->n; i++) drop(&s->v[i]);
    s->n = 0;
}

int ln_shortcuts_parse(struct ln_shortcuts *s, const char *text, size_t n)
{
    const char *p = text, *end = text + n;
    while (p < end) {
        const char *line = p, *stop = memchr(p, '\n', (size_t)(end - p)), *f[4];
        size_t len[4];
        int fields = 0;
        if (!stop) stop = end;
        p = stop < end ? stop + 1 : end;
        while (fields < 4 && line <= stop) {
            const char *tab = memchr(line, '\t', (size_t)(stop - line));
            if (!tab) tab = stop;
            f[fields] = line; len[fields] = (size_t)(tab - line); fields++;
            line = tab + 1;
        }
        if (fields >= 3 && len[1] && len[1] < 80 && len[2]) {
            char kind = len[0] == 5 && !memcmp(f[0], "alias", 5) ? LN_ALIAS : len[0] == 6 && !memcmp(f[0], "hotkey", 6) ? LN_HOTKEY : 0;
            char *key = copy(f[1], len[1]), *target = copy(f[2], len[2]), *title = copy(fields > 3 ? f[3] : "", fields > 3 ? len[3] : 0);
            int ok = key && target && title;
            if (ok && kind) ok = ln_shortcuts_set(s, kind, key, target, title) || s->n >= LN_SHORTCUTS_MAX;
            free(key); free(target); free(title);
            if (!ok) return 0;
        }
    }
    return 1;
}

char *ln_shortcuts_text(const struct ln_shortcuts *s, size_t *n)
{
    size_t cap = 1, k = 0;
    char *out;
    int i;
    for (i = 0; i < s->n; i++) cap += strlen(s->v[i].key) + strlen(s->v[i].target) + strlen(s->v[i].title) + 12;
    if (!(out = malloc(cap))) return NULL;
    for (i = 0; i < s->n; i++)
        k += (size_t)snprintf(out + k, cap - k, "%s\t%s\t%s\t%s\n", s->v[i].kind == LN_ALIAS ? "alias" : "hotkey",
                              s->v[i].key, s->v[i].target, s->v[i].title);
    out[k] = 0;
    if (n) *n = k;
    return out;
}
