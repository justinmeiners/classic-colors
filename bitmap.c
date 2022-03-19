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


#include <assert.h>
#include "bitmap.h"

#define SWAP(x_, y_, T) do { T SWAP = x_; x_ = y_; y_ = SWAP; } while (0)

void cc_bitmap_clear(CcBitmap* b, uint32_t color)
{
    int N = b->w * b->h;
    if (color == 0 || color == 0xFFFFFFFF)
    {
        memset(b->data, color, sizeof(uint32_t) * N);
    }
    else
    {
   	    for (int i = 0; i < N; ++i) b->data[i] = color;
    }
}

CcBitmap* cc_bitmap_create(int w, int h)
{
    assert(w >= 0);
    assert(h >= 0);

    CcBitmap* b = malloc(sizeof(CcBitmap));
    b->w = w;
    b->h = h;
    b->data = malloc(w * h * sizeof(uint32_t));
    return b;
}

void cc_bitmap_destroy(CcBitmap* b)
{
    free(b->data);
    free(b);
}

CcBitmap* cc_bitmap_create_copy(const CcBitmap* b)
{
    CcBitmap* n = cc_bitmap_create(b->w, b->h);
    memcpy(n->data, b->data, sizeof(uint32_t) * b->w * b->h);
    return n;
}

void cc_bitmap_copy_buffer(CcBitmap* b, unsigned char* rgba_buffer)
{
    int comps[4];
    int n = b->w * b->h;
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            comps[j] = rgba_buffer[i * 4 + j];
        }
        b->data[i] = color_pack(comps);
    }
}

void cc_bitmap_copy_mask(CcBitmap* b, const unsigned char* mask_buffer, uint32_t color)
{
    int comps[4];
    color_unpack(color, comps);

    int n = b->w * b->h;
    for (int i = 0; i < n; ++i)
    {
        comps[3] = mask_buffer[i];
        b->data[i] = color_pack(comps);
    }
}

void cc_bitmap_replace(CcBitmap* b, uint32_t old_color, uint32_t new_color)
{
    int n = b->w * b->h;
    for (int i = 0; i < n; ++i)
    {
        if (b->data[i] == old_color)
            b->data[i] = new_color;
    }
}

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
        )
{
    CcRect dst_rect = {
        dst_x, dst_y, w, h
    };

    if (!cc_rect_intersect(dst_rect, cc_bitmap_rect(dst), &dst_rect)) return;

    cc_bitmap_blit_unsafe(
            src,
            dst, 
            src_x + (dst_rect.x - dst_x),
            src_y + (dst_rect.y - dst_y),
            dst_rect.x,
            dst_rect.y,
            dst_rect.w,
            dst_rect.h,
            blend
        );
}


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
        )
{
    assert(src_x + w <= src->w);
    assert(src_y + h <= src->h);

    int src_comps[4];
    int dst_comps[4];

#define LOOP(code_) \
    for (int y = 0; y < h; ++y) \
    { \
        for (int x = 0; x < w; ++x) \
        { \
            int src_index = (src_x + x) + (src_y + y) * src->w; \
            int dst_index = (dst_x + x) + (dst_y + y) * dst->w; \
            color_unpack(src->data[src_index], src_comps); \
            color_unpack(dst->data[dst_index], dst_comps); \
            code_ \
        } \
    }


    switch (blend)
    {
        case COLOR_BLEND_REPLACE:
            for (int y = 0; y < h; ++y)
            {
                int dst_index = dst_x + (dst_y + y) * dst->w;
                int src_index = src_x + (src_y + y) * src->w;
                memcpy(dst->data + dst_index, src->data + src_index, w * sizeof(uint32_t));
            }
            break;
        case COLOR_BLEND_OVERLAY:
            LOOP( 
               color_blend_overlay(src_comps, dst_comps);
               dst->data[dst_index] = color_pack(dst_comps);
            )
            break;
        case COLOR_BLEND_FULL:
            LOOP( 
               color_blend_full(src_comps, dst_comps);
               dst->data[dst_index] = color_pack(dst_comps);
            )
            break;
        case COLOR_BLEND_INVERT:
            LOOP(
                color_blend_invert(src_comps, dst_comps);
                dst->data[dst_index] = color_pack(dst_comps);
            ) 
            break;
    }
#undef LOOP
}

#define BITMAP_DRAW_LINE(func) {\
    int m, x, y, sx, ex, sy, ey, index; \
    if (x1 == x2 && y1 == y2) \
    { \
        x = x1; y = y1; \
        func; return; \
    } \
    if (abs(x2 - x1) > abs(y2 - y1)) \
    { \
        if (x1 > x2) \
        { \
            sx = x2; \
            ex = x1; \
            sy = y2; \
            ey = y1; \
        } \
        else \
        { \
            sx = x1; \
            ex = x2; \
            sy = y1; \
            ey = y2; \
        } \
        int w = ex - sx; \
        m = ((ey - sy) * 10000) / w; \
        for (x = sx; x <= ex; ++x) \
        { \
            index = x - sx; \
            y = sy + ((x - sx) * m) / 10000; \
            func; \
        }  \
    } \
    else \
    { \
        if (y1 > y2) \
        { \
            sx = x2; \
            ex = x1; \
            sy = y2; \
            ey = y1; \
        } \
        else \
        { \
            sx = x1; \
            ex = x2; \
            sy = y1; \
            ey = y2; \
        } \
        int h = ey - sy; \
        m = ((ex - sx) * 10000) / h; \
        for (y = sy; y <= ey; ++y) \
        { \
            index = y - sy; \
            x = sx + ((y - sy) * m) / 10000; \
            func; \
        } \
    } \
} \
 

void cc_bitmap_draw_spray(CcBitmap* b, int cx, int cy, int r, int density, uint32_t color)
{
    CcRect rect = {
        cx - r, cy - r, 2 * r, 2 * r
    };
    cc_rect_intersect(rect, cc_bitmap_rect(b), &rect);

    for (int y = rect.y; y < rect.y + rect.h; ++y)
    {
        for (int x = rect.x; x < rect.x + rect.w; ++x)
        {
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r)
            {
                if (rand() % 1000 < density * 10)
                {
                    b->data[x + y * b->w] = color;
                }
            }
        }
    }
}

void cc_bitmap_draw_circle(CcBitmap* b, int cx, int cy, int r, uint32_t color)
{
    CcRect rect = {
        cx - r, cy - r, 2 * r, 2 * r
    };
    cc_rect_intersect(rect, cc_bitmap_rect(b), &rect);

    for (int y = rect.y; y < rect.y + rect.h; ++y)
    {
        for (int x = rect.x; x < rect.x + rect.w; ++x)
        {
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r)
            {
                b->data[x + y * b->w] = color;
            }
        }
    }
}

void cc_bitmap_draw_square(CcBitmap* b, int cx, int cy, int d, uint32_t color)
{
    CcRect rect = {
        cx - d / 2, cy - d / 2, d, d
    };
    cc_rect_intersect(rect, cc_bitmap_rect(b), &rect);

    for (int y = 0; y < rect.h; ++y)
    {
        for (int x = 0; x < rect.w; ++x)
        {
            b->data[(rect.x + x) + (rect.y + y) * b->w] = color;
        }
    }
}

   
void cc_bitmap_interp_square(CcBitmap* b, int x1, int y1, int x2, int y2, int width, uint32_t color)
{
    BITMAP_DRAW_LINE(cc_bitmap_draw_square(b, x, y, width, color));
}

void cc_bitmap_interp_circle(CcBitmap* b, int x1, int y1, int x2, int y2, int radius, uint32_t color)
{
    BITMAP_DRAW_LINE(cc_bitmap_draw_circle(b, x, y, radius, color));
}


void cc_bitmap_interp_dotted(CcBitmap* b, int x1, int y1, int x2, int y2, uint32_t color)
{
    BITMAP_DRAW_LINE(
            if (index % 8 < 4)
            {
                cc_bitmap_set(b, x, y, color);
            }
    );
}


void cc_bitmap_dotted_rect(CcBitmap* b, int x1, int y1, int x2, int y2, uint32_t color)
{
    cc_bitmap_interp_dotted(b, x1, y1, x2, y1, color);
    cc_bitmap_interp_dotted(b, x2, y1, x2, y2, color);
    cc_bitmap_interp_dotted(b, x2, y2, x1, y2, color);
    cc_bitmap_interp_dotted(b, x1, y2, x1, y1, color);
}

void cc_bitmap_fill_rect(CcBitmap* dst, int x1, int y1, int x2, int y2, uint32_t color)
{
    if (x2 < x1) {
        SWAP(x1, x2, int);
    }
    if (y2 < y1) {
        SWAP(y1, y2, int);
    }

    CcRect r = {
        x1, y1, x2 - x1 + 1, y2 - y1 + 1
    };

    CcRect to_fill;
    cc_rect_intersect(r, cc_bitmap_rect(dst), &to_fill);

    for (int y = 0; y < to_fill.h; ++y)
    {
        for (int x = 0; x < to_fill.w; ++x)
        {
            int dst_index = (to_fill.x + x) + (to_fill.y + y) * dst->w;
            dst->data[dst_index] = color;
        }
    }
}

void cc_bitmap_stroke_ellipse(CcBitmap* dst, int x0, int y0, int x1, int y1, uint32_t color)
{
    // I did one.. but it sucked
    // http://members.chello.at/~easyfilter/bresenham.html
    int a = abs(x1 - x0);
    int b = abs(y1 - y0);
    int b1 = b & 1;

   long dx = 4 * ( 1 - a) * b * b, dy = 4*(b1 + 1) * a * a;
   long err = dx + dy + b1 * a*a, e2; 

   if (x0 > x1) { x0 = x1; x1 += a; }
   if (y0 > y1) y0 = y1; 

   y0 += (b+1)/2; y1 = y0-b1;
   a *= 8*a; b1 = 8*b*b;

#define PUT(x_, y_) cc_bitmap_set(dst, x_, y_, color);
   do {
       PUT(x1, y0); 
       PUT(x0, y0); 
       PUT(x0, y1); 
       PUT(x1, y1);

       e2 = 2*err;
       if (e2 <= dy) {
           y0++; y1--;
           err += dy += a;
       }  

       if (e2 >= dx || 2*err > dy)
       {
           x0++; x1--;
           err += dx += b1;
       } 
   } while (x0 <= x1);
   
   while (y0-y1 < b) { 
       PUT(x0-1, y0); 
       PUT(x1+1, y0++); 
       PUT(x0-1, y1);
       PUT(x1+1, y1--); 
   }
#undef PUT
}

/*
void cc_bitmap_stroke_ellipse(CcBitmap* dst, int x1, int y1, int x2, int y2, uint32_t color)
{
    int min_x = MIN(x1, x2);
    int min_y = MIN(y1, y2);

    int w = abs(x2 - x1);
    int h = abs(y2 - y1);

    long c_x = w / 2;
    long c_y = h / 2;

    if (c_x == 0 || c_y == 0)
    {
        return;
    }

#define PUT(x_, y_) if ((x_) >= 0 && (x_) < dst->w && (y_) >= 0 && (y_) < dst->h) { dst->data[(x_) + (y_) * dst->w] = color; }

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            long dx = (x - c_x);
            long dy = (y - c_y);

            long d = (dx * dx * 100) + (dy * dy) * (c_x * c_x * 100)/ (c_y * c_y);

            if (d <= (c_x * c_x + c_x) * 100 && d >= (c_x * c_x - c_x) * 100)
            {
                PUT(x + min_x, y + min_y);
            }
        }
    }
#undef PUT
}
*/



static
void cc_fill_even_odd_(CcBitmap* dst, CcRect rect, uint32_t color, uint32_t sentinel)
{
    int from;
    int to;
    int dir;

    if (rect.x < 0) {
        dir = -1;
        from = rect.x + rect.w;
        to = rect.x;
    } else {
        dir = 1;
        from = rect.x;
        to = rect.x + rect.w;
    }

#define PUT(x_, y_) dst->data[(x_) + (y_) * dst->w] = color;
    for (int y = rect.y; y < rect.y + rect.h; ++y)
    { 
        if (y < 0 || y >= dst->h) continue;

        int in_boundary = 0;
        int counter = 0;

        for (int x = from; (rect.x <= x && x < rect.x + rect.w); x += dir)
        {
            if (x < 0 || x >= dst->w) continue;

            uint32_t current = dst->data[x + y * dst->w];

            if (current == sentinel)
            {
                PUT(x, y);

                if (!in_boundary)
                {
                    counter += 1;
                    in_boundary = 1;
                }
            }
            else
            {
                in_boundary = 0;
            }

            if (counter % 2 == 1 && y != rect.y && y != rect.y + rect.h - 1)
            {
                PUT(x, y);
            }
        }
    }
#undef PUT
}

void cc_bitmap_fill_ellipse(CcBitmap* dst, int x1, int y1, int x2, int y2, uint32_t color)
{
    // ultimate hack
    uint32_t special_marker = 0x12345678;
    cc_bitmap_stroke_ellipse(dst, x1, y1, x2, y2, special_marker);

    int min_x = x1;
    int max_x = x1;
    extend_interval(x2, &min_x, &max_x);

    int min_y = y1;
    int max_y = y1;
    extend_interval(y2, &min_y, &max_y);

    cc_fill_even_odd_(
        dst,
        cc_rect_extrema(min_x, min_y, max_x, max_y),
        color,
        special_marker
        );
}

void cc_bitmap_stroke_polygon(
        CcBitmap* dst,
        CcCoord* points,
        int n,
        int closed,
        int width,
        uint32_t color
        )
{
    for (int i = 0; i < n - 1; ++i)
    {
        cc_bitmap_interp_square(
                dst,
                points[i].x,
                points[i].y,
                points[i + 1].x,
                points[i + 1].y,
                width,
                color
        );
    }

    if (closed && n > 2)
    {
        cc_bitmap_interp_square(
                dst,
                points[n - 1].x,
                points[n - 1].y,
                points[0].x,
                points[0].y,
                width,
                color
        );
    }
}

static int int_compare_(const void *ap, const void *bp)
{
    const int* a = ap;
    const int* b = bp;
    return *a - *b;
}

void cc_bitmap_fill_polygon(
        CcBitmap* dst,
        CcCoord* points,
        int n,
        uint32_t color
        )
{
    CcRect rect = cc_rect_around_points(points, n);
    if (!cc_rect_intersect(rect, cc_bitmap_rect(dst), &rect)) return;

    int* intersections = malloc(sizeof(int) * n);

    for (int y = rect.y; y < rect.y + rect.h; ++y)
    {
        int k = 0;
        for (int i = 0; i < n; ++i)
        {
            CcCoord a = points[i];
            CcCoord b = points[(i + 1) % n];

            if (y < a.y && y < b.y) continue;
            if (y > a.y && y > b.y) continue;
            assert(a.y != b.y);

            int inv_m = (b.x - a.x) * 10000 / (b.y - a.y);
            int fy = y - a.y;
            int fx = (fy * inv_m) / 10000;

            intersections[k] = a.x + fx;
            ++k;
        }

        qsort(intersections, k, sizeof(int), int_compare_);
        
        uint32_t* data = dst->data + dst->w * y;

        int counter = 1;
        int start = intersections[0];
        for (int i = 1; i < k; ++i)
        {
            int end = intersections[i];

            if (end - start > 1)
            {
                if (counter % 2 == 1)
                {
                    for (int j = start; j <= end; ++j)
                        data[j] = color;
                }

                counter += 1;
            }

            start = end;
        }
    }

    free(intersections);
}

void cc_bitmap_stroke_rect(CcBitmap* b, int x1, int y1, int x2, int y2, int width, uint32_t color)
{
    cc_bitmap_interp_square(b, x1, y1, x2, y1, width, color);
    cc_bitmap_interp_square(b, x2, y1, x2, y2, width, color);
    cc_bitmap_interp_square(b, x2, y2, x1, y2, width, color);
    cc_bitmap_interp_square(b, x1, y2, x1, y1, width, color);
}

CcRect cc_bitmap_flood_fill(CcBitmap* b, int sx, int sy, uint32_t new_color)
{
    /* stack safe verison of:
    cc_bitmap_flood_fill_r(b, x - 1, y, old_color, new_color);
    cc_bitmap_flood_fill_r(b, x + 1, y, old_color, new_color);
    cc_bitmap_flood_fill_r(b, x, y - 1, old_color, new_color);
    cc_bitmap_flood_fill_r(b, x, y + 1, old_color, new_color);
    */

    int W = b->w;
    int H = b->h;

    assert(sx >= 0 && sy >= 0 && sx <= W && sy <= H);

    uint32_t old_color = b->data[sx + sy * W];

    if (old_color == new_color)
    {
        CcRect r = { 0, 0, 0, 0 };
        return r;
    }

    // every pixel will be visited at most once 
    CcCoord* queue = malloc(sizeof(CcCoord) * b->w * b->h);
    CcCoord* front = queue;
    CcCoord* back = front;

    back->x = sx;
    back->y = sy;    
    ++back;

    int min_x = sx;
    int max_x = sx;

    int min_y = sy;
    int max_y = sy;


    while (front != back)
    {
        int x = front->x;
        int y = front->y;

        int loc = x + y * W;

        extend_interval(x, &min_x, &max_x);
        extend_interval(y, &min_y, &max_y);


        if (0 <= x - 1 && b->data[loc - 1] == old_color)
        {
            back->x = x - 1;
            back->y = y;
            ++back;

            // must immediately indicate that this pixel has been visited,
            // to avoid double counting.
            b->data[loc - 1] = new_color;
        }

        if (x + 1 < W && b->data[loc + 1] == old_color)
        {
            back->x = x + 1;
            back->y = y;
            ++back;

            b->data[loc + 1] = new_color;
        }

        if (0 < y && b->data[loc - W] == old_color)
        {
            back->x = x;
            back->y = y - 1;
            ++back;

            b->data[loc - W] = new_color;
        }

        if (y + 1 < H && b->data[loc + W] == old_color)
        {
            back->x = x;
            back->y = y + 1;
            ++back;

            b->data[loc + W] = new_color;
        }

        ++front;
    }

    free(queue);

    return cc_rect_extrema(min_x, min_y, max_x, max_y);
}

void cc_bitmap_invert_colors(CcBitmap* bitmap)
{
    int comps[4];
    int N = bitmap->w * bitmap->h;

    for (int i = 0; i < N; ++i)
    {
        uint32_t color = bitmap->data[i];
        comps[3] = color_component(color, 3);
        for (int j = 0; j < 3; ++j)
            comps[j] = 255 - color_component(color, j);
        bitmap->data[i] = color_pack(comps);
    }
}

void cc_bitmap_rotate_90(const CcBitmap* src, CcBitmap* dst)
{
    for (int y = 0; y < src->h; ++y)
    {
        for (int x = 0; x < src->w; ++x)
        {
            dst->data[(src->h - (y + 1)) + x * src->h] = src->data[x + y * src->w];
        }
    }
}

void cc_bitmap_flip_horiz(const CcBitmap* src, CcBitmap* dst)
{
    for (int y = 0; y < src->h; ++y)
    {
        for (int x = 0; x < src->w; ++x)
        {
            int flip_x = src->w - (x + 1);
            dst->data[flip_x + y * src->w] = src->data[x + y * src->w];
        }
    }
}

void cc_bitmap_flip_vert(const CcBitmap* src, CcBitmap* dst)
{
    for (int y = 0; y < src->h; ++y)
    {
        for (int x = 0; x < src->w; ++x)
        {
            int flip_y = src->h - (y + 1);
            dst->data[x + flip_y * src->w] = src->data[x + y * src->w];
        }
    }
}

void cc_bitmap_zoom(const CcBitmap* src, CcBitmap* dst, int zoom)
{
    // we have enough src material
    assert(dst->w <= src->w * zoom);
    assert(dst->h <= src->h * zoom);

    int x = 1;
    int i;
    for (i = 0; i < 16; ++i)
    {
        if (x == zoom) break;
        x *= 2;
    }

    if (i == 16)
    {
        cc_bitmap_zoom_general(src, dst, zoom);
    }
    else
    {
        cc_bitmap_zoom_power_of_2(src, dst, i);
    }
}

void cc_bitmap_zoom_general(const CcBitmap* src, CcBitmap* dst, int zoom)
{
    for (int y = 0; y < dst->h; ++y)
    {
        for (int x = 0; x < dst->w; ++x)
        {
            int src_x = x / zoom;
            int src_y = y / zoom;
            dst->data[x + y * dst->w] = src->data[src_x + src_y * src->w];
        }
    }
}

void cc_bitmap_zoom_power_of_2(const CcBitmap* src, CcBitmap* dst, int zoom_power)
{
    for (int y = 0; y < dst->h; ++y)
    {
        for (int x = 0; x < dst->w; ++x)
        {
            int src_x = x >> zoom_power;
            int src_y = y >> zoom_power;
            dst->data[x + y * dst->w] = src->data[src_x + src_y * src->w];
        }
    }
}

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

