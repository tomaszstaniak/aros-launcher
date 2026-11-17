#include <assert.h>
#include <string.h>
#include "queryline.h"

static ze_doc *d;
static struct ln_key_out o;

static enum ln_act key(enum ln_key k, unsigned mods) { return ln_queryline_key(d, k, mods, NULL, 0, &o); }
static enum ln_act type(const char *s) { return ln_queryline_key(d, LN_K_TEXT, 0, s, strlen(s), &o); }
static int is(const char *s) { size_t n; const char *t = ze_text(d, &n); return n == strlen(s) && !memcmp(t, s, n); }

int main(void)
{
    static const char *zazolc[] = { "z", "a", "\xc5\xbc", "\xc3\xb3", "\xc5\x82", "\xc4\x87" };
    size_t a, b, i;
    d = ze_new();
    assert(d);

    /* typing, cluster by cluster */
    for (i = 0; i < 6; i++) assert(type(zazolc[i]) == LN_ACT_QUERY_CHANGED);
    assert(is("za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87"));

    /* Left, then Shift+Home selects everything before the caret */
    assert(key(LN_K_LEFT, 0) == LN_ACT_CARET);
    assert(key(LN_K_HOME, LN_MOD_SHIFT) == LN_ACT_CARET);
    ze_selection(d, &a, &b);
    assert(a == 0 && b == 8);
    /* typing replaces the selection */
    assert(type("x") == LN_ACT_QUERY_CHANGED && is("x\xc4\x87"));

    /* Backspace and Delete by one character */
    assert(key(LN_K_END, 0) == LN_ACT_CARET);
    assert(key(LN_K_BACKSPACE, 0) == LN_ACT_QUERY_CHANGED && is("x"));
    assert(key(LN_K_DELETE, 0) == LN_ACT_NONE);         /* nothing after the caret */
    assert(key(LN_K_HOME, 0) == LN_ACT_CARET);
    assert(key(LN_K_DELETE, 0) == LN_ACT_QUERY_CHANGED && is(""));
    assert(key(LN_K_BACKSPACE, 0) == LN_ACT_NONE);

    /* word Backspace */
    assert(type("foo bar") == LN_ACT_QUERY_CHANGED);
    assert(key(LN_K_BACKSPACE, LN_MOD_WORD) == LN_ACT_QUERY_CHANGED && is("foo "));
    /* word Delete from the start */
    assert(key(LN_K_HOME, 0) == LN_ACT_CARET);
    assert(key(LN_K_DELETE, LN_MOD_WORD) == LN_ACT_QUERY_CHANGED && !is("foo "));
    ze_set_text(d, "", 0);

    /* select all, copy, cut */
    type("hello world");
    assert(key(LN_K_COPY, 0) == LN_ACT_NONE);           /* nothing selected */
    assert(key(LN_K_SELECT_ALL, 0) == LN_ACT_CARET);
    assert(key(LN_K_COPY, 0) == LN_ACT_COPY && o.outn == 11 && !memcmp(o.out, "hello world", 11));
    assert(is("hello world"));
    key(LN_K_END, 0);
    key(LN_K_LEFT, LN_MOD_SHIFT | LN_MOD_WORD);
    assert(key(LN_K_CUT, 0) == LN_ACT_CUT && o.outn == 5 && !memcmp(o.out, "world", 5));
    assert(is("hello "));

    /* paste: one line, line ends to spaces, the last one dropped */
    assert(key(LN_K_PASTE, 0) == LN_ACT_WANT_PASTE);
    assert(ln_queryline_paste(d, "a\nb\r\n", 5) == LN_ACT_QUERY_CHANGED && is("hello a b"));
    /* not UTF-8, or control characters: refused whole */
    assert(ln_queryline_paste(d, "\xff", 1) == LN_ACT_NONE && is("hello a b"));
    assert(ln_queryline_paste(d, "ok\x01no", 5) == LN_ACT_NONE && is("hello a b"));
    assert(ln_queryline_paste(d, "", 0) == LN_ACT_NONE);
    assert(ln_queryline_paste(d, "\n", 1) == LN_ACT_NONE);
    assert(ln_queryline_key(d, LN_K_TEXT, 0, "\xff", 1, &o) == LN_ACT_NONE);
    assert(ln_queryline_key(d, LN_K_TEXT, 0, "\x1b", 1, &o) == LN_ACT_NONE);

    /* Esc: first clears, then closes; Undo brings the text back */
    assert(key(LN_K_ESC, 0) == LN_ACT_QUERY_CHANGED && is(""));
    assert(key(LN_K_ESC, 0) == LN_ACT_CLOSE);
    assert(key(LN_K_UNDO, 0) == LN_ACT_QUERY_CHANGED && is("hello a b"));
    assert(key(LN_K_REDO, 0) == LN_ACT_QUERY_CHANGED && is(""));

    /* keys for the list */
    assert(key(LN_K_DOWN, 0) == LN_ACT_MOVE && o.delta == 1);
    assert(key(LN_K_UP, 0) == LN_ACT_MOVE && o.delta == -1);
    assert(key(LN_K_PAGE_DOWN, 0) == LN_ACT_MOVE && o.delta == 8);
    assert(key(LN_K_RETURN, 0) == LN_ACT_RUN);
    assert(key(LN_K_TAB, 0) == LN_ACT_ACTIONS);

    ze_free(d);
    return 0;
}
