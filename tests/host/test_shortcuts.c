#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/shortcuts.h"

int main(void)
{
    struct ln_shortcuts s, t;
    char k[16], *text;
    size_t n;
    int i;
    memset(&s, 0, sizeof s);
    memset(&t, 0, sizeof t);
    assert(ln_shortcut_key("  Ctrl   ALT w ", k, sizeof k) && !strcmp(k, "ctrl alt w"));
    assert(!ln_shortcut_key("   ", k, sizeof k) && !ln_shortcut_key("", k, sizeof k));
    assert(!ln_shortcut_key("a very long key indeed", k, sizeof k));

    assert(ln_shortcuts_set(&s, LN_ALIAS, "WC", "SYS:Apps/WordCraft", "WordCraft") && s.n == 1);
    assert(ln_shortcuts_set(&s, LN_HOTKEY, "ctrl alt w", "sys:apps/wordcraft", "WordCraft") && s.n == 2);
    assert(!strcmp(ln_shortcuts_key_of(&s, LN_ALIAS, "SYS:APPS/WORDCRAFT"), "wc"));
    assert(!strcmp(ln_shortcuts_by_key(&s, LN_ALIAS, " wc ")->target, "SYS:Apps/WordCraft"));
    assert(!ln_shortcuts_by_key(&s, LN_HOTKEY, "wc") && !ln_shortcuts_by_key(&s, LN_ALIAS, "w"));
    /* a new alias for the same target replaces the old one */
    assert(ln_shortcuts_set(&s, LN_ALIAS, "word", "SYS:Apps/WordCraft", "WordCraft") && s.n == 2);
    assert(!ln_shortcuts_by_key(&s, LN_ALIAS, "wc") && ln_shortcuts_by_key(&s, LN_ALIAS, "word"));
    /* a key another target takes moves */
    assert(ln_shortcuts_set(&s, LN_ALIAS, "word", "command:Quit Jumpstart", "Quit Jumpstart") && s.n == 2);
    assert(!ln_shortcuts_key_of(&s, LN_ALIAS, "SYS:Apps/WordCraft"));
    assert(!strcmp(ln_shortcuts_by_key(&s, LN_ALIAS, "WORD")->title, "Quit Jumpstart"));
    /* empty removes; removing what is not there is fine; rubbish changes nothing */
    assert(ln_shortcuts_set(&s, LN_ALIAS, "", "command:Quit Jumpstart", "x") && s.n == 1);
    assert(ln_shortcuts_set(&s, LN_ALIAS, "  ", "command:Quit Jumpstart", "x") && s.n == 1);
    assert(!ln_shortcuts_set(&s, 'x', "a", "b", "c") && !ln_shortcuts_set(&s, LN_ALIAS, "a", "", "c"));
    assert(!ln_shortcuts_set(&s, LN_HOTKEY, "this key is far too long to be kept by anyone, it goes on and on and on and on and on", "b", "c") && s.n == 1);

    assert(ln_shortcuts_set(&s, LN_ALIAS, "q", "command:Quit Jumpstart", "Quit Jumpstart"));
    text = ln_shortcuts_text(&s, &n);
    assert(text && ln_shortcuts_parse(&t, text, n) && t.n == 2);
    assert(!strcmp(ln_shortcuts_key_of(&t, LN_HOTKEY, "SYS:Apps/WordCraft"), "ctrl alt w"));
    assert(!strcmp(ln_shortcuts_by_key(&t, LN_ALIAS, "q")->title, "Quit Jumpstart"));
    free(text);
    ln_shortcuts_free(&t);
    memset(&t, 0, sizeof t);
    {
        const char *odd = "rubbish\n\nalias\t\tx\nnothing\talias\ta\tb\nalias\tz\tT:z\n";
        assert(ln_shortcuts_parse(&t, odd, strlen(odd)) && t.n == 1 && !strcmp(t.v[0].key, "z"));
    }
    ln_shortcuts_free(&t);
    memset(&t, 0, sizeof t);
    for (i = 0; i < LN_SHORTCUTS_MAX; i++) {
        char key[8], target[16];
        snprintf(key, sizeof key, "k%d", i); snprintf(target, sizeof target, "T:%d", i);
        assert(ln_shortcuts_set(&t, LN_ALIAS, key, target, ""));
    }
    assert(!ln_shortcuts_set(&t, LN_ALIAS, "more", "T:more", "") && t.n == LN_SHORTCUTS_MAX);
    assert(ln_shortcuts_set(&t, LN_ALIAS, "k0", "T:5", "") && t.n == LN_SHORTCUTS_MAX - 1);
    ln_shortcuts_free(&t);
    ln_shortcuts_free(&s);
    return 0;
}
