/* SPDX-License-Identifier: MIT */
#include "queryline.h"

#include <stdlib.h>
#include <string.h>

static enum ln_act changed(int r) { return r > 0 ? LN_ACT_QUERY_CHANGED : LN_ACT_NONE; }

static int has_control(const char *s, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if ((unsigned char)s[i] < 0x20 || s[i] == 0x7f) return 1;
    return 0;
}

enum ln_act ln_queryline_key(ze_doc *d, enum ln_key key, unsigned mods, const char *utf8, size_t n,
                             struct ln_key_out *o)
{
    int extend = (mods & LN_MOD_SHIFT) != 0, word = (mods & LN_MOD_WORD) != 0;
    size_t len, a, b;
    o->out = NULL; o->outn = 0; o->delta = 0;
    ze_text(d, &len);

    switch (key) {
    case LN_K_TEXT:
        if (!n || has_control(utf8, n)) return LN_ACT_NONE;
        return changed(ze_insert(d, utf8, n));
    case LN_K_LEFT:
    case LN_K_RIGHT:
        if (word) ze_word_step(d, key == LN_K_RIGHT, extend);
        else ze_step(d, key == LN_K_RIGHT, extend);
        return LN_ACT_CARET;
    case LN_K_HOME: ze_set_caret(d, 0, extend); return LN_ACT_CARET;
    case LN_K_END:  ze_set_caret(d, len, extend); return LN_ACT_CARET;
    case LN_K_BACKSPACE:
    case LN_K_DELETE:
        ze_selection(d, &a, &b);
        if (word && a == b) ze_word_step(d, key == LN_K_DELETE, 1);     /* select the word, then remove it */
        return changed(key == LN_K_DELETE ? ze_delete(d) : ze_backspace(d));
    case LN_K_UP:        o->delta = -1; return LN_ACT_MOVE;
    case LN_K_DOWN:      o->delta = 1;  return LN_ACT_MOVE;
    case LN_K_PAGE_UP:   o->delta = -8; return LN_ACT_MOVE;
    case LN_K_PAGE_DOWN: o->delta = 8;  return LN_ACT_MOVE;
    case LN_K_RETURN: return LN_ACT_RUN;
    case LN_K_TAB:    return LN_ACT_ACTIONS;
    case LN_K_ESC:
        if (!len) return LN_ACT_CLOSE;
        ze_select_all(d);
        return changed(ze_backspace(d));
    case LN_K_SELECT_ALL: ze_select_all(d); return LN_ACT_CARET;
    case LN_K_COPY:
    case LN_K_CUT:
        o->out = ze_selected(d, &o->outn);
        if (!o->outn) { o->out = NULL; return LN_ACT_NONE; }
        if (key == LN_K_COPY) return LN_ACT_COPY;
        {
            /* the selected text must outlive its removal */
            static char *kept;
            char *c = realloc(kept, o->outn + 1);
            if (!c) return LN_ACT_NONE;
            kept = c;
            memcpy(kept, o->out, o->outn);
            kept[o->outn] = 0;
            o->out = kept;
        }
        return ze_backspace(d) > 0 ? LN_ACT_CUT : LN_ACT_NONE;
    case LN_K_PASTE: return LN_ACT_WANT_PASTE;
    case LN_K_UNDO:  return changed(ze_undo(d));
    case LN_K_REDO:  return changed(ze_redo(d));
    }
    return LN_ACT_NONE;
}

enum ln_act ln_queryline_paste(ze_doc *d, const char *utf8, size_t n)
{
    char *c;
    size_t i;
    int r;
    if (!n) return LN_ACT_NONE;
    if (!(c = malloc(n))) return LN_ACT_NONE;
    memcpy(c, utf8, n);
    /* one line: line ends and tabs become spaces; a final line end is dropped */
    while (n && (c[n - 1] == '\n' || c[n - 1] == '\r')) n--;
    for (i = 0; i < n; i++)
        if (c[i] == '\n' || c[i] == '\r' || c[i] == '\t') c[i] = ' ';
    r = (n && !has_control(c, n)) ? ze_insert(d, c, n) : 0;
    free(c);
    return changed(r);
}
