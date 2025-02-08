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

#ifndef CC_BITMAP_TEXT_H
#define CC_BITMAP_TEXT_H

#include "bitmap.h"
#include "stb_truetype.h" 

typedef enum
{
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_RIGHT
} CcTextAlign;

void cc_text_render(
        CcBitmap* bitmap,
        const wchar_t* text,
        const stbtt_fontinfo* font_info,
        int font_size,
        CcTextAlign align,
        uint32_t font_color
    );


void test_text_wordwrap(void);

#endif
