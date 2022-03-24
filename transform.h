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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define SWAP(x_, y_, T) do { T SWAP = x_; x_ = y_; y_ = SWAP; } while (0)

static inline
int sign_of_int(int x) {
    return (x > 0) - (x < 0);
}

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

typedef struct
{
    int x;
    int y;
} CcCoord;

typedef struct
{
    int x;
    int y;
    int w;
    int h;
} CcRect;

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
int interval_clamp(int x, int lower, int upper)
{
    if (x < lower)
    {
        return lower;
    }
    else if (x > upper)
    {
        return upper;
    }
    else
    {
        return x;
    }
}

static inline
int cc_rect_intersect(CcRect a, CcRect b, CcRect* out)
{
    int intersects = intersect_half_open(a.x, a.x + a.w, b.x, b.x + b.w, &out->x, &out->w)
        && intersect_half_open(a.y, a.y + a.h, b.y, b.y + b.h, &out->y, &out->h);

    out->w -= out->x;
    out->h -= out->y;
    return intersects;
}

static inline
int cc_rect_equal(CcRect a, CcRect b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static inline
int cc_rect_contains(CcRect r, int x, int y)
{
    return r.x <= x && x < r.x + r.w && r.y <= y && y < r.y + r.h;
}

static inline
CcRect cc_rect_from_extrema(int min_x, int min_y, int max_x, int max_y)
{
    CcRect result = {
        min_x,
        min_y, 
        max_x - min_x + 1,
        max_y - min_y + 1
    };
    return result;
}

static inline
CcRect cc_rect_around_corners(int x1, int y1, int x2, int y2)
{
    if (x2 < x1) {
        SWAP(x1, x2, int);
    }
    if (y2 < y1) {
        SWAP(y1, y2, int);
    }
    return cc_rect_from_extrema(x1, y1, x2, y2);
}

static inline
int int_ceil(int a, int b)
{
    return (a / b) + (a % b != 0);
}

CcRect cc_rect_around_points(const CcCoord* points, int n);
CcRect cc_rect_pad(CcRect r, int pad_w, int pad_h);

void cc_line_align_to_45(int start_x, int start_y, int end_x, int end_y, int* out_x, int* out_y);
void align_rect_to_square(int start_x, int start_y, int end_x, int end_y, int* out_x, int* out_y);

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

#endif


