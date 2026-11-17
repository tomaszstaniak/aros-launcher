/* SPDX-License-Identifier: MIT */
/* The query line: what each key does to the typed text, and what it asks
 * of the panel. Pure; the text itself is a zunekit edit document. */
#ifndef LN_QUERYLINE_H
#define LN_QUERYLINE_H

#include <stddef.h>
#include "zk_edit_core.h"

enum ln_key {
    LN_K_TEXT,          /* typed characters, as UTF-8 */
    LN_K_LEFT, LN_K_RIGHT, LN_K_HOME, LN_K_END,
    LN_K_BACKSPACE, LN_K_DELETE,
    LN_K_UP, LN_K_DOWN, LN_K_PAGE_UP, LN_K_PAGE_DOWN,
    LN_K_RETURN, LN_K_ESC, LN_K_TAB,
    LN_K_SELECT_ALL, LN_K_CUT, LN_K_COPY, LN_K_PASTE, LN_K_UNDO, LN_K_REDO
};
enum { LN_MOD_SHIFT = 1, LN_MOD_WORD = 2 };     /* WORD: Ctrl or Alt held */

enum ln_act {
    LN_ACT_NONE,
    LN_ACT_CARET,           /* caret or selection moved: redraw */
    LN_ACT_QUERY_CHANGED,   /* the text changed: ask the sources again */
    LN_ACT_MOVE,            /* move the result selection by *delta rows */
    LN_ACT_RUN,
    LN_ACT_CLOSE,
    LN_ACT_ACTIONS,
    LN_ACT_COPY,            /* put *out (outn bytes) on the clipboard */
    LN_ACT_CUT,             /* the same, and the text changed */
    LN_ACT_WANT_PASTE       /* read the clipboard and call ln_queryline_paste */
};

struct ln_key_out { const char *out; size_t outn; int delta; };

/* One key. utf8/n is the text for LN_K_TEXT. Esc clears a non-empty query
 * and closes the panel on an empty one. Text that is not UTF-8, or that
 * holds control characters, is refused (LN_ACT_NONE). The copied text
 * stays valid until the next call. */
enum ln_act ln_queryline_key(ze_doc *d, enum ln_key key, unsigned mods, const char *utf8, size_t n,
                             struct ln_key_out *o);
/* Clipboard text into the query: line feeds and tabs become spaces, text
 * that is not UTF-8 is refused whole. */
enum ln_act ln_queryline_paste(ze_doc *d, const char *utf8, size_t n);

#endif
