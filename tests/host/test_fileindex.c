#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fileindex.h"

static struct ln_fileindex *x;
static struct ln_file out[16];

static int q(const char *s, int max) { return ln_fileindex_query(x, s, strlen(s), out, max); }

int main(void)
{
    char *text;
    size_t n;
    int k, i;
    struct ln_fileindex *y;

    x = ln_fileindex_new();
    assert(x && ln_fileindex_count(x) == 0 && q("anything", 8) == 0);

    assert(ln_fileindex_add(x, "Work:Documents", "Report 2026.txt", 0));
    assert(ln_fileindex_add(x, "Work:Documents", "annual-report.pdf", 0));
    assert(ln_fileindex_add(x, "Work:Documents", "Reports", 1));
    assert(ln_fileindex_add(x, "Work:Pictures/Holiday", "beach.png", 0));
    assert(ln_fileindex_add(x, "Work:Pictures/Holiday", "unreported.jpg", 0));
    assert(ln_fileindex_add(x, "Work:Music", "Report.mod", 0));
    assert(!ln_fileindex_add(x, "Work:Music", "", 0));
    assert(ln_fileindex_count(x) == 6);

    /* every name with the word, case not counting; start of name first,
       then start of a word, then inside; shorter first */
    k = q("report", 8);
    assert(k == 5);
    assert(!strcmp(out[0].name, "Reports") && out[0].is_dir);            /* starts, 7 bytes */
    assert(!strcmp(out[1].name, "Report.mod"));
    assert(!strcmp(out[2].name, "Report 2026.txt"));
    assert(!strcmp(out[3].name, "annual-report.pdf"));                   /* word start */
    assert(!strcmp(out[4].name, "unreported.jpg"));                      /* inside */
    assert(!strcmp(out[4].dir, "Work:Pictures/Holiday"));

    /* all words must occur, in any order */
    assert(q("report 2026", 8) == 1 && !strcmp(out[0].name, "Report 2026.txt"));
    assert(q("2026 report", 8) == 1);
    assert(q("report png", 8) == 0);
    assert(q("REPORT PDF", 8) == 1 && !strcmp(out[0].name, "annual-report.pdf"));

    /* max keeps the best */
    assert(q("report", 2) == 2 && !strcmp(out[0].name, "Reports") && !strcmp(out[1].name, "Report.mod"));

    /* too little to search for */
    assert(q("", 8) == 0 && q(" ", 8) == 0 && q("r", 8) == 0);
    assert(q("r e", 8) == 0);
    assert(q("re p", 8) > 0);          /* one word of two letters is enough */
    assert(q("zzz", 8) == 0);

    /* save and load give the same index */
    text = ln_fileindex_save(x, &n);
    assert(text && n == strlen(text));
    y = ln_fileindex_new();
    assert(ln_fileindex_load(y, text, n) == 6 && ln_fileindex_count(y) == 6);
    free(text);
    ln_fileindex_free(x);
    x = y;
    assert(q("beach", 8) == 1 && !strcmp(out[0].dir, "Work:Pictures/Holiday") && !out[0].is_dir);
    assert(q("reports", 8) == 1 && out[0].is_dir);

    /* lines that do not parse are skipped */
    {
        const char *bad = "X\ta\tb\nF\tonly-dir\nF\t\tname\nD\tRAM:\tT\n\nF\tWork:\tfile";
        struct ln_fileindex *z = ln_fileindex_new();
        assert(ln_fileindex_load(z, bad, strlen(bad)) == 2);
        ln_fileindex_free(z);
    }

    /* many entries: blocks grow, names stay where they were */
    {
        struct ln_fileindex *z = ln_fileindex_new();
        char name[64], dir[64];
        for (i = 0; i < 50000; i++) {
            snprintf(dir, sizeof dir, "Vol:dir%03d", i / 100);
            snprintf(name, sizeof name, "file-%05d.dat", i);
            assert(ln_fileindex_add(z, dir, name, 0));
        }
        assert(ln_fileindex_count(z) == 50000);
        y = x; x = z;
        assert(q("file-04321", 8) == 1 && !strcmp(out[0].name, "file-04321.dat") && !strcmp(out[0].dir, "Vol:dir043"));
        assert(q("file", 16) == 16);
        x = y;
        ln_fileindex_free(z);
    }
    /* a very long name does not break anything */
    {
        char longname[400];
        memset(longname, 'a', sizeof longname - 1); longname[sizeof longname - 1] = 0;
        assert(ln_fileindex_add(x, "Work:", longname, 0));
        assert(q("aaaa", 8) == 1);
    }
    ln_fileindex_free(x);
    {
        /* a folder that cannot be walked keeps its entries; names that
           differ only in a character of the same letter column are two files */
        struct ln_fileindex *old = ln_fileindex_new(), *fresh = ln_fileindex_new();
        struct ln_file f[4];
        assert(ln_fileindex_add(old, "Work:Docs", "a.txt", 0));
        assert(ln_fileindex_add(old, "Work:Docs/Sub", "b.txt", 0));
        assert(ln_fileindex_add(old, "Work:Docs2", "c.txt", 0));
        assert(ln_fileindex_add(old, "Other:", "d.txt", 0));
        assert(ln_fileindex_keep(fresh, old, "work:docs/") == 2);
        assert(ln_fileindex_count(fresh) == 2);
        assert(ln_fileindex_query(fresh, "c.txt", 5, f, 4) == 0);
        assert(ln_fileindex_keep(fresh, old, "Other:") == 1);
        assert(ln_fileindex_keep(fresh, NULL, "Other:") == 0);
        assert(!ln_fileindex_full(fresh));
        assert(ln_path_hash("Work:raport[1].txt") != ln_path_hash("Work:raport{1}.txt"));
        assert(ln_path_hash("Work:a") == ln_path_hash("Work:a"));
        ln_fileindex_free(old);
        ln_fileindex_free(fresh);
    }
    return 0;
}
