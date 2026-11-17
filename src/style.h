/* SPDX-License-Identifier: MIT */
/* The looks of the panel as values: two themes, each full or compact. */
#ifndef LN_STYLE_H
#define LN_STYLE_H

struct ln_style {
    int width;          /* of the panel, without the shadow */
    int radius;         /* corners of the panel */
    int query_h;        /* the query line */
    int row_h;          /* one result */
    int card_h;         /* the calculator's result, shown large */
    int heading_h;      /* a section heading; 0: no headings */
    int footer_h;
    int inset;          /* selection bar from the panel's sides */
    int sel_radius;
    int pad;            /* text and icons from the panel's sides */
    int icon;           /* icon edge */
    int two_lines;      /* subtitle under the title, else beside it */
    int font_query, font_title, font_small, font_big;   /* pixels */
    /* colours, 0xAARRGGBB with straight alpha */
    unsigned long tint;         /* body; its alpha is used over a backdrop */
    unsigned long tint_plain;   /* body without a backdrop */
    unsigned long text, dim, match;
    unsigned long sel_fill, sel_text, sel_dim, sel_match;
    unsigned long rule, edge;
};

const struct ln_style *ln_style_get(int dark, int compact);

#endif
