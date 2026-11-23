/* SPDX-License-Identifier: MIT */
/* Aliases and hotkeys of results. Pure.
 *
 * An alias is a short word that finds one program or command first; a
 * hotkey is a key combination (a commodities description such as
 * "ctrl alt w") that starts it from anywhere. Each has a target: a
 * program's path, or "command:<title>" for one of the launcher's own
 * commands. A key belongs to one target; a target has at most one alias
 * and one hotkey.
 *
 * Kept as text, one per line, fields separated by tabs:
 *   alias<TAB>wc<TAB>SYS:Packages/WordCraft/WordCraft<TAB>WordCraft
 *   hotkey<TAB>ctrl alt w<TAB>SYS:Packages/WordCraft/WordCraft<TAB>WordCraft */
#ifndef LN_SHORTCUTS_H
#define LN_SHORTCUTS_H

#include <stddef.h>

#define LN_SHORTCUTS_MAX 64
#define LN_ALIAS 'a'
#define LN_HOTKEY 'h'
#define LN_PIN 'p'          /* a pinned target; its key says what it is: program, file, drawer */

struct ln_shortcut {
    char kind;          /* LN_ALIAS, LN_HOTKEY or LN_PIN */
    char *key;          /* lower case, single spaces */
    char *target;
    char *title;        /* the target's name for people */
};

struct ln_shortcuts {
    struct ln_shortcut v[LN_SHORTCUTS_MAX];
    int n;
};

/* A key as it is kept: lower case, outer spaces gone, inner ones single.
 * 0 when it does not fit or is empty. */
int ln_shortcut_key(const char *typed, char *out, size_t cap);
/* Gives target this key of this kind. An empty key removes the target's
 * key of that kind. A key another target had moves here. 1 done, 0 when
 * the list is full or memory ran out (nothing changed then). */
int ln_shortcuts_set(struct ln_shortcuts *s, char kind, const char *key, const char *target, const char *title);
/* The entry of this key, compared as ln_shortcut_key makes it; NULL none. */
const struct ln_shortcut *ln_shortcuts_by_key(const struct ln_shortcuts *s, char kind, const char *key);
/* The target's key of this kind; NULL none. Targets compare without case. */
const char *ln_shortcuts_key_of(const struct ln_shortcuts *s, char kind, const char *target);
void ln_shortcuts_remove(struct ln_shortcuts *s, int i);
/* Reads text into s (zeroed or freed first); lines that are not
 * understood are left out. 0 when out of memory. */
int ln_shortcuts_parse(struct ln_shortcuts *s, const char *text, size_t n);
/* As text, malloc'd; NULL when out of memory. */
char *ln_shortcuts_text(const struct ln_shortcuts *s, size_t *n);
void ln_shortcuts_free(struct ln_shortcuts *s);

#endif
