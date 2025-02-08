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

#ifndef BITMAP_H
#define BITMAP_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "plane.h"
#include "color.h"


typedef uint16_t CcDim;

typedef struct
{
    CcDim w;
    CcDim h;
    uint32_t stride;
    char *data;
} CcGrayBitmap;

void cc_gray_bitmap_alloc(CcGrayBitmap* b);
void cc_gray_bitmap_free(CcGrayBitmap *b);


typedef struct
{
    int w;
    int h;
    CcPixel* data;
} CcBitmap;


static inline
CcRect cc_bitmap_rect(const CcBitmap* b)
{
    CcRect r = {
        0, 0, b->w, b->h
    };
    return  r;
}

static inline
char *cc_bitmap_bytes_at(const CcBitmap *b, int x, int y)
{
    return (char *)(b->data + b->w * y + x);
}

static inline
CcPixel cc_bitmap_get(const CcBitmap* b, int x, int y, CcPixel bg_color)
{
    if (!cc_rect_contains(cc_bitmap_rect(b), x, y)) return bg_color;
    return b->data[x + y * b->w];
}

static inline
void cc_bitmap_set(CcBitmap* b, int x, int y, CcPixel color)
{
    if (!cc_rect_contains(cc_bitmap_rect(b), x, y)) return;
    b->data[x + y * b->w] = color;
}

void cc_bitmap_clear(CcBitmap* b, CcPixel color);

void cc_bitmap_alloc(CcBitmap* b);
void cc_bitmap_free(CcBitmap* b);
void cc_bitmap_copy(const CcBitmap *src, CcBitmap *dst);

// a mask is a 1 channel (8 bit) alpha image.
void cc_bitmap_copy_channel(CcBitmap* b, size_t channel_index, const CcGrayBitmap *channel);

void cc_bitmap_replace(CcBitmap* b, CcPixel old_color, CcPixel new_color);

void cc_bitmap_swap_channels(CcBitmap *b);

// requires:
//      the src and destination rects should not overlap in memory.
void cc_bitmap_blit(
        const CcBitmap* src,
        CcBitmap* dst,
        int src_x,
        int src_y,
        int dst_x,
        int dst_y,
        int w,
        int h,
        CcColorBlend blend
        );

// requires: src rect is in bounds
//      the src and destination rects should not overlap in memory.
void cc_bitmap_blit_unsafe(
        const CcBitmap *src,
        CcBitmap *dst,
        int src_x,
        int src_y,
        int dst_x,
        int dst_y,
        int w,
        int h,
        CcColorBlend blend
        );

void cc_bitmap_draw_spray(CcBitmap* b, int cx, int cy, int r, int density, CcPixel color);

void cc_bitmap_draw_circle(CcBitmap* b, int cx, int cy, int r, CcPixel color);
void cc_bitmap_draw_square(CcBitmap* b, int cx, int cy, int d, CcPixel color);
   
void cc_bitmap_interp_square(CcBitmap* b, int x1, int y1, int x2, int y2, int width, CcPixel color);
void cc_bitmap_interp_circle(CcBitmap* b, int x1, int y1, int x2, int y2, int radius, CcPixel color);
void cc_bitmap_interp_dotted(CcBitmap* b, int x1, int y1, int x2, int y2, CcPixel color);

void cc_bitmap_stroke_rect(CcBitmap* b, int x1, int y1, int x2, int y2, int width, CcPixel color);
void cc_bitmap_dotted_rect(CcBitmap* b, int x1, int y1, int x2, int y2, CcPixel color);
void cc_bitmap_fill_rect(CcBitmap* dst, int x1, int y1, int x2, int y2, CcPixel color);

void cc_bitmap_stroke_ellipse(CcBitmap* dst, int x1, int y1, int x2, int y2, CcPixel color);
void cc_bitmap_fill_ellipse(CcBitmap* dst, int x1, int y1, int x2, int y2, CcPixel color);

void cc_bitmap_invert_colors(CcBitmap* bitmap);

void cc_bitmap_rotate_90(const CcBitmap* src, CcBitmap* dst);
void cc_bitmap_flip_horiz(const CcBitmap* src, CcBitmap* dst);
void cc_bitmap_flip_vert(const CcBitmap* src, CcBitmap* dst);

// requires: 
//  src and dst are not the same bitmap
void cc_bitmap_zoom(const CcBitmap* src, CcBitmap* dst, int zoom);
void cc_bitmap_zoom_general(const CcBitmap* src, CcBitmap* dst, int zoom);
void cc_bitmap_zoom_power_of_2(const CcBitmap* src, CcBitmap* dst, int zoom_power);

CcRect cc_bitmap_flood_fill(CcBitmap* b, int sx, int sy, CcPixel new_color);

CcBitmap cc_bitmap_decompress(unsigned char* compressed_data, size_t compressed_size);
unsigned char* cc_bitmap_compress(const CcBitmap* b, size_t* out_size);

#endif
