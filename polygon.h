#ifndef POLYGON_H
#define POLYGON_H

#include "bitmap.h"

typedef struct
{
    CcCoord* points;
    int count;
    int capacity;
} CcPolygon;

void cc_polygon_init(CcPolygon* p);
void cc_polygon_shutdown(CcPolygon* p);

void cc_polygon_add(CcPolygon* p, CcCoord x);
void cc_polygon_clear(CcPolygon* p);

void cc_polygon_update_last(CcPolygon* p, CcCoord x, int align);

CcRect cc_polygon_rect(const CcPolygon* p);
void cc_polygon_shift(CcPolygon* p, CcCoord shift);

void cc_polygon_remove_duplicates_open(CcPolygon* p);
void cc_polygon_remove_duplicates_closed(CcPolygon* p);
void cc_polygon_cleanup(CcPolygon* p, int closed);

void cc_bitmap_stroke_polygon(
        CcBitmap* dst,
        const CcCoord* points,
        int n,
        int closed,
        int width,
        uint32_t color
        );


// requires:
//      polygon does not contain trivially duplicate points
//      (recommend calling cc_polygon_cleanup).
void cc_bitmap_fill_polygon(
        CcBitmap* dst,
        const CcCoord* points,
        int n,
        uint32_t color
        );


#endif
