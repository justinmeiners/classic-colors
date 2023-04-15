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

#include <ctype.h>
#include <assert.h>
#include "polygon.h"

typedef struct
{
    int before;
    int after;
} CriticalValue; 

void cc_polygon_init(CcPolygon* p)
{
    p->count = 0;
    p->capacity = 0;
    p->points = NULL;
}

void cc_polygon_shutdown(CcPolygon* p)
{
    cc_polygon_clear(p);
}

void cc_polygon_add(CcPolygon* p, CcCoord x)
{   
    if (p->count + 1 >= p->capacity)
    {
        int n = MAX(16, p->capacity * 2);
        p->points = realloc(p->points, sizeof(CcCoord) * n);
        p->capacity = n;
    }
    p->points[p->count] = x;
    ++p->count;
}

void cc_polygon_clear(CcPolygon* p)
{
    if (p->capacity > 0)
    {
        free(p->points);
        p->points = NULL;
        p->capacity = 0;
    }
    p->count = 0;
}

void cc_polygon_update_last(CcPolygon* p, CcCoord x, int align)
{
    int k = p->count - 1;
    if (align)
    {
        assert(p->count >= 2);
        cc_line_align_to_45(p->points[k - 1].x, p->points[k - 1].y, x.x, x.y, &x.x, &x.y);
    }
    else
    {
        assert(p->count >= 1);
    }
    p->points[k] = x;
}

CcRect cc_polygon_rect(const CcPolygon* p)
{
    return cc_rect_around_points(p->points, p->count);
}

void cc_polygon_shift(CcPolygon* p, CcCoord shift)
{
    for (int i = 0; i < p->count; ++i)
    {
        p->points[i].x += shift.x;
        p->points[i].y += shift.y;
    }
}

void cc_polygon_remove_duplicates_open(CcPolygon* p)
{
    if (p->count <= 0) return;

    int count = 1;
    for (int i = 1; i < p->count; ++i)
    {
        if (p->points[i - 1].x == p->points[i].x &&
            p->points[i - 1].y == p->points[i].y) {
            // skip over
        } else {
            p->points[count] = p->points[i];
            ++count;
        }
    }
    p->count = count;
}

void cc_polygon_remove_duplicates_closed(CcPolygon* p)
{
    if (p->count <= 0) return;
    cc_polygon_remove_duplicates_open(p);

    if (p->count > 1)
    {
        int n = p->count;
        if (p->points[n - 1].x == p->points[0].x && p->points[n - 1].y == p->points[0].y) --n;
        p->count = n;
    }
}

void cc_polygon_cleanup(CcPolygon* p, int closed)
{
    if (closed)
    {
        cc_polygon_remove_duplicates_closed(p);
    }
    else
    {
        cc_polygon_remove_duplicates_open(p);
    }
}

void cc_bitmap_stroke_polygon(
        CcBitmap* dst,
        const CcCoord* points,
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

static
void polygon_critical_values_x_(const CcCoord* points, int n, CriticalValue* out)
{
    assert(n >= 3);
    int i = 0;
    out[i].before = points[n - 1].x - points[i].x;
    out[i].after = points[i + 1].x - points[i].x;
    ++i;

    while (i < (n - 1))
    {
        out[i].before = points[i - 1].x - points[i].x;
        out[i].after = points[i + 1].x - points[i].x;
        ++i;
    }
    out[i].before = points[i - 1].x - points[i].x;
    out[i].after = points[0].x - points[i].x;
    ++i;
}

static
void polygon_critical_values_y_(const CcCoord* points, int n, CriticalValue* out)
{
    assert(n >= 3);
    int i = 0;
    out[i].before = points[n - 1].y - points[i].y;
    out[i].after = points[i + 1].y - points[i].y;
    ++i;

    while (i < (n - 1))
    {
        out[i].before = points[i - 1].y - points[i].y;
        out[i].after = points[i + 1].y - points[i].y;
        ++i;
    }
    out[i].before = points[i - 1].y - points[i].y;
    out[i].after = points[0].y - points[i].y;
    ++i;
}

int remove_axis_colinear_points_(CcCoord* points, CriticalValue* dirs, int n)
{
    int count = 0;
    for (int i = 0; i < n; ++i)
    {
        if (dirs[i].before == 0 && dirs[i].after == 0)
        {
            // ignore point
        }
        else
        {
            points[count] = points[i];
            dirs[count] = dirs[i];
            ++count;
        }
    }
    return count;
}

// requires: polygon is closed
//           n >= 3
//           polygon is not colinear
//           no duplicate polygon points 
static
int scanline_crossings_(const CcCoord* points, const CriticalValue* y_dirs, int n, int scan_y, int* out_x)
{
    // The key insight is to look at the perspective of the entire polygon.
    // What are all the places this polygon passes the given scan line?
    // https://stackoverflow.com/a/35551300/425756
    assert(n >= 3);
    int crossings = 0;

    for (int i = 0; i < n; ++i)
    {
        CcCoord start = points[i];
        if (start.y == scan_y)
        {
            CriticalValue dir = y_dirs[i];
            // special case: vertex on scanline.

            if (dir.before == 0)
            {
                // end of a horizontal scan line
                // ignore.
                // it will be handled when we get around the loop.
            }
            else if (dir.after == 0)
            {
                // start of a horizontal line
                CriticalValue next_dir = y_dirs[(i + 1) % n];
                assert(next_dir.after != 0);

                if (next_dir.after * dir.before < 0)
                {
                    out_x[crossings] = start.x;
                    ++crossings;
                }
            }
            else if (dir.before * dir.after < 0)
            {
                // signs of y differ. This is a crossing at a vertex.
                out_x[crossings] = start.x;
                ++crossings;
            }
            // signs of y same. This is an extrema, not a crossing.
        }
        else
        {
            // normal case: find the intersection with the line.
            CcCoord end = points[(i + 1) % n];
            if ((start.y < scan_y && scan_y < end.y) 
               || (end.y < scan_y && scan_y < start.y))
            {
                assert(end.y != start.y);
                double inv_m = (double)(end.x - start.x) / (double)(end.y - start.y);
                double fy = (double)(scan_y - start.y);
                double fx = (fy * inv_m);

                out_x[crossings] = (int)round((double)start.x + fx);
                ++crossings;
            }
        }
    }
    return crossings;
}

static
int crossing_compare_(const void *ap, const void *bp)
{
    int a = *((int*)ap);
    int b = *((int*)bp);
    return a - b;
}

// requires:
//      polygon does not contain trivially duplicate points
static
void fill_polygon_(
        CcBitmap* dst,
        const CcCoord* points,
        const CriticalValue* y_dirs,
        int n,
        uint32_t color
        )
{
    // Scan line algorithm for arbitrary polygons.
    // Overview: https://web.cs.ucdavis.edu/~ma/ECS175_S00/Notes/0411_b.pdf
    // It's not the fastests thing in the world, but should be reuse.
    assert(n >= 3);

    CcRect rect = cc_rect_around_points(points, n);
    if (!cc_rect_intersect(rect, cc_bitmap_rect(dst), &rect)) return;

    int* crossings = malloc(sizeof(int) * n);
    for (int y = rect.y; y < rect.y + rect.h; ++y)
    {
        int crossing_count = scanline_crossings_(points, y_dirs, n, y, crossings);
        qsort(crossings, crossing_count, sizeof(int), crossing_compare_);

        /* DEBUG
        for (int i = 0; i < crossing_count; ++i)
        {
            printf("%d ", crossings[i]);
        }
        printf("\n");
        */

        uint32_t* row_data = dst->data + dst->w * y;

        int i = 0;
        while (i + 1 < crossing_count)
        {
            int start_x = interval_clamp(crossings[i], rect.x, rect.x + rect.w);
            int end_x = interval_clamp(crossings[i + 1], rect.x, rect.x + rect.w);

            for (int j = start_x; j < end_x; ++j) row_data[j] = color;
            i += 2;
        }
    }

    free(crossings);
}

void cc_bitmap_fill_polygon_inplace(
        CcBitmap* dst,
        CcCoord* points,
        int n,
        uint32_t color
        )
{

    if (n < 3) return;

    CriticalValue* dirs = malloc(sizeof(CriticalValue) * n);
    polygon_critical_values_x_(points, n, dirs);
    n = remove_axis_colinear_points_(points, dirs, n);

    if (n >= 3) {
      // must do y after x so the y critical values are valid for drawing.
      polygon_critical_values_y_(points, n, dirs);
      n = remove_axis_colinear_points_(points, dirs, n);

      if (n >= 3) {
        fill_polygon_(dst, points, dirs, n, color);
      }
    }

    free(dirs);
}

void cc_bitmap_fill_polygon(
        CcBitmap* dst,
        const CcCoord* points,
        int n,
        uint32_t color
        )
{
    CcCoord* copy = malloc(sizeof(CcCoord) * n);
    memcpy(copy, points, sizeof(CcCoord) * n);
    cc_bitmap_fill_polygon_inplace(dst, copy, n, color);
    free(copy);
}


