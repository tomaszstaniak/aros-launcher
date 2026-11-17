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

    /* start with the system: undecided until a file says so, then kept */
    memset(&c, 0, sizeof c);
    assert(ln_config_parse(&c, "theme=dark\n", 11) && c.autostart == -1);
    t = ln_config_text(&c, &n);
    assert(!strstr(t, "autostart"));
    free(t); ln_config_free(&c);
    memset(&c, 0, sizeof c);
    assert(ln_config_parse(&c, "autostart=0\n", 12) && c.autostart == 0);
    c.autostart = 1;
    t = ln_config_text(&c, &n);
    assert(strstr(t, "autostart=1\n"));
    free(t); ln_config_free(&c);

    /* folders indexed for files: their own list, kept through a round trip */
    memset(&c, 0, sizeof c);
    {
        const char *both = "files=Work:Documents/\nfolder=SYS:Tools\nfiles=work:documents\nfiles=Data:\n";
        assert(ln_config_parse(&c, both, strlen(both)));
    }
    assert(c.files_n == 2 && c.folders == 1 && !strcmp(c.files[0], "Work:Documents") && !strcmp(c.files[1], "Data:"));
    t = ln_config_text(&c, &n);
    assert(strstr(t, "files=Work:Documents\n") && strstr(t, "files=Data:\n") && strstr(t, "folder=SYS:Tools\n"));
    free(t);
    ln_config_remove_files(&c, 0);
    assert(c.files_n == 1 && !strcmp(c.files[0], "Data:"));
    ln_config_free(&c);

    /* what is kept out of the file index: folders and name patterns */
    memset(&c, 0, sizeof c);
    {
        const char *ex = "exclude=Work:Documents/Old/\nexclude=#?.bak\nexclude=#?.BAK\nexclude=Work:tmp\n";
        assert(ln_config_parse(&c, ex, strlen(ex)));
    }
    assert(c.exclude_n == 3 && !strcmp(c.exclude[0], "Work:Documents/Old") && !strcmp(c.exclude[1], "#?.bak"));
    t = ln_config_text(&c, &n);
    assert(strstr(t, "exclude=Work:Documents/Old\n") && strstr(t, "exclude=#?.bak\n") && strstr(t, "exclude=Work:tmp\n"));
    free(t);
    ln_config_remove_exclude(&c, 1);
    assert(c.exclude_n == 2 && !strcmp(c.exclude[1], "Work:tmp"));
    ln_config_free(&c);

    /* an empty file: no folders, default looks */
    memset(&c, 0, sizeof c);
    assert(ln_config_parse(&c, "", 0) && c.folders == 0 && c.dark && c.glass);
    ln_config_free(&c);
    return 0;
}
