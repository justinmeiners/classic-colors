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
#include "color.h"

typedef struct
{
    int x;
    int y;
} BitmapCoord;

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
void extend_interval(int x, int* lower, int* upper)
{
    if (x < *lower)
    {
        *lower = x;
    }
    else if (x > *upper)
    {
        *upper = x;
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

static inline
BitmapRect bitmap_rect_extrema(int min_x, int min_y, int max_x, int max_y)
{
    BitmapRect result = {
        min_x,
        min_y, 
        max_x - min_x + 1,
        max_y - min_y + 1
    };
    return result;
}

BitmapRect bitmap_rect_around_points(BitmapCoord* points, int n);
BitmapRect bitmap_rect_pad(BitmapRect r, int pad_w, int pad_h);

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

void bitmap_stroke_polygon(
        Bitmap* dst,
        BitmapCoord* points,
        int n,
        int closed,
        int width,
        uint32_t color
        );

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
