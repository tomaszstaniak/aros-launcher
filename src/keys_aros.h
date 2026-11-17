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

#endif
