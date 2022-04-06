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

#include <stdint.h>
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


#define COLOR_WHITE 0xFFFFFFFF
#define COLOR_BLACK 0x000000FF
#define COLOR_GRAY 0x888888FF
#define COLOR_RED   0xFF0000FF
#define COLOR_GREEN 0x00FF00FF
#define COLOR_BLUE  0x0000FFFF
#define COLOR_CLEAR 0x00000000

static inline
int cc_color_component(uint32_t c, int i)
{
    return (c >> (24 - 8 * i)) & 0xFF;
}

static inline
uint32_t cc_color_pack(int comps[])
{
    uint32_t color = 0;
    for (int i = 0; i < 4; ++i)
        color |= ((((uint32_t)comps[i]) & 0xFF) << (24 - 8 * i));
    return color;
}
static inline
void cc_color_unpack(uint32_t c, int comps[])
{
    for (int i = 0; i < 4; ++i)
        comps[i] = cc_color_component(c, i);
}

// http://x86asm.net/articles/fixed-point-arithmetic-and-tricks/
// https://en.wikipedia.org/wiki/Alpha_compositing

static inline
void cc_color_blend_full(int* src_comps, int* dst_comps)
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
void cc_color_blend_overlay(int* src_comps, int* dst_comps)
{
    // a * src + (1 -a) * dst
    for (int i = 0; i < 3; ++i)
    {
        dst_comps[i] += ((src_comps[i] - dst_comps[i]) * src_comps[3]) >> 8;
    }
    dst_comps[3] = 0xFF;
}

static inline
void cc_color_blend_invert(int* src_comps, int* dst_comps)
{
    // invert colors
    for (int i = 0; i < 3; ++i) src_comps[i] = 255 - dst_comps[i];
    for (int i = 0; i < 3; ++i)
    {
        dst_comps[i] = ((src_comps[3] + 1) * src_comps[i] + (256 - src_comps[3]) * dst_comps[i]) >> 8;
    }
    dst_comps[3] = 0xFF;
}

static inline
void cc_color_blend_multiply(int* src_comps, int* dst_comps)
{
    // a * src + (1 -a) * dst
    for (int i = 0; i < 4; ++i)
        dst_comps[i] = ((src_comps[i] + 1) * dst_comps[i]) >> 8;
}

void color_blending_test();

#endif
