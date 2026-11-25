/* SPDX-License-Identifier: MIT */
/* The texts that were on the clipboard, newest first. Pure. Kept in
 * memory only: nothing of it is written to a disk. */
#ifndef LN_CLIPHIST_H
#define LN_CLIPHIST_H

#include <stddef.h>

#define LN_CLIP_MAX 50
#define LN_CLIP_TEXT_MAX (64u * 1024u)  /* a longer text is not kept */
#define LN_CLIP_PICTURE_MAX (32u * 1024u * 1024u)   /* nor a larger picture */
#define LN_CLIP_BYTES_MAX (64u * 1024u * 1024u)     /* all entries together; the oldest go first */

enum { LN_CLIP_TEXT = 0, LN_CLIP_PICTURE };

struct ln_clip {
    char *text;         /* the text, or a picture's bytes exactly as they were on the clipboard */
    size_t n;
    int kind;           /* LN_CLIP_TEXT or LN_CLIP_PICTURE */
    int w, h, depth;    /* of a picture */
    unsigned char *thumb;   /* the caller's small picture of it, freed with the entry */
    int thumb_size;
    unsigned id;        /* stable while the entry lives, never 0 */
    unsigned when;      /* the caller's clock */
};

struct ln_cliphist {
    struct ln_clip v[LN_CLIP_MAX];
    int n;
    unsigned next_id;
    size_t bytes;       /* of all texts and pictures held */
};

/* Puts a text first. One that is there already moves to the front; empty
 * texts, texts of spaces only and texts over the limit are not kept; the
 * oldest goes when the list is full. 1 when the list changed. */
int ln_cliphist_add(struct ln_cliphist *h, const char *text, size_t n, unsigned when);
/* The entry with this id; NULL none. */
/* A picture's bytes join the history like a text; 0 when it is too large,
 * already the newest, or there is no memory. Older entries leave until
 * everything fits LN_CLIP_BYTES_MAX. */
int ln_cliphist_add_picture(struct ln_cliphist *h, const void *data, size_t n, int w, int hgt, int depth, unsigned when);

const struct ln_clip *ln_cliphist_find(const struct ln_cliphist *h, unsigned id);
void ln_cliphist_remove(struct ln_cliphist *h, unsigned id);
void ln_cliphist_clear(struct ln_cliphist *h);
/* One line for a list: the first line that has text, spaces single,
 * control characters as spaces, cut at a character boundary of UTF-8. */
void ln_cliphist_title(const struct ln_clip *c, char *out, size_t cap);

/* Looks for q (qn bytes, letters A-Z without case) anywhere in the entry's
 * whole text. 1 when it is there; line then holds the line it was found in,
 * shortened to fit, as ln_cliphist_title writes one. */
int ln_cliphist_search(const struct ln_clip *c, const char *q, size_t qn, char *line, size_t cap);

#endif
