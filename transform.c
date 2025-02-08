#include <assert.h>
#include "transform.h"

CcTransform cc_transform_inverse(CcTransform t)
{
    double det = (t.m[0] * t.m[3] - t.m[1] * t.m[2]);
    double s = 1.0 / det;

    CcTransform inv = {
        { s * t.m[3], s * -t.m[1], s * -t.m[2], s * t.m[0] }
    };

    return inv;
}

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

CcBitmap cc_bitmap_transform(const CcBitmap* src, CcTransform A, uint32_t bg_color)
{
    double epsilon = 0.0001;
    Vec2 corners[4];
    corners[0].x = epsilon;
    corners[0].y = epsilon;

    corners[1].x = (double)src->w - epsilon;
    corners[1].y = epsilon;

    corners[2].x = (double)src->w - epsilon;
    corners[2].y = (double)src->h - epsilon;

    corners[3].x = epsilon;
    corners[3].y = (double)src->h - epsilon;


    Vec2 min = cc_transform_apply(A, corners[0]);
    Vec2 max = min; 

    for (int i = 1; i < 4; ++i)
    {
        Vec2 out_corner = cc_transform_apply(A, corners[i]);

        min.x = MIN(out_corner.x, min.x);
        min.y = MIN(out_corner.y, min.y);

        max.x = MAX(out_corner.x, max.x);
        max.y = MAX(out_corner.y, max.y);
    }

    min.x = floor(min.x);
    min.y = floor(min.y);
    max.x = ceil(max.x);
    max.y = ceil(max.y);

    CcBitmap dst = {
        .w = (int)(max.x - min.x),
        .h = (int)(max.y - min.y)
    };

    cc_bitmap_alloc(&dst);

    uint32_t *restrict out = dst.data;

    // A: R^n -> R^m
    // Iterate each pixel in the destination
    // and find it's preimage under A to know its previous color.
    CcTransform inverse = cc_transform_inverse(A);

    // center pixels

    for (int y = 0; y < dst.h; ++y)
    {
        for (int x = 0; x < dst.w; ++x)
        {
            Vec2 image;
            image.x = min.x + (double)x + 0.5;
            image.y = min.y + (double)y + 0.5;

            // I explored adding the derivative instead of transforming each time.
            // but, lots of small additions accumlate error and this isn't a big deal for a 2x2 matrix.
            Vec2 pre_image = cc_transform_apply(inverse, image);

            int src_x = (int)floor(pre_image.x);
            int src_y = (int)floor(pre_image.y);

            //printf("image: %f, %f pre: %f, %f\n", image.x, image.y, pre_image.x, pre_image.y);
            *out = cc_bitmap_get(src, src_x, src_y, bg_color);
            ++out;
        }
    }
    return dst;
}

