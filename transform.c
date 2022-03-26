#include <assert.h>
#include "transform.h"


CcBitmap* cc_bitmap_transform(const CcBitmap* src, CcBitmap* dst, CcTransform A, uint32_t bg_color)
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

    int w = (int)(max.x - min.x);
    int h = (int)(max.y - min.y);

    if (!dst)
    {
        dst = cc_bitmap_create(w, h);
    }

    // A: R^n -> R^m
    // Iterate each pixel in the destination
    // and find it's preimage under A to know its previous color.
    CcTransform inverse = cc_transform_inverse(A);

    // center pixels

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
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
            dst->data[x + y * dst->w] = cc_bitmap_get(src, src_x, src_y, bg_color);
        }
    }
    return dst;
}

