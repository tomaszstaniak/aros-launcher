/* SPDX-License-Identifier: MIT */
/* Keys and the clipboard on AROS, at the edge between the system's 8-bit
 * character set and the UTF-8 inside. */
#ifndef LN_KEYS_AROS_H
#define LN_KEYS_AROS_H

#include <stddef.h>
#include <intuition/intuition.h>
#include "queryline.h"

/* What an IDCMP_RAWKEY message means: 1 with *key, *mods and, for
 * LN_K_TEXT, the typed text in utf8 (*n bytes); 0 for a key release or a
 * key that means nothing here. */
int ln_keys_translate(const struct IntuiMessage *im, enum ln_key *key, unsigned *mods,
                      char *utf8, size_t cap, size_t *n);

/* Text to clipboard unit 0 as FTXT with a UTF8 chunk and a CHRS copy in
 * the system's set. 1 written. */
int ln_clip_write(const char *utf8, size_t n);
/* The clipboard's text as UTF-8, malloc'd, *n bytes; NULL when there is
 * none. */
char *ln_clip_read(size_t *n);

/* The clipboard's content when it is an IFF FORM of the given type (four
 * letters, "ILBM" for a picture): all of its bytes, malloc'd, *n of them.
 * NULL when the clipboard holds something else or more than max bytes. */
unsigned char *ln_clip_read_form(const char *type, size_t max, size_t *n);
/* Puts bytes read that way back on the clipboard as they are. 1 written. */
int ln_clip_write_form(const unsigned char *data, size_t n);

#endif
