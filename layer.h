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

#ifndef LAYER_H
#define LAYER_H

#include "bitmap.h"
#include "config.h"
#include "stb_truetype.h" 
#include <wctype.h>

typedef enum
{
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_RIGHT
} TextAlign;


typedef struct
{
    Bitmap* bitmaps;
    ColorBlend blend;

    int x;
    int y;

    int font;
    stbtt_fontinfo font_info;

    int font_size;
    TextAlign font_align;
    uint32_t font_color;

    size_t text_buffer_size;
    wchar_t* text;
} Layer;

static inline
BitmapRect layer_rect(const Layer* layer)
{
    BitmapRect r = {
        layer->x, layer->y, layer->bitmaps->w, layer->bitmaps->h
    };
    return r;
}

static inline
int layer_w(const Layer* layer)
{
    if (!layer->bitmaps) return 0;
    return layer->bitmaps->w;
}

static inline
int layer_h(const Layer* layer)
{
    if (!layer->bitmaps) return 0;
    return layer->bitmaps->h;
}

void layer_init(Layer* layer, int x, int y);
void layer_shutdown(Layer* layer);
void layer_reset(Layer* layer);
void layer_flip(Layer* layer, int horiz);
void layer_set_bitmap(Layer* layer, Bitmap* new_bitmap);
void layer_rotate_90(Layer* layer, int repeat);
void layer_rotate_angle(Layer* layer, double angle, uint32_t bg_color);
void layer_stretch(Layer* layer, int w, int h, int w_angle, int h_angle, uint32_t bg_color);
void layer_resize(Layer* layer, int new_w, int new_h, uint32_t bg_color);
void layer_ensure_size(Layer* layer, int w, int h);

void layer_set_text(Layer* layer, const wchar_t* text);

void layer_render(Layer* layer);

unsigned char* bitmap_compress(const Bitmap* b, size_t* out_size);
Bitmap* bitmap_decompress(unsigned char* compressed_data, size_t compressed_size);


void test_text_wordwrap(void);


void text_render(
        Bitmap* bitmap,
        const wchar_t* text,
        const stbtt_fontinfo* font_info,
        int font_size,
        TextAlign align,
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

