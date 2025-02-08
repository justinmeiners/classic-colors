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


#ifndef CC_COLOR_H
#define CC_COLOR_H

#include "common.h"
#include <math.h>

typedef enum {
    // ignore destination and replace
    COLOR_BLEND_REPLACE,
    // alpha blend assuming destination is opaque
    COLOR_BLEND_OVERLAY,
    // full alpha composite "over" operation
    COLOR_BLEND_FULL,
    // invert colors in destination
    COLOR_BLEND_INVERT,
    // 
    COLOR_BLEND_MULTIPLY,

} CcColorBlend;


typedef uint32_t CcPixel;

#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_BLACK 0x000000FF
#define COLOR_GRAY 0x888888FF
#define COLOR_RED   0xFF0000FF
#define COLOR_GREEN 0x00FF00FF
#define COLOR_BLUE  0x0000FFFF
#define COLOR_CLEAR 0x00000000

static inline
CcPixel cc_color_swap(CcPixel x)
{
    return ((x & 0xFF000000) >> 24) |
        ((x & 0x00FF0000) >> 8)  |
        ((x & 0x0000FF00) << 8)  |
        ((x & 0x000000FF) << 24);
}

static inline
CcPixel cc_color_pack(const uint8_t comps[])
{
    CcPixel color;
    memcpy(&color, comps, sizeof(CcPixel));
    return cc_color_swap(color);
}

static inline
void cc_color_unpack(CcPixel c, uint8_t comps[])
{
    c = cc_color_swap(c);
    memcpy(comps, &c, sizeof(CcPixel));
}

// divide by 255
static inline
int16_t fixed_unscale_255(int32_t x)
{
    int32_t wide = (x + (x >> 7)) >> 8;
    return (int16_t)wide;
}

static inline
int16_t fixed_mul_255(int16_t x, int16_t y)
{
    int32_t scaled = x * y;
    return fixed_unscale_255(scaled);
}


// http://x86asm.net/articles/fixed-point-arithmetic-and-tricks/
// https://en.wikipedia.org/wiki/Alpha_compositing

static inline
void cc_color_blend_full2(
    const uint8_t *restrict src,
    const uint8_t *restrict dst,
    uint8_t *restrict out
) {
    // I would really like a nice integer math one, but couldn't figure out a good way.
    // This is currently only used for text, so shouldn't be a problem.
    float a1 = (float)src[3] / 255.0;
    float a2 = (float)dst[3] / 255.0;

    float alpha = a1 + a2 - a1 * a2;
    if (alpha > 0.0)
    {
        for (int i = 0; i < 3; ++i)
        {
            float c1 = src[i] / 255.0;
            float c2 = dst[i] / 255.0;
            
            float num = a1 * c1 + a2 * c2 * (1.0 - a1);
            float x = num / alpha;
            out[i] = round(x * 255.0);
        }
    }
    out[3] = round(alpha * 255.0);
}


// Use overlay when destination of alpha is 255.

// alpha: a + 1(1 - a) = 1
// color: (c_1 a_1 + c_2 ( 1 - a_1)) / 1
//      =  c_1 a_1 + c_2 - c_2 a_1
//      = c_2 + a_1(c_1 - c_2)
static inline
void cc_color_blend_overlay2(
    const uint8_t *restrict src,
    const uint8_t *restrict dst,
    uint8_t *restrict out
) {
    int16_t a = src[3];
    // a * src + (1 - a) * dst
    for (size_t i = 0; i < 3; ++i)
    {
        out[i] = (uint8_t)(fixed_mul_255(255 - a, dst[i]) + fixed_mul_255(src[i], a));
    }
    out[3] = 0xFF;
}


static inline
void cc_color_blend_invert2(
    const uint8_t *restrict src,
    const uint8_t *restrict dst,
    uint8_t *restrict out)
{
    // invert colors: src = 1-dst
    // a * src + (1 - a) * dst
    // a * (1 - dst) + (1 - a) * dst
    // a - a * dst + dst - a * dst
    int16_t a = src[3];

    for (size_t i = 0; i < 3; ++i)
    {
        out[i] = (uint8_t)((int16_t)dst[i] + (int16_t)a - 2 * fixed_mul_255(dst[i], a));
    }
    out[3] = dst[3];
}

static inline
void cc_color_blend_multiply2(
    const uint8_t *restrict src,
    const uint8_t *restrict dst,
    uint8_t *restrict out
) {
    // src[i] * dst[i]
    for (size_t i = 0; i < 4; ++i)
    {
        out[i] = (uint8_t)fixed_mul_255(dst[i], src[i]);
    }
}

static inline
CcPixel cc_color_blend_overlay(
    CcPixel src,
    CcPixel dst
) {
    uint8_t src_comps[4];
    uint8_t dst_comps[4];
    uint8_t out_comps[4];
    cc_color_unpack(src, src_comps);
    cc_color_unpack(dst, dst_comps);
    cc_color_blend_overlay2(src_comps, dst_comps, out_comps);
    return cc_color_pack(out_comps);
}

static inline
CcPixel cc_color_blend_invert(
    CcPixel src,
    CcPixel dst
) {
    uint8_t src_comps[4];
    uint8_t dst_comps[4];
    uint8_t out_comps[4];
    cc_color_unpack(src, src_comps);
    cc_color_unpack(dst, dst_comps);
    cc_color_blend_invert2(src_comps, dst_comps, out_comps);
    return cc_color_pack(out_comps);
}

static inline
CcPixel cc_color_blend_multiply(
    CcPixel src,
    CcPixel dst
) {
    uint8_t src_comps[4];
    uint8_t dst_comps[4];
    uint8_t out_comps[4];
    cc_color_unpack(src, src_comps);
    cc_color_unpack(dst, dst_comps);
    cc_color_blend_multiply2(src_comps, dst_comps, out_comps);
    return cc_color_pack(out_comps);
}

static inline
CcPixel cc_color_blend_full(
    CcPixel src,
    CcPixel dst
) {
    uint8_t src_comps[4];
    uint8_t dst_comps[4];
    uint8_t out_comps[4];
    cc_color_unpack(src, src_comps);
    cc_color_unpack(dst, dst_comps);
    cc_color_blend_full2(src_comps, dst_comps, out_comps);
    return cc_color_pack(out_comps);
}



void color_blending_test();

#endif
