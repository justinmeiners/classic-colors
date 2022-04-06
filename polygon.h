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


// - points will be modified.
// - recommend calling cc_polygon_cleanup first
void cc_bitmap_fill_polygon_inplace(
        CcBitmap* dst,
        CcCoord* points,
        int n,
        uint32_t color
        );

void cc_bitmap_fill_polygon(
        CcBitmap* dst,
        const CcCoord* points,
        int n,
        uint32_t color
        );

#endif
