/* 
 * Copyright (c) 2021 Justin Meiners
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CC_LAYER_H
#define CC_LAYER_H

#include "bitmap.h"
#include "config.h"
#include "stb_truetype.h" 
#include <wctype.h>

typedef enum
{
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_RIGHT
} CcTextAlign;


typedef struct
{
    CcBitmap* bitmaps;
    CcColorBlend blend;

    int x;
    int y;

    int font;
    stbtt_fontinfo font_info;

    int font_size;
    CcTextAlign font_align;
    uint32_t font_color;

    size_t text_buffer_size;
    wchar_t* text;
} CcLayer;

static inline
CcRect cc_layer_rect(const CcLayer* layer)
{
    CcRect r = {
        layer->x, layer->y, layer->bitmaps->w, layer->bitmaps->h
    };
    return r;
}

static inline
int cc_layer_w(const CcLayer* layer)
{
    if (!layer->bitmaps) return 0;
    return layer->bitmaps->w;
}

static inline
int cc_layer_h(const CcLayer* layer)
{
    if (!layer->bitmaps) return 0;
    return layer->bitmaps->h;
}

void cc_layer_init(CcLayer* layer, int x, int y);
void cc_layer_shutdown(CcLayer* layer);
void cc_layer_reset(CcLayer* layer);
void cc_layer_flip(CcLayer* layer, int horiz);
void cc_layer_set_bitmap(CcLayer* layer, CcBitmap* new_bitmap);
void cc_layer_rotate_90(CcLayer* layer, int repeat);
void cc_layer_rotate_angle(CcLayer* layer, double angle, uint32_t bg_color);
void cc_layer_stretch(CcLayer* layer, int w, int h, int w_angle, int h_angle, uint32_t bg_color);
void cc_layer_resize(CcLayer* layer, int new_w, int new_h, uint32_t bg_color);
void cc_layer_ensure_size(CcLayer* layer, int w, int h);

void cc_layer_set_text(CcLayer* layer, const wchar_t* text);

void cc_layer_render(CcLayer* layer);

unsigned char* cc_bitmap_compress(const CcBitmap* b, size_t* out_size);
CcBitmap* cc_bitmap_decompress(unsigned char* compressed_data, size_t compressed_size);


void test_text_wordwrap(void);


void text_render(
        CcBitmap* bitmap,
        const wchar_t* text,
        const stbtt_fontinfo* font_info,
        int font_size,
        CcTextAlign align,
        uint32_t font_color
    );


typedef struct
{
    // origin point of viewport in 
    // paint coordinates.
    int paint_x;
    int paint_y;

    // Width of visible pain area.
    // May be larger than actual image.
    int w;
    int h;

    int zoom;

} Viewport;

static inline
void viewport_init(Viewport* v)
{
    v->paint_x = 0;
    v->paint_y = 0;

    v->zoom = 1;

    v->w = 0;
    v->h = 0;
}

static inline
void viewport_coord_to_paint(const Viewport* v, int x, int y, int* out_x, int* out_y)
{
    *out_x = (x / v->zoom) + v->paint_x;
    *out_y = (y / v->zoom) + v->paint_y;
}

static inline
Viewport viewport_zoom_centered(const Viewport* v, int new_zoom)
{
    Viewport out;
    out.w = v->w;
    out.h = v->h;
    out.zoom = new_zoom;

    int center_x = v->paint_x + v->w / (2 * v->zoom);
    int center_y = v->paint_y + v->h / (2 * v->zoom);

    out.paint_x = center_x - v->w / (2 * new_zoom);
    out.paint_y = center_y - v->h / (2 * new_zoom);

    return out;
}



#endif

