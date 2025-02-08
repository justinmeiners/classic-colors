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

#include "plane.h"

CcRect cc_rect_around_points(const CcCoord* points, int n)
{
    assert(n > 0);
    int min_x = points[0].x;
    int min_y = points[0].y;
    int max_x = min_x;
    int max_y = min_y;

    for (int i = 1; i < n; ++i)
    {
        extend_interval(points[i].x, &min_x, &max_x);
        extend_interval(points[i].y, &min_y, &max_y);
    }
    return cc_rect_from_extrema(min_x, min_y, max_x, max_y);
}

CcRect cc_rect_pad(CcRect r, int pad_w, int pad_h)
{
    r.x -= pad_w;
    r.y -= pad_h;

    r.w += 2 * pad_w;
    r.h += 2 * pad_h;
    return r;
}

void cc_line_align_to_45(int start_x, int start_y, int end_x, int end_y, int* out_x, int* out_y)
{
    int dx = (end_x - start_x);
    int dy = (end_y - start_y);

    int best_x = 0;
    int best_y = 0;
    int best = 0;

    for (int i = -1; i <= 1; ++i)
    {
        for (int j = -1; j <= 1; ++j)
        {
            if (i == 0 && j == 0) { continue; }

            int dot = i * dx + j * dy;

            if (i != 0 && j != 0)
            {
                // sqrt(2) ~= 1.41
                dot = (dot * 100) / 141;
            }

            if (dot > best)
            {
                best_x = i;
                best_y = j;
                best = dot;
            }
        }
    }

    if (best_x != 0 && best_y != 0)
    {
        // one more time
        best = (best * 100) / 141;
    }
    *out_x = start_x + best_x * best;
    *out_y = start_y + best_y * best;
}

void align_rect_to_square(int start_x, int start_y, int end_x, int end_y, int* out_x, int* out_y)
{
    int dx = (end_x - start_x);
    int dy = (end_y - start_y);

    int side_length = MIN(abs(dx), abs(dy));

    *out_x = start_x + sign_of_int(dx) * side_length;
    *out_y = start_y + sign_of_int(dy) * side_length;
}
