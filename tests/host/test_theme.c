#include <assert.h>
#include <string.h>
#include "../../src/style.h"
#include "../../src/theme.h"

int main(void)
{
    struct ln_theme t, f, b;
    struct ln_style s = *ln_style_get(1, 0), l = *ln_style_get(0, 0);
    const char *nord = "# Nord\nbackground=#2e3440\nforeground = #d8dee9 \r\ncursor=#eceff4\nselection_bg=#eceff4\nselection_fg=#4c566a\nansi4=#81a1c1\nansi12=#81a1c1\nfont=Vera\nbad=#12\n";
    const char *sol = "background=#fdf6e3\nforeground=#657b83\nselection_bg=#eee8d5\nansi4=#268bd2\n";
    assert(ln_theme_parse(nord, strlen(nord), &t));
    assert(t.bg == 0x2e3440 && t.fg == 0xd8dee9 && t.has_cursor && t.sel_bg == 0xeceff4 && t.has_accent && t.accent == 0x81a1c1);
    assert(ln_theme_dark(&t));
    assert(!ln_theme_parse("foreground=#ffffff\n", 19, &f) && !ln_theme_parse("", 0, &f));
    assert(ln_theme_contrast100(0x000000, 0xffffff) == 2100 && ln_theme_contrast100(0x777777, 0x777777) == 100);

    /* a dark theme asked for dark keeps its own background and text */
    ln_theme_apply(&t, 1, &s);
    assert((s.tint & 0xffffff) == 0x2e3440 && (s.text & 0xffffff) == 0xd8dee9 && (s.match & 0xffffff) == 0x81a1c1);
    assert(s.width == ln_style_get(1, 0)->width);
    assert(ln_theme_contrast100(s.sel_text, s.sel_fill) >= 450 && ln_theme_contrast100(s.text, s.tint) >= 450);
    /* asked for light, the other variant is made: light background, dark text that reads */
    ln_theme_apply(&t, 0, &l);
    assert(ln_theme_contrast100(l.tint, 0xffffff) < 140 && ln_theme_contrast100(l.text, l.tint) >= 700);
    assert(ln_theme_contrast100(l.match, l.tint) >= 300 && ln_theme_contrast100(l.sel_text, l.sel_fill) >= 450);

    /* a light theme: its dark variant is made, and making it twice comes back to a light one */
    assert(ln_theme_parse(sol, strlen(sol), &t) && !ln_theme_dark(&t));
    ln_theme_flip(&t, &f);
    assert(ln_theme_dark(&f) && ln_theme_contrast100(f.fg, f.bg) >= 700);
    assert(ln_theme_contrast100(f.bg, 0x000000) >= 115 && ln_theme_contrast100(f.bg, 0x000000) < 260);
    ln_theme_flip(&f, &b);
    assert(!ln_theme_dark(&b));
    ln_theme_apply(&t, 1, &s);
    assert(ln_theme_contrast100(s.text, s.tint) >= 700 && ln_theme_contrast100(s.match, s.tint) >= 300);
    ln_theme_apply(&t, 0, &l);
    assert((l.tint & 0xffffff) == 0xfdf6e3 && (l.sel_fill & 0xffffff) == 0xeee8d5);
    return 0;
}
