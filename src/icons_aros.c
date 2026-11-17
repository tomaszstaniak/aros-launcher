/* SPDX-License-Identifier: MIT */
#include "icons_aros.h"

#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <graphics/gfx.h>
#include <utility/tagitem.h>
#include <workbench/icon.h>
#include <workbench/workbench.h>
#include <proto/icon.h>

#include "zk_resample.h"

enum { WANTED, READY, NONE };

struct slot { unsigned id; int size, state; unsigned char *pixels; };

struct ln_icons { struct slot *s; int n, cap, wanted; };

struct ln_icons *ln_icons_new(void) { return calloc(1, sizeof(struct ln_icons)); }

void ln_icons_free(struct ln_icons *c)
{
    int i;
    if (!c) return;
    for (i = 0; i < c->n; i++) free(c->s[i].pixels);
    free(c->s);
    free(c);
}

int ln_icons_wanted(const struct ln_icons *c) { return c->wanted; }

const unsigned char *ln_icons_get(struct ln_icons *c, unsigned id, int size)
{
    int i;
    for (i = 0; i < c->n; i++)
        if (c->s[i].id == id && c->s[i].size == size) return c->s[i].state == READY ? c->s[i].pixels : NULL;
    if (c->n == c->cap) {
        int cap = c->cap ? c->cap * 2 : 64;
        struct slot *g = realloc(c->s, sizeof *g * (size_t)cap);
        if (!g) return NULL;
        c->s = g; c->cap = cap;
    }
    c->s[c->n].id = id; c->s[c->n].size = size; c->s[c->n].state = WANTED; c->s[c->n].pixels = NULL;
    c->n++;
    c->wanted++;
    return NULL;
}

/* The icon's first picture as A,R,G,B bytes: its own true-colour picture,
 * or its palette picture. *own says whether the result must be freed. */
static const unsigned char *source(struct DiskObject *icon, int *w, int *h, int *own)
{
    const ULONG *argb = NULL;
    const UBYTE *indices = NULL;
    const struct ColorRegister *palette = NULL;
    ULONG width = 0, height = 0, colors = 0;
    LONG transparent = -1;
    struct TagItem original[] = {
        { ICONCTRLA_GetARGBImageData1, (IPTR)&argb },
        { ICONCTRLA_GetWidth, (IPTR)&width }, { ICONCTRLA_GetHeight, (IPTR)&height }, { TAG_DONE, 0 }
    };
    struct TagItem indexed[] = {
        { ICONCTRLA_GetImageData1, (IPTR)&indices }, { ICONCTRLA_GetPalette1, (IPTR)&palette },
        { ICONCTRLA_GetPaletteSize1, (IPTR)&colors }, { ICONCTRLA_GetTransparentColor1, (IPTR)&transparent },
        { TAG_DONE, 0 }
    };
    unsigned char *made;
    size_t count, i;
    *own = 0;
    /* ask for the size together with the picture: decoding can change it */
    IconControlA(icon, original);
    if (!width && icon->do_Gadget.Width > 0) width = (ULONG)icon->do_Gadget.Width;
    if (!height && icon->do_Gadget.Height > 0) height = (ULONG)icon->do_Gadget.Height;
    if (width < 1 || height < 1 || width > 1024 || height > 1024) return NULL;
    *w = (int)width; *h = (int)height;
    if (argb) return (const unsigned char *)argb;
    IconControlA(icon, indexed);
    if (!indices || !palette || colors < 1 || colors > 256) return NULL;
    count = (size_t)width * height;
    if (!(made = malloc(count * 4))) return NULL;
    for (i = 0; i < count; i++) {
        unsigned color = indices[i];
        unsigned char *p = made + i * 4;
        if ((LONG)color == transparent || color >= colors) { memset(p, 0, 4); continue; }
        p[0] = 255; p[1] = palette[color].red; p[2] = palette[color].green; p[3] = palette[color].blue;
    }
    *own = 1;
    return made;
}

/* size x size pixels with the icon scaled to fit, centred; NULL when the
 * icon has no picture this can use. */
static unsigned char *load(const char *path, int size)
{
    struct DiskObject *icon = GetIconTags((CONST_STRPTR)path, ICONGETA_FailIfUnavailable, TRUE,
                                          ICONGETA_RemapIcon, FALSE, ICONGETA_GenerateImageMasks, FALSE, TAG_DONE);
    const unsigned char *src;
    unsigned char *scaled = NULL, *out = NULL;
    int w = 0, h = 0, own = 0, tw, th, x, y;
    if (!icon) return NULL;
    if ((src = source(icon, &w, &h, &own))) {
        tw = w >= h ? size : (w * size + h / 2) / h;
        th = h >= w ? size : (h * size + w / 2) / w;
        if (tw < 1) tw = 1;
        if (th < 1) th = 1;
        if (zk_resample_argb(src, w, h, tw, th, &scaled) && (out = calloc((size_t)size * (size_t)size, 4))) {
            x = (size - tw) / 2; y = (size - th) / 2;
            for (h = 0; h < th; h++)
                memcpy(out + ((size_t)(y + h) * (size_t)size + (size_t)x) * 4, scaled + (size_t)h * (size_t)tw * 4, (size_t)tw * 4);
        }
        free(scaled);
        if (own) free((void *)src);
    }
    FreeDiskObject(icon);
    return out;
}

int ln_icons_work(struct ln_icons *c, const char *(*path_of)(void *ctx, unsigned id), void *ctx)
{
    int i;
    if (!c->wanted) return 0;
    /* the latest wishes first: they are the rows on screen now */
    for (i = c->n - 1; i >= 0; i--) {
        struct slot *s = &c->s[i];
        const char *path;
        if (s->state != WANTED) continue;
        path = path_of(ctx, s->id);
        s->pixels = path ? load(path, s->size) : NULL;
        s->state = s->pixels ? READY : NONE;
        c->wanted--;
        return 1;
    }
    c->wanted = 0;
    return 0;
}
