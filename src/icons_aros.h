/* SPDX-License-Identifier: MIT */
/* Program icons as pictures for the panel. Icons are read from disk one
 * at a time, between key presses, so that typing never waits for them. */
#ifndef LN_ICONS_AROS_H
#define LN_ICONS_AROS_H

struct ln_icons;

struct ln_icons *ln_icons_new(void);
void ln_icons_free(struct ln_icons *c);
/* The icon of program id as size x size pixels of A,R,G,B bytes, or NULL
 * when it is not read yet (it is then wanted) or has no usable picture. */
const unsigned char *ln_icons_get(struct ln_icons *c, unsigned id, int size);
/* Reads one wanted icon; path_of gives the program's path (NULL: gone).
 * 1 when one was read (a picture may have changed), 0 when none is wanted. */
int ln_icons_work(struct ln_icons *c, const char *(*path_of)(void *ctx, unsigned id), void *ctx);
int ln_icons_wanted(const struct ln_icons *c);

#endif
