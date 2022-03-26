#ifndef PLANE_H
#define PLANE_H

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

#endif