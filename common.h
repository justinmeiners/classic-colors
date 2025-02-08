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


#ifndef COMMON_H_
#define COMMON_H_

#include "config.h"
#include <memory.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define SWAP(x_, y_, T) do { T SWAP = x_; x_ = y_; y_ = SWAP; } while (0)

static inline
int sign_of_int(int x)
{
    return (x > 0) - (x < 0);
}

// Integer square root (using binary search)
// https://en.wikipedia.org/wiki/Integer_square_root
static inline
unsigned int isqrt(unsigned int y)
{
 	unsigned int L = 0;
	unsigned int M;
	unsigned int R = y + 1;

    while( L != R - 1 )
    {
        M = (L + R) / 2;

		if( M * M <= y )
			L = M;
		else
			R = M;
	}

    return L;
}

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

#define ALIGN_DOWN(x, alignment) ((x) & (~(alignment - 1)))
#define ALIGN_UP(x, alignment) ALIGN_DOWN(x + (alignment - 1), alignment)

#endif // COMMON_H_
