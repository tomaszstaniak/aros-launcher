#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

int main(void)
{
    struct ln_config c, d;
    char *t;
    size_t n;
    const char *file =
        "folder=SYS:Tools/\r\n"
        "folder = ignored because of the space before =\n"
        "folder=Work:Apps\n"
        "folder=sys:tools\n"            /* the same folder again */
        "theme=light\n"
        "compact=1\n"
        "glass=0\n"
        "future=thing\n"
        "\n"
        "theme=purple\n";               /* not understood: kept */

    assert(ln_config_defaults(&c));
    assert(c.folders == 5 && c.dark && !c.compact && c.glass);
    assert(!strcmp(c.folder[4], "SYS:Packages"));
    assert(!strcmp(c.folder[0], "SYS:Tools"));
    t = ln_config_text(&c, &n);
    assert(t && n == strlen(t));
    memset(&d, 0, sizeof d);
    assert(ln_config_parse(&d, t, n));
    assert(d.folders == 5 && d.dark && d.glass && !d.compact && !d.other);
    free(t); ln_config_free(&d); ln_config_free(&c);

    memset(&c, 0, sizeof c);
    assert(ln_config_parse(&c, file, strlen(file)));
    assert(c.folders == 2);
    assert(!strcmp(c.folder[0], "SYS:Tools") && !strcmp(c.folder[1], "Work:Apps"));
    assert(!c.dark && c.compact && !c.glass);
    assert(c.other && strstr(c.other, "future=thing\n") && strstr(c.other, "theme=purple\n"));
    assert(strstr(c.other, "folder = ignored"));

    /* unknown lines survive a round trip */
    t = ln_config_text(&c, &n);
    assert(strstr(t, "future=thing\n") && strstr(t, "folder=Work:Apps\n") && strstr(t, "theme=light\n"));
    free(t);

    /* folders: no duplicates, a volume keeps its colon, removal */
    assert(!ln_config_add_folder(&c, "work:apps/"));
    assert(ln_config_add_folder(&c, "RAM:") && !strcmp(c.folder[2], "RAM:"));
    assert(!ln_config_add_folder(&c, "  "));
    ln_config_remove_folder(&c, 0);
    assert(c.folders == 2 && !strcmp(c.folder[0], "Work:Apps"));
    ln_config_remove_folder(&c, 9);
    assert(c.folders == 2);
    while (ln_config_add_folder(&c, "X:")) break;
    {
        char name[16];
        int i;
        for (i = 0; i < 40; i++) { name[0] = 'V'; name[1] = (char)('a' + i % 26); name[2] = (char)('a' + i / 26); name[3] = ':'; name[4] = 0; ln_config_add_folder(&c, name); }
        assert(c.folders == LN_FOLDERS_MAX);
    }
    ln_config_free(&c);

    /* an empty file: no folders, default looks */
    memset(&c, 0, sizeof c);
    assert(ln_config_parse(&c, "", 0) && c.folders == 0 && c.dark && c.glass);
    ln_config_free(&c);
    return 0;
}
