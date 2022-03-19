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
    int w;
    int h;
    uint32_t* data;
} CcBitmap;


static inline
CcRect cc_rect(const CcBitmap* b)
{
    CcRect r = {
        0, 0, b->w, b->h
    };
    return  r;
}

static inline
uint32_t cc_bitmap_get(const CcBitmap* b, int x, int y, uint32_t bg_color)
{
    if (!cc_rect_contains(cc_rect(b), x, y)) return bg_color;
    return b->data[x + y * b->w];
}

static inline
void cc_bitmap_set(CcBitmap* b, int x, int y, uint32_t color)
{
    if (!cc_rect_contains(cc_rect(b), x, y)) return;
    b->data[x + y * b->w] = color;
}


void cc_bitmap_clear(CcBitmap* b, uint32_t color);

CcBitmap* cc_bitmap_create(int w, int h);
void cc_bitmap_destroy(CcBitmap* b);

CcBitmap* cc_bitmap_create_copy(const CcBitmap* b);
void cc_bitmap_copy_buffer(CcBitmap* b, unsigned char* rgba_buffer);

// a mask is a 1 channel (8 bit) alpha image.
void cc_bitmap_copy_mask(CcBitmap* b, const unsigned char* mask_buffer, uint32_t color);

void cc_bitmap_replace(CcBitmap* b, uint32_t old_color, uint32_t new_color);

// src_rect must be in bounds
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

void cc_bitmap_blit_unsafe(
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

void cc_bitmap_draw_spray(CcBitmap* b, int cx, int cy, int r, int density, uint32_t color);

void cc_bitmap_draw_circle(CcBitmap* b, int cx, int cy, int r, uint32_t color);
void cc_bitmap_draw_square(CcBitmap* b, int cx, int cy, int d, uint32_t color);
   
void cc_bitmap_interp_square(CcBitmap* b, int x1, int y1, int x2, int y2, int width, uint32_t color);
void cc_bitmap_interp_circle(CcBitmap* b, int x1, int y1, int x2, int y2, int radius, uint32_t color);
void cc_bitmap_interp_dotted(CcBitmap* b, int x1, int y1, int x2, int y2, uint32_t color);

void cc_bitmap_stroke_rect(CcBitmap* b, int x1, int y1, int x2, int y2, int width, uint32_t color);
void cc_bitmap_dotted_rect(CcBitmap* b, int x1, int y1, int x2, int y2, uint32_t color);
void cc_bitmap_fill_rect(CcBitmap* dst, int x1, int y1, int x2, int y2, uint32_t color);

void cc_bitmap_stroke_ellipse(CcBitmap* dst, int x1, int y1, int x2, int y2, uint32_t color);
void cc_bitmap_fill_ellipse(CcBitmap* dst, int x1, int y1, int x2, int y2, uint32_t color);

void cc_bitmap_stroke_polygon(
        CcBitmap* dst,
        CcCoord* points,
        int n,
        int closed,
        int width,
        uint32_t color
        );

void cc_bitmap_invert_colors(CcBitmap* bitmap);

void cc_bitmap_rotate_90(const CcBitmap* src, CcBitmap* dst);
void cc_bitmap_flip_horiz(const CcBitmap* src, CcBitmap* dst);
void cc_bitmap_flip_vert(const CcBitmap* src, CcBitmap* dst);

void cc_bitmap_zoom(const CcBitmap* src, CcBitmap* dst, int zoom);
void cc_bitmap_zoom_general(const CcBitmap* src, CcBitmap* dst, int zoom);
void cc_bitmap_zoom_power_of_2(const CcBitmap* src, CcBitmap* dst, int zoom_power);

CcRect cc_bitmap_flood_fill(CcBitmap* b, int sx, int sy, uint32_t new_color);
CcBitmap* cc_bitmap_transform(const CcBitmap* src, CcBitmap* dst, CcTransform A, uint32_t bg_color);


#endif
