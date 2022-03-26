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

#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "bitmap.h"

// TODO: should this be an affine transform?
// how do we invert affine transform?
typedef struct
{
    double m[4];
} CcTransform;

typedef struct
{
    double x;
    double y;
} Vec2;

static inline CcTransform cc_transform_create(double a, double b, double c, double d)
{
    CcTransform t = {
        { a, b, c, d }
    };
    return t;
}

static inline CcTransform cc_transform_identity(void)
{
    CcTransform t = {
        { 1.0, 0.0, 0.0, 1.0 }
    };
    return t;
}

static inline CcTransform cc_transform_inverse(CcTransform t)
{
    double det = (t.m[0] * t.m[3] - t.m[1] * t.m[2]);
    double s = 1.0 / det;

    CcTransform inv = {
        { s * t.m[3], s * -t.m[1], s * -t.m[2], s * t.m[0] }
    };

    return inv;
}

static inline CcTransform cc_transform_rotate(double angle)
{
    double c = cos(angle);
    double s = sin(angle);

    CcTransform t = {
        { c, s, -s, c }
    };
    return t;
}

static inline
CcTransform cc_transform_scale(double w, double h)
{
    CcTransform t = {
        { w, 0.0, 0.0, h }
    };
    return t;
}

static inline
CcTransform cc_transform_skew(double x_angle, double y_angle)
{
    double c_x = cos(x_angle);
    double s_x = sin(x_angle);

    double c_y = cos(y_angle);
    double s_y = sin(y_angle);

    CcTransform t = {
        {  c_x, s_x, -s_y, c_y }
    };
    return t;
}

static inline
CcTransform cc_transform_concat(CcTransform a, CcTransform b)
{
    CcTransform t = {
        { a.m[0] * b.m[0] + a.m[1] * b.m[2],
          a.m[0] * b.m[1] + a.m[1] * b.m[3],
          a.m[2] * b.m[0] + a.m[3] * b.m[2],
          a.m[2] * b.m[1] + a.m[3] * b.m[3],
        }
    };
    return t;
}

static inline
Vec2 cc_transform_apply(CcTransform t, Vec2 v)
{
    Vec2 r;
    r.x = t.m[0] * v.x + t.m[1] * v.y;
    r.y = t.m[2] * v.x + t.m[3] * v.y;
    return r;
}

CcBitmap* cc_bitmap_transform(const CcBitmap* src, CcBitmap* dst, CcTransform A, uint32_t bg_color);

#endif


