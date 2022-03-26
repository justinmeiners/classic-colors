#include <ctype.h>
#include <assert.h>
#include "polygon.h"

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
    int k = p->count;

    if (k + 1 > p->capacity)
    {
        int n = MAX(16, p->capacity * 2);
        p->points = realloc(p->points, sizeof(CcCoord) * n);
        p->capacity = n;
    }
    p->points[k] = x;
    ++p->count;
}

void cc_polygon_clear(CcPolygon* p)
{
    if (p->capacity > 0)
    {
        free(p->points);
        p->capacity = 0;
        p->count = 0;
    }
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

CcRect cc_polygon_bounds(const CcPolygon* p)
{
    return cc_rect_around_points(p->points, p->count);
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

static int polygon_point_classify_extrema_(CcCoord before, CcCoord middle, CcCoord after)
{
    if (before.y <= middle.y && after.y <= middle.y)
    {
        return 1;
    }
    else if (before.y >= middle.y && after.y >= middle.y)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}
static void polygon_classify_points_(const CcCoord* points, int n, int* classification)
{
    CcCoord before = points[n - 1];
    CcCoord middle = points[0];
    CcCoord after = points[1];
    classification[0] = polygon_point_classify_extrema_(before, middle, after);

    for (int i = 1; i < n; ++i)
    {
        before = points[i - 1];
        middle = points[i];
        after = points[(i + 1) % n];
        classification[i] = polygon_point_classify_extrema_(before, middle, after);
    }
}

static int intersect_scanline_line_(CcCoord a, CcCoord b, int y, int *out_x)
{
    if ( (y < a.y && y < b.y) ||
         (y > a.y && y > b.y) ) return 0;

    if (y == a.y)
    {
        *out_x = a.x;
        return 1;
    }
    else if (y == b.y)
    {
        *out_x = b.x;
        return 1;
    }

    int inv_m = (b.x - a.x) * 10000 / (b.y - a.y);
    int fy = y - a.y;
    int fx = (fy * inv_m) / 10000;

    *out_x = a.x + fx;
    return 1;
}

typedef struct
{
    int x;
    int duplicate;
} ScanLineHit;

static int hit_compare_(const void *ap, const void *bp)
{
    const ScanLineHit* a = ap;
    const ScanLineHit* b = bp;
    return a->x - b->x;
}

static int remove_duplicate_hits_(ScanLineHit* hits, int k)
{
    if (k == 5 || k == 3)
    {
        for (int m = 0; m < k; ++m) printf("%d %d\n", hits[m].x, hits[m].duplicate);
    }

    int i = 0;
    int j = 0;

    while (i != k)
    {
        hits[j] = hits[i];
        ++j;

        if (i + 1 < k && hits[i].x == hits[i + 1].x)
        {
            if (hits[i].duplicate)
            {
                hits[j] = hits[i];
                ++j;
            }
            ++i;
        }
        ++i;
    }
    return j;
}

// https://web.cs.ucdavis.edu/~ma/ECS175_S00/Notes/0411_b.pdf
void cc_bitmap_fill_polygon(
        CcBitmap* dst,
        const CcCoord* points,
        int n,
        uint32_t color
        )
{
    CcRect rect = cc_rect_around_points(points, n);
    if (!cc_rect_intersect(rect, cc_bitmap_rect(dst), &rect)) return;

    int* classification = malloc(sizeof(int) * n);
    polygon_classify_points_(points, n, classification);

    ScanLineHit* hits = malloc(sizeof(ScanLineHit) * n);
    for (int y = rect.y; y < rect.y + rect.h; ++y)
    {
        int k = 0;
        for (int i = 0; i < n; ++i)
        {
            CcCoord start = points[i];
            CcCoord end = points[(i + 1) % n];

            int x;
            if (!intersect_scanline_line_(start, end, y, &x)) continue;

            if (y == start.y && x == start.x)
            {
                hits[k].duplicate = (classification[i] == 0) ? 0 : 1;
            }
            else if (y == end.y && x == end.x)
            {
                hits[k].duplicate = (classification[(i + 1) % n] == 0) ? 0 : 1;
            }
            else
            {
                hits[k].duplicate = 1;
            }

            hits[k].x = x;
            ++k;
        }
        printf("before %d\n", k);
        qsort(hits, k, sizeof(ScanLineHit), hit_compare_);
        k = remove_duplicate_hits_(hits, k);
        printf("after %d\n", k);

        uint32_t* data = dst->data + dst->w * y;

        int i = 0;
        while (i + 1 < k)
        {
            int start = interval_clamp(hits[i].x, rect.x, rect.x + rect.w);
            int end = interval_clamp(hits[i + 1].x, rect.x, rect.x + rect.w);

            for (int j = start; j <= end; ++j) data[j] = color;
            i += 2;
        }
    }

    free(classification);
    free(hits);
}

