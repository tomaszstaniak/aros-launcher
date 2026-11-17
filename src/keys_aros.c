/* SPDX-License-Identifier: MIT */
#include "keys_aros.h"

#include <stdlib.h>
#include <string.h>

#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <libraries/iffparse.h>
#include <proto/exec.h>
#include <proto/iffparse.h>
#include <proto/keymap.h>

#include "zk_text.h"
#include "zk_text_aros.h"

#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d) ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))
#endif
#define ID_FTXT MAKE_ID('F','T','X','T')
#define ID_CHRS MAKE_ID('C','H','R','S')
#define ID_UTF8 MAKE_ID('U','T','F','8')

int ln_keys_translate(const struct IntuiMessage *im, enum ln_key *key, unsigned *mods,
                      char *utf8, size_t cap, size_t *n)
{
    UWORD code = im->Code, qual = im->Qualifier;
    int shift = (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
    int ctrl = (qual & IEQUALIFIER_CONTROL) != 0;
    int alt = (qual & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) != 0;
    int amiga = (qual & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)) != 0;
    struct InputEvent ie;
    UBYTE mapped[32];
    LONG got;

    *n = 0;
    *mods = (shift ? LN_MOD_SHIFT : 0) | ((ctrl || alt) ? LN_MOD_WORD : 0);
    if (code & IECODE_UP_PREFIX) return 0;
    switch (code) {
    case RAWKEY_LEFT:      *key = LN_K_LEFT; return 1;
    case RAWKEY_RIGHT:     *key = LN_K_RIGHT; return 1;
    case RAWKEY_UP:        *key = LN_K_UP; return 1;
    case RAWKEY_DOWN:      *key = LN_K_DOWN; return 1;
    case RAWKEY_HOME:      *key = LN_K_HOME; return 1;
    case RAWKEY_END:       *key = LN_K_END; return 1;
    case RAWKEY_PAGEUP:    *key = LN_K_PAGE_UP; return 1;
    case RAWKEY_PAGEDOWN:  *key = LN_K_PAGE_DOWN; return 1;
    case RAWKEY_BACKSPACE: *key = LN_K_BACKSPACE; return 1;
    case RAWKEY_DELETE:    *key = LN_K_DELETE; return 1;
    case RAWKEY_RETURN:
    case RAWKEY_KP_ENTER:  *key = LN_K_RETURN; return 1;
    case RAWKEY_ESCAPE:    *key = LN_K_ESC; return 1;
    case RAWKEY_TAB:       *key = LN_K_TAB; return 1;
    default: break;
    }

    memset(&ie, 0, sizeof ie);
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = code;
    ie.ie_Qualifier = qual;
    /* the keys before this one, for dead keys */
    if (im->IAddress) ie.ie_EventAddress = (APTR)(IPTR)*(ULONG *)im->IAddress;
    if (ctrl || amiga) {
        ie.ie_Qualifier &= ~(IEQUALIFIER_CONTROL | IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND |
                             IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT | IEQUALIFIER_LALT | IEQUALIFIER_RALT);
        if (MapRawKey(&ie, (STRPTR)mapped, sizeof mapped, NULL) != 1) return 0;
        *mods = 0;
        switch (mapped[0] | 0x20) {
        case 'a': *key = LN_K_SELECT_ALL; return 1;
        case 'c': *key = LN_K_COPY; return 1;
        case 'x': *key = LN_K_CUT; return 1;
        case 'v': *key = LN_K_PASTE; return 1;
        case 'z': *key = shift ? LN_K_REDO : LN_K_UNDO; return 1;
        case 'y': *key = LN_K_REDO; return 1;
        default: return 0;
        }
    }
    got = MapRawKey(&ie, (STRPTR)mapped, sizeof mapped, NULL);
    if (got <= 0) return 0;
    /* the keymap gives the system's set */
    *n = zt_keys_utf8(mapped, (size_t)got, utf8, cap);
    if (!*n) return 0;
    *mods = 0;
    *key = LN_K_TEXT;
    return 1;
}

int ln_clip_write(const char *utf8, size_t n)
{
    struct IFFHandle *h;
    size_t k = 0;
    char *local = zt_local_copy(utf8, n, &k);
    int ok = 0;
    if (!local) return 0;
    if ((h = AllocIFF())) {
        if ((h->iff_Stream = (IPTR)OpenClipboard(0))) {
            InitIFFasClip(h);
            if (!OpenIFF(h, IFFF_WRITE)) {
                ok = !PushChunk(h, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN)
                     && !PushChunk(h, 0, ID_CHRS, (LONG)k)
                     && WriteChunkBytes(h, local, (LONG)k) == (LONG)k && !PopChunk(h)
                     && !PushChunk(h, 0, ID_UTF8, (LONG)n)
                     && WriteChunkBytes(h, (APTR)utf8, (LONG)n) == (LONG)n && !PopChunk(h)
                     && !PopChunk(h);
                CloseIFF(h);
            }
            CloseClipboard((struct ClipboardHandle *)h->iff_Stream);
        }
        FreeIFF(h);
    }
    zt_free(local);
    return ok;
}

char *ln_clip_read(size_t *n)
{
    struct IFFHandle *h;
    char *utf = NULL, *chrs = NULL, *out = NULL;
    LONG un = 0, cn = 0;
    *n = 0;
    if (!(h = AllocIFF())) return NULL;
    if ((h->iff_Stream = (IPTR)OpenClipboard(0))) {
        InitIFFasClip(h);
        if (!OpenIFF(h, IFFF_READ)) {
            static const LONG stops[] = { ID_FTXT, ID_UTF8, ID_FTXT, ID_CHRS };
            if (!StopChunks(h, (LONG *)stops, 2)) {
                while (!ParseIFF(h, IFFPARSE_SCAN)) {
                    struct ContextNode *c = CurrentChunk(h);
                    int is_utf = c && c->cn_ID == ID_UTF8;
                    char **dst = is_utf ? &utf : &chrs;
                    LONG *dn = is_utf ? &un : &cn, size = c ? c->cn_Size : 0;
                    if (!c || *dst || size <= 0 || size > 65536) continue;      /* the first of each kind */
                    if (!(*dst = malloc((size_t)size))) break;
                    *dn = ReadChunkBytes(h, *dst, size);
                    if (*dn != size) { free(*dst); *dst = NULL; *dn = 0; }
                }
            }
            CloseIFF(h);
        }
        CloseClipboard((struct ClipboardHandle *)h->iff_Stream);
    }
    FreeIFF(h);
    if (utf && zt_valid(utf, (size_t)un)) {
        out = utf; utf = NULL; *n = (size_t)un;
    } else if (chrs && (out = malloc((size_t)cn * 4 + 1))) {
        LONG i;
        size_t k = 0;
        for (i = 0; i < cn; i++) {
            UBYTE ch = (UBYTE)chrs[i];
            if (ch == '\n' || ch == '\r' || ch == '\t') { out[k++] = (char)ch; continue; }
            k += zt_from_local(ch, out + k);
        }
        *n = k;
    }
    free(utf); free(chrs);
    return out;
}
