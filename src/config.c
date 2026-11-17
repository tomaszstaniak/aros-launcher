/* SPDX-License-Identifier: MIT */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int lower(int ch) { return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch; }

static int same(const char *a, const char *b)
{
    while (*a && *b && lower((unsigned char)*a) == lower((unsigned char)*b)) { a++; b++; }
    return !*a && !*b;
}

/* A copy of s[0..n) without spaces around it and without a trailing '/'
 * (a volume's ':' stays). */
static char *clean(const char *s, size_t n)
{
    char *c;
    while (n && (*s == ' ' || *s == '\t')) { s++; n--; }
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) n--;
    while (n > 1 && s[n - 1] == '/') n--;
    if (!(c = malloc(n + 1))) return NULL;
    memcpy(c, s, n);
    c[n] = 0;
    return c;
}

int ln_config_add_folder(struct ln_config *c, const char *path)
{
    char *p = clean(path, strlen(path));
    int i;
    if (!p) return 0;
    if (!*p || c->folders == LN_FOLDERS_MAX) { free(p); return 0; }
    for (i = 0; i < c->folders; i++)
        if (same(c->folder[i], p)) { free(p); return 0; }
    c->folder[c->folders++] = p;
    return 1;
}

void ln_config_remove_folder(struct ln_config *c, int i)
{
    if (i < 0 || i >= c->folders) return;
    free(c->folder[i]);
    memmove(&c->folder[i], &c->folder[i + 1], sizeof c->folder[0] * (size_t)(c->folders - i - 1));
    c->folders--;
}

void ln_config_free(struct ln_config *c)
{
    int i;
    for (i = 0; i < c->folders; i++) free(c->folder[i]);
    free(c->other);
    memset(c, 0, sizeof *c);
}

int ln_config_defaults(struct ln_config *c)
{
    static const char *const first[] = { "SYS:Tools", "SYS:Utilities", "SYS:System", "SYS:Prefs", "SYS:Packages" };
    size_t i;
    memset(c, 0, sizeof *c);
    c->dark = 1;
    c->glass = 1;
    for (i = 0; i < sizeof first / sizeof first[0]; i++)
        if (!ln_config_add_folder(c, first[i])) { ln_config_free(c); return 0; }
    return 1;
}

static int keep(struct ln_config *c, const char *line, size_t n)
{
    size_t have = c->other ? strlen(c->other) : 0;
    char *g = realloc(c->other, have + n + 2);
    if (!g) return 0;
    memcpy(g + have, line, n);
    g[have + n] = '\n';
    g[have + n + 1] = 0;
    c->other = g;
    return 1;
}

int ln_config_parse(struct ln_config *c, const char *text, size_t n)
{
    size_t at = 0;
    c->dark = 1;
    c->glass = 1;
    while (at < n) {
        size_t end = at, len, klen;
        const char *line = text + at, *eq;
        while (end < n && text[end] != '\n') end++;
        len = end - at;
        at = end + 1;
        while (len && (line[len - 1] == '\r' || line[len - 1] == ' ')) len--;
        if (!len) continue;
        eq = memchr(line, '=', len);
        klen = eq ? (size_t)(eq - line) : 0;
        if (eq && klen == 6 && !memcmp(line, "folder", 6)) {
            char *p = clean(eq + 1, len - 7);
            if (!p) return 0;
            if (*p) ln_config_add_folder(c, p);
            free(p);
        } else if (eq && klen == 5 && !memcmp(line, "theme", 5) && len - 6 == 5 && !memcmp(eq + 1, "light", 5)) {
            c->dark = 0;
        } else if (eq && klen == 5 && !memcmp(line, "theme", 5) && len - 6 == 4 && !memcmp(eq + 1, "dark", 4)) {
            c->dark = 1;
        } else if (eq && klen == 7 && !memcmp(line, "compact", 7) && len == 9 && (eq[1] == '0' || eq[1] == '1')) {
            c->compact = eq[1] == '1';
        } else if (eq && klen == 5 && !memcmp(line, "glass", 5) && len == 7 && (eq[1] == '0' || eq[1] == '1')) {
            c->glass = eq[1] == '1';
        } else if (!keep(c, line, len)) {
            return 0;
        }
    }
    return 1;
}

char *ln_config_text(const struct ln_config *c, size_t *n)
{
    size_t cap = 96 + (c->other ? strlen(c->other) : 0), len = 0;
    char *out;
    int i;
    for (i = 0; i < c->folders; i++) cap += strlen(c->folder[i]) + 9;
    if (!(out = malloc(cap))) return NULL;
    for (i = 0; i < c->folders; i++) len += (size_t)snprintf(out + len, cap - len, "folder=%s\n", c->folder[i]);
    len += (size_t)snprintf(out + len, cap - len, "theme=%s\ncompact=%d\nglass=%d\n",
                            c->dark ? "dark" : "light", c->compact, c->glass);
    if (c->other) len += (size_t)snprintf(out + len, cap - len, "%s", c->other);
    if (n) *n = len;
    return out;
}
