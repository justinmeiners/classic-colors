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

void cc_gray_bitmap_alloc(CcGrayBitmap* b)
{
    assert(b->w >= 0);
    assert(b->h >= 0);

    b->stride = ALIGN_UP(sizeof(char) * b->w, 8);

    assert(b->stride >= b->w);
    b->data = malloc(b->h * b->stride);
}

void cc_gray_bitmap_free(CcGrayBitmap *b)
{
    free(b->data);
    b->data = NULL;
}

static
void fill_(char *bytes, CcDim w, CcDim h, size_t stride, CcPixel x)
{
    while (h) {
        CcPixel *data = (CcPixel *)bytes;
        for (size_t col = 0; col < w; ++col) {
            data[col] = x;
        }
        bytes += stride;
        --h;
    }
}

void cc_bitmap_clear(CcBitmap* b, CcPixel color)
{
    size_t N = b->w * b->h;

    switch (color)
    {
    case 0:
        memset(b->data, 0, sizeof(CcPixel) * N);
        break;
    case 0xFFFFFFFF:
        memset(b->data, 0xFF, sizeof(CcPixel) * N);
        break;
    default:
        fill_((char *)b->data, b->w, b->h, b->w * sizeof(CcPixel), color);
        break;
    }
}

void cc_bitmap_alloc(CcBitmap* b)
{
    assert(b->w >= 0);
    assert(b->h >= 0);
    b->data = malloc(b->w * b->h * sizeof(CcPixel));
}

void cc_bitmap_free(CcBitmap* b)
{
    free(b->data);
    b->data = NULL;
}

void cc_bitmap_copy(const CcBitmap *src, CcBitmap *dst)
{
    assert(src->w == dst->w);
    assert(src->h == dst->h);
    cc_bitmap_blit_unsafe(src, dst, 0, 0, 0, 0, src->w, src->h, COLOR_BLEND_REPLACE);
}

static inline
void interleave_channel_(
        const char * restrict src,
        char * restrict dst,
        size_t src_stride,
        size_t dst_stride,
        int w,
        int h
        ) {
    while (h)
    {
        for (size_t col = 0; col < w; ++col) {
            dst[col * sizeof(CcPixel)] = src[col];
        }
        src += src_stride;
        dst += dst_stride;
        --h;
    }
}

void cc_bitmap_copy_channel(CcBitmap* b, size_t channel_index, const CcGrayBitmap *channel)
{
    char *buffer = (char *)b->data;
    interleave_channel_(channel->data, buffer + channel_index, channel->stride, sizeof(CcPixel) * b->w, b->w, b->h);
}

void cc_bitmap_replace(CcBitmap* b, CcPixel old_color, CcPixel new_color)
{
    CcPixel *data = b->data;

    size_t n = b->w * b->h;
    for (size_t i = 0; i < n; ++i)
    {
        if (data[i] == old_color)
            data[i] = new_color;
    }
}

void cc_bitmap_swap_channels(CcBitmap *b)
{
    CcPixel *data = b->data;

    size_t n = b->w * b->h;
    for (size_t i = 0; i < n; ++i)
    {
        data[i] = cc_color_swap(data[i]);
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

    if (src == dst) {
        CcRect src_rect = {
            src_x, src_y, w, h
        };
        CcRect temp;
        assert(!cc_rect_intersect(src_rect, dst_rect, &temp));
    }

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

static inline
void blit_copy_(
        const char * restrict src,
        char * restrict dst,
        size_t src_stride,
        size_t dst_stride,
        int w,
        int h
        ) {
    while (h)
    {
        memcpy(dst, src, w * sizeof(CcPixel));

        src += src_stride;
        dst += dst_stride;
        --h;
    }
}

static inline
void blit_blend_(
        const char * restrict src,
        char * restrict dst,
        size_t src_stride,
        size_t dst_stride,
        int w,
        int h,
        CcPixel (*blend)(CcPixel, CcPixel)
        ) {

    while (h)
    {
        const CcPixel *src_data = (CcPixel *)src;
        CcPixel *dst_data = (CcPixel *)dst;

        for (int x = 0; x < w; ++x)
        {
            dst_data[x] = blend(src_data[x], dst_data[x]);
        }
        src += src_stride;
        dst += dst_stride;
        --h;
    }
}

void cc_bitmap_blit_unsafe(
        const CcBitmap *src,
        CcBitmap *dst,
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

    char *src_bytes = cc_bitmap_bytes_at(src, src_x, src_y);
    char *dst_bytes = cc_bitmap_bytes_at(dst, dst_x, dst_y);
    size_t src_stride = src->w * sizeof(CcPixel);
    size_t dst_stride = dst->w * sizeof(CcPixel);

    switch (blend)
    {
        case COLOR_BLEND_REPLACE:
            blit_copy_(src_bytes, dst_bytes, src_stride, dst_stride, w, h);
            break;
        case COLOR_BLEND_OVERLAY:
            blit_blend_(src_bytes, dst_bytes, src_stride, dst_stride, w, h, cc_color_blend_overlay);
            break;
        case COLOR_BLEND_FULL:
            blit_blend_(src_bytes, dst_bytes, src_stride, dst_stride, w, h, cc_color_blend_full);
            break;
        case COLOR_BLEND_INVERT:
            blit_blend_(src_bytes, dst_bytes, src_stride, dst_stride, w, h, cc_color_blend_invert);
            break;
        case COLOR_BLEND_MULTIPLY:
            blit_blend_(src_bytes, dst_bytes, src_stride, dst_stride, w, h, cc_color_blend_multiply);
            break;
    }
}

static inline
void interp_linear_(
        CcBitmap* b,
        int x1,
        int y1,
        int x2,
        int y2,
        int width,
        CcPixel color,
        void (*draw_part)(CcBitmap* b, int x, int y, int w, CcPixel c, void* ctx),
        void* ctx
        ) {
    int m, x, y, sx, ex, sy, ey, index;
    if (x1 == x2 && y1 == y2)
    {
        x = x1; y = y1;
        draw_part(b, x, y, width, color, ctx);
        return;
    }
    if (abs(x2 - x1) > abs(y2 - y1))
    {
        if (x1 > x2)
        {
            sx = x2; ex = x1;
            sy = y2; ey = y1;
        }
        else
        {
            sx = x1; ex = x2;
            sy = y1; ey = y2;
        }
        int delta_x = ex - sx;
        int delta_y = ey - sy;
        int dy = delta_y > 0 ? 1 : -1;
        delta_y *= dy;
        int D = 2 * delta_y - delta_x;
        y = sy;
        for (x = sx; x <= ex; ++x)
        {
            draw_part(b, x, y, width, color, ctx);
            if (D > 0)
            {
                y += dy;
                D = D + 2 * (delta_y - delta_x);
            }
            else
            {
                D += 2 * delta_y;
            }
        } 
    }
    else
    {
        if (y1 > y2)
        {
            sx = x2; ex = x1;
            sy = y2; ey = y1;
        }
        else
        {
            sx = x1; ex = x2;
            sy = y1; ey = y2;
        }
        int delta_x = ex - sx;
        int delta_y = ey - sy;

        int dx = delta_x > 0 ? 1 : -1;
        delta_x *= dx;

        int D = 2 * delta_x - delta_y;
        x = sx;

        for (y = sy; y <= ey; ++y)
        {
            draw_part(b, x, y, width, color, ctx);
            if (D > 0)
            {
                x += dx;
                D = D + 2 * (delta_x - delta_y);
            }
            else
            {
                D += 2 * delta_x;
            }
        }
    }
}

void cc_bitmap_draw_spray(CcBitmap* b, int cx, int cy, int r, int density, CcPixel color)
{
    CcRect rect = {
        cx - r, cy - r, 2 * r, 2 * r
    };
    cc_rect_intersect(rect, cc_bitmap_rect(b), &rect);

    CcPixel *data = b->data;

    for (int y = rect.y; y < rect.y + rect.h; ++y)
    {
        for (int x = rect.x; x < rect.x + rect.w; ++x)
        {
            int64_t d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (d < r * r)
            {
                if (rand() % 1000 < density * 10)
                {
                    data[x + y * b->w] = color;
                }
            }
        }
    }
}

void cc_bitmap_draw_circle(CcBitmap* b, int cx, int cy, int r, CcPixel color)
{
    CcRect rect = {
        cx - r, cy - r, 2 * r, 2 * r
    };
    cc_rect_intersect(rect, cc_bitmap_rect(b), &rect);

    CcPixel *data = b->data;
    for (int y = rect.y; y < rect.y + rect.h; ++y)
    {
        for (int x = rect.x; x < rect.x + rect.w; ++x)
        {
            int64_t d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (d < r * r)
            {
                data[x + y * b->w] = color;
            }
        }
    }
}

void cc_bitmap_draw_square(CcBitmap* b, int cx, int cy, int d, CcPixel color)
{
    CcRect rect = {
        cx - d / 2, cy - d / 2, d, d
    };
    cc_rect_intersect(rect, cc_bitmap_rect(b), &rect);

    CcPixel *data = b->data;
    for (int y = 0; y < rect.h; ++y)
    {
        for (int x = 0; x < rect.w; ++x)
        {
            data[(rect.x + x) + (rect.y + y) * b->w] = color;
        }
    }
}

static inline
void interp_square_helper_(CcBitmap* b, int x, int y, int d, CcPixel c, void* ctx) {
    cc_bitmap_draw_square(b, x, y, d, c);
}
   
void cc_bitmap_interp_square(CcBitmap* b, int x1, int y1, int x2, int y2, int width, CcPixel color)
{
    interp_linear_(b, x1, y1, x2, y2, width, color, interp_square_helper_, NULL);
}


static inline
void interp_circle_helper_(CcBitmap* b, int x, int y, int w, CcPixel c, void* ctx) {
    cc_bitmap_draw_circle(b, x, y, w, c);
}
 
void cc_bitmap_interp_circle(CcBitmap* b, int x1, int y1, int x2, int y2, int radius, CcPixel color)
{
    interp_linear_(b, x1, y1, x2, y2, radius, color, interp_circle_helper_, NULL);
}

static inline
void interp_dotted_horiz_(CcBitmap* b, int x, int y, int r, CcPixel color, void* ctx) {
    if (x % 8 < 4) cc_bitmap_set(b, x, y, color);
}

static inline
void interp_dotted_vert_(CcBitmap* b, int x, int y, int r, CcPixel color, void* ctx) {
    if (y % 8 < 4) cc_bitmap_set(b, x, y, color);
}

void cc_bitmap_interp_dotted(CcBitmap* b, int x1, int y1, int x2, int y2, CcPixel color)
{
    if (abs(x2 - x1) > abs(y2 - y1))
    {
        interp_linear_(b, x1, y1, x2, y2, 0, color, interp_dotted_horiz_, NULL);
    }
    else
    {
        interp_linear_(b, x1, y1, x2, y2, 0, color, interp_dotted_vert_, NULL);
    }
}

void cc_bitmap_dotted_rect(CcBitmap* b, int x1, int y1, int x2, int y2, CcPixel color)
{
    cc_bitmap_interp_dotted(b, x1, y1, x2, y1, color);
    cc_bitmap_interp_dotted(b, x2, y1, x2, y2, color);
    cc_bitmap_interp_dotted(b, x2, y2, x1, y2, color);
    cc_bitmap_interp_dotted(b, x1, y2, x1, y1, color);
}

void cc_bitmap_fill_rect(CcBitmap* dst, int x1, int y1, int x2, int y2, CcPixel color)
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

    CcPixel *data = dst->data;

    for (int y = 0; y < to_fill.h; ++y)
    {
        for (int x = 0; x < to_fill.w; ++x)
        {
            size_t dst_index = (to_fill.x + x) + (to_fill.y + y) * dst->w;
            data[dst_index] = color;
        }
    }
}

void cc_bitmap_stroke_ellipse(CcBitmap* dst, int x0, int y0, int x1, int y1, CcPixel color)
{
    // I tried inventing my own implementation.. but it sucked.
    // http://members.chello.at/~easyfilter/bresenham.html
    //
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

void cc_bitmap_fill_ellipse(CcBitmap* dst, int x1, int y1, int x2, int y2, CcPixel color)
{
    CcRect rect = cc_rect_around_corners(x1, y1, x2, y2);
    if (rect.w < 2 || rect.h < 2) return;

    double a = 0.5 * (double)rect.w;
    double b = 0.5 * (double)rect.h;

    double cx = (double)rect.x + a;
    double cy = (double)rect.y + b;

    if (!cc_rect_intersect(rect, cc_bitmap_rect(dst), &rect)) return;

    CcPixel *data = dst->data;

    for (int row = 0; row < rect.h; ++row)
    {
        double ey = (double)(rect.y + row) + 0.5 - cy;

        double a2 = a * a;
        double ey2 = ey * ey;
        double b2 = b * b;

        double D = a2 * (1.0 - (ey2 / b2));
        
        if (D >= 0.0)
        {
            int l = round(sqrt(D) - 0.8);
            int l2 = round(sqrt(D) - 0.2);

            int startx = interval_clamp(cx - l, rect.x, rect.x + rect.w);
            int endx = interval_clamp(cx + l2, rect.x, rect.x + rect.w);

            int y = row + rect.y; 
            for (int x = startx; x < endx; ++x)
                data[x + y * dst->w] = color;
        }
    }
}

void cc_bitmap_stroke_rect(CcBitmap* b, int x1, int y1, int x2, int y2, int width, CcPixel color)
{
    cc_bitmap_interp_square(b, x1, y1, x2, y1, width, color);
    cc_bitmap_interp_square(b, x2, y1, x2, y2, width, color);
    cc_bitmap_interp_square(b, x2, y2, x1, y2, width, color);
    cc_bitmap_interp_square(b, x1, y2, x1, y1, width, color);
}

CcRect cc_bitmap_flood_fill(CcBitmap* b, int sx, int sy, CcPixel new_color)
{
    /* stack safe verison of:
    cc_bitmap_flood_fill_r(b, x - 1, y, old_color, new_color);
    cc_bitmap_flood_fill_r(b, x + 1, y, old_color, new_color);
    cc_bitmap_flood_fill_r(b, x, y - 1, old_color, new_color);
    cc_bitmap_flood_fill_r(b, x, y + 1, old_color, new_color);
    */

    CcPixel *data = b->data;
    int W = b->w;
    int H = b->h;

    if (sx < 0 || sy < 0 || sx >= W || sy >= H)
    {
        return (CcRect) { 0, 0, 0, 0 };
    }

    CcPixel old_color = data[sx + sy * W];
    if (old_color == new_color)
    {
        return (CcRect) { 0, 0, 0, 0 };
    }

    data[sx + sy * W] = new_color;

    // every pixel will be visited at most once
    CcCoord* queue = malloc(sizeof(CcCoord) * W * H);
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

        if (0 <= x - 1 && data[loc - 1] == old_color)
        {
            // immediately indicate that this pixel has been visited, to avoid double counting.
            data[loc - 1] = new_color;
            back->x = x - 1;
            back->y = y;
            ++back;
        }

        if (x + 1 < W && data[loc + 1] == old_color)
        {
            data[loc + 1] = new_color;
            back->x = x + 1;
            back->y = y;
            ++back;
        }

        if (0 <= y - 1 && data[loc - W] == old_color)
        {
            data[loc - W] = new_color;
            back->x = x;
            back->y = y - 1;
            ++back;
        }

        if (y + 1 < H && data[loc + W] == old_color)
        {
            data[loc + W] = new_color;
            back->x = x;
            back->y = y + 1;
            ++back;
        }

        ++front;
    }

    free(queue);

    return cc_rect_from_extrema(min_x, min_y, max_x, max_y);
}

void cc_bitmap_invert_colors(CcBitmap* b)
{
    CcPixel *data = b->data;

    size_t N = b->w * b->h;

    for (size_t i = 0; i < N; ++i)
    {
        data[i] = cc_color_blend_invert(COLOR_WHITE, data[i]);
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
    const CcPixel * restrict in = src->data;
    CcPixel * restrict out = dst->data;

    for (int y = 0; y < src->h; ++y)
    {
        for (int x = 0; x < src->w; ++x)
        {
            int flip_x = src->w - (x + 1);
            out[flip_x + y * src->w] = in[x + y * src->w];
        }
    }
}

void cc_bitmap_flip_vert(const CcBitmap* src, CcBitmap* dst)
{
    const CcPixel * restrict in = src->data;
    CcPixel * restrict out = dst->data;

    for (int y = 0; y < src->h; ++y)
    {
        for (int x = 0; x < src->w; ++x)
        {
            int flip_y = src->h - (y + 1);
            out[x + flip_y * src->w] = in[x + y * src->w];
        }
    }
}

void cc_bitmap_zoom(const CcBitmap* src, CcBitmap* dst, int zoom)
{
    // we have enough src material
    assert(dst->w <= src->w * zoom);
    assert(dst->h <= src->h * zoom);
    assert(src != dst);

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
    const CcPixel * restrict in = src->data;
    CcPixel * restrict out = dst->data;

    for (int y = 0; y < dst->h; ++y)
    {
        for (int x = 0; x < dst->w; ++x)
        {
            int src_x = x / zoom;
            int src_y = y / zoom;
            *out = in[src_x + src_y * src->w];
            ++out;
        }
    }
}

void cc_bitmap_zoom_power_of_2(const CcBitmap* src, CcBitmap* dst, int zoom_power)
{
    const CcPixel *restrict in = src->data;
    CcPixel *restrict out = dst->data;

    for (int y = 0; y < dst->h; ++y)
    {
        for (int x = 0; x < dst->w; ++x)
        {
            int src_x = x >> zoom_power;
            int src_y = y >> zoom_power;
            *out = in[src_x + src_y * src->w];
            ++out;
        }
    }
}

