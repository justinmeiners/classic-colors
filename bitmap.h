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

#include "transform.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

static inline
int sign_of_int(int x) {
    return (x > 0) - (x < 0);
}

typedef enum {
    // ignore destination and replace
    COLOR_BLEND_REPLACE,
    // alpha blend assuming destination is opaque
    COLOR_BLEND_OVERLAY,
    // full alpha composite "over" operation
    COLOR_BLEND_FULL,
    // invert colors in destination
    COLOR_BLEND_INVERT,
} ColorBlend;


#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_BLACK 0x000000FF
#define COLOR_GRAY 0x888888FF
#define COLOR_RED   0xFF0000FF
#define COLOR_GREEN 0x00FF00FF
#define COLOR_BLUE  0x0000FFFF
#define COLOR_CLEAR 0x00000000

static inline
int color_component(uint32_t c, int i)
{
    return (c >> (24 - 8 * i)) & 0xFF;
}

static inline
uint32_t color_pack(int comps[])
{
    uint32_t color = 0;
    for (int i = 0; i < 4; ++i)
        color |= ((((uint32_t)comps[i]) & 0xFF) << (24 - 8 * i));
    return color;
}
static inline
void color_unpack(uint32_t c, int comps[])
{
    for (int i = 0; i < 4; ++i)
        comps[i] = color_component(c, i);
}

// http://x86asm.net/articles/fixed-point-arithmetic-and-tricks/
// https://en.wikipedia.org/wiki/Alpha_compositing

static inline
void color_blend_full(int* src_comps, int* dst_comps)
{
    // I would really like a nice integer math one, but couldn't figure out a good way.
    // This is currently only used for text, so shouldn't be a problem.
    float a1 = (float)src_comps[3] / 255.0;
    float a2 = (float)dst_comps[3] / 255.0;

    float alpha = a1 + a2 - a1 * a2;
    if (alpha > 0.0)
    {
        for (int i = 0; i < 3; ++i)
        {
            float c1 = src_comps[i] / 255.0;
            float c2 = dst_comps[i] / 255.0;
            
            float num = a1 * c1 + a2 * c2 * (1.0 - a1);
            float x = num / alpha;
            dst_comps[i] = round(x * 255.0);
        }
    }
    dst_comps[3] = round(alpha * 255.0);
}


// Use overlay when destination of alpha is 255.

// alpha: a + 1(1 - a) = 1
// color: (c_1 a_1 + c_2 ( 1 - a_1)) / 1
//      =  c_1 a_1 + c_2 - c_2 a_1
//      = c_2 + a_1(c_1 - c_2)
static inline
void color_blend_overlay(int* src_comps, int* dst_comps)
{
    // a * src + (1 -a) * dst
    for (int i = 0; i < 3; ++i)
    {
        dst_comps[i] += ((src_comps[i] - dst_comps[i]) * src_comps[3]) >> 8;
    }
    dst_comps[3] = 0xFF;
}

static inline
void color_blend_invert(int* src_comps, int* dst_comps)
{
    // invert colors
    for (int i = 0; i < 3; ++i) src_comps[i] = 255 - dst_comps[i];
    for (int i = 0; i < 3; ++i)
    {
        dst_comps[i] = ((src_comps[3] + 1) * src_comps[i] + (256 - src_comps[3]) * dst_comps[i]) >> 8;
    }
    dst_comps[3] = 0xFF;

}


typedef struct
{
    int x;
    int y;
    int w;
    int h;

} BitmapRect;

static inline
int intersect_half_open(int a, int b, int c, int d, int *out_a, int *out_b)
{
    int upper = MIN(b, d);
    int lower = MAX(a, c);

    if (lower >= upper)
    {
        *out_a = 0;
        *out_b = 0;
        return 0;
    }
    else
    {
        *out_a = lower;
        *out_b = upper;
        return 1;
    }
}

static inline
int bitmap_rect_intersect(BitmapRect a, BitmapRect b, BitmapRect* out)
{
    int intersects = intersect_half_open(a.x, a.x + a.w, b.x, b.x + b.w, &out->x, &out->w)
        && intersect_half_open(a.y, a.y + a.h, b.y, b.y + b.h, &out->y, &out->h);

    out->w -= out->x;
    out->h -= out->y;
    return intersects;
}

static inline
int bitmap_rect_equal(BitmapRect a, BitmapRect b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static inline
int bitmap_rect_contains(BitmapRect r, int x, int y)
{
    return r.x <= x && x < r.x + r.w && r.y <= y && y < r.y + r.h;
}

typedef struct
{
    int w;
    int h;
    uint32_t* data;
} Bitmap;

static inline
BitmapRect bitmap_rect(const Bitmap* b)
{
    BitmapRect r = {
        0, 0, b->w, b->h
    };
    return  r;
}

static
uint32_t bitmap_get(const Bitmap* b, int x, int y, uint32_t bg_color)
{
    if (!bitmap_rect_contains(bitmap_rect(b), x, y)) return bg_color;
    return b->data[x + y * b->w];
}

static
void bitmap_set(Bitmap* b, int x, int y, uint32_t color)
{
    if (!bitmap_rect_contains(bitmap_rect(b), x, y)) return;
    b->data[x + y * b->w] = color;
}


void bitmap_clear(Bitmap* b, uint32_t color);

Bitmap* bitmap_create(int w, int h);
void bitmap_destroy(Bitmap* b);

Bitmap* bitmap_create_copy(const Bitmap* b);
void bitmap_copy_buffer(Bitmap* b, unsigned char* rgba_buffer);

// a mask is a 1 channel (8 bit) alpha image.
void bitmap_copy_mask(Bitmap* b, const unsigned char* mask_buffer, uint32_t color);

void bitmap_replace(Bitmap* b, uint32_t old_color, uint32_t new_color);

// src_rect must be in bounds
void bitmap_blit(
        const Bitmap* src,
        Bitmap* dst,
        int src_x,
        int src_y,
        int dst_x,
        int dst_y,
        int w,
        int h,
        ColorBlend blend
        );

void bitmap_blit_unsafe(
        const Bitmap* src,
        Bitmap* dst,
        int src_x,
        int src_y,
        int dst_x,
        int dst_y,
        int w,
        int h,
        ColorBlend blend
        );

void bitmap_draw_spray(Bitmap* b, int cx, int cy, int r, int density, uint32_t color);

void bitmap_draw_circle(Bitmap* b, int cx, int cy, int r, uint32_t color);
void bitmap_draw_square(Bitmap* b, int cx, int cy, int d, uint32_t color);
   
void bitmap_interp_square(Bitmap* b, int x1, int y1, int x2, int y2, int width, uint32_t color);
void bitmap_interp_circle(Bitmap* b, int x1, int y1, int x2, int y2, int radius, uint32_t color);
void bitmap_interp_dotted(Bitmap* b, int x1, int y1, int x2, int y2, uint32_t color);

void bitmap_stroke_rect(Bitmap* b, int x1, int y1, int x2, int y2, int width, uint32_t color);
void bitmap_dotted_rect(Bitmap* b, int x1, int y1, int x2, int y2, uint32_t color);
void bitmap_fill_rect(Bitmap* dst, int x1, int y1, int x2, int y2, uint32_t color);

void bitmap_stroke_ellipse(Bitmap* dst, int x1, int y1, int x2, int y2, uint32_t color);
void bitmap_fill_ellipse(Bitmap* dst, int x1, int y1, int x2, int y2, uint32_t color);

void bitmap_invert_colors(Bitmap* bitmap);

void bitmap_rotate_90(const Bitmap* src, Bitmap* dst);
void bitmap_flip_horiz(const Bitmap* src, Bitmap* dst);
void bitmap_flip_vert(const Bitmap* src, Bitmap* dst);

void bitmap_zoom(const Bitmap* src, Bitmap* dst, int zoom);
void bitmap_zoom_general(const Bitmap* src, Bitmap* dst, int zoom);
void bitmap_zoom_power_of_2(const Bitmap* src, Bitmap* dst, int zoom_power);

BitmapRect bitmap_flood_fill(Bitmap* b, int sx, int sy, uint32_t new_color);
Bitmap* bitmap_transform(const Bitmap* src, Bitmap* dst, Transform A, uint32_t bg_color);

void align_line_to_45_angle(int start_x, int start_y, int end_x, int end_y, int* out_x, int* out_y);
void align_rect_to_square(int start_x, int start_y, int end_x, int end_y, int* out_x, int* out_y);

void test_bitmap_blending();


#endif
