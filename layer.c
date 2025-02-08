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

#include <string.h>
#include <assert.h>
#include <wchar.h>
#include <ctype.h>
#include "layer.h"

#include "fonts/seshat.h"
#include "fonts/vegur-regular.h"
#include "fonts/monospace.h"

static
CcBitmap empty_bitmap_ = { 0 };

void cc_layer_init(CcLayer* layer, int x, int y)
{
    layer->bitmap = empty_bitmap_;
    layer->x = x;
    layer->y = y;
    layer->blend = COLOR_BLEND_OVERLAY;
}

void cc_layer_shutdown(CcLayer* layer)
{
    cc_layer_set_bitmap(layer, NULL);
}

void cc_layer_reset(CcLayer* layer)
{
    cc_layer_set_bitmap(layer, NULL);
    layer->blend = COLOR_BLEND_OVERLAY;
}

void cc_layer_set_bitmap(CcLayer* layer, CcBitmap* new_bitmap)
{
    if (!new_bitmap) {
        new_bitmap = &empty_bitmap_;
    }

    if (layer->bitmap.data != new_bitmap->data) {
        cc_bitmap_free(&layer->bitmap);
    }
    layer->bitmap = *new_bitmap;
}

void cc_layer_flip(CcLayer* layer, int horiz)
{
    CcBitmap n = layer->bitmap;
    cc_bitmap_alloc(&n);
    if (horiz)
    {
        cc_bitmap_flip_horiz(&layer->bitmap, &n);
    }
    else
    {
        cc_bitmap_flip_vert(&layer->bitmap, &n);
    }
    cc_layer_set_bitmap(layer, &n);
}

void cc_layer_rotate_90(CcLayer* layer, int repeat)
{
    CcBitmap current = layer->bitmap;
    CcBitmap next = {
        .w = current.h,
        .h = current.w
    };
    cc_bitmap_alloc(&next);


    while (repeat > 0) 
    {
        cc_bitmap_rotate_90(&current, &next);

        CcBitmap temp = current;
        current = next;
        next = temp;
        --repeat;
    }
    // forget it ever had it
    layer->bitmap = empty_bitmap_;

    cc_layer_set_bitmap(layer, &current);
    cc_bitmap_free(&next);
}

void cc_layer_rotate_angle(CcLayer* layer, double angle, uint32_t bg_color)
{
    CcTransform rotate = cc_transform_rotate(-M_PI * angle / 180.0);
    CcBitmap b = cc_bitmap_transform(&layer->bitmap, rotate, bg_color);
    cc_layer_set_bitmap(layer, &b);
}

void cc_layer_stretch(CcLayer* layer, int w, int h, int w_angle, int h_angle, uint32_t bg_color)
{
    double sw = (double)w / 100.0;
    double sh = (double)h / 100.0;
    CcTransform scale = cc_transform_scale(sw, sh);

    double ax = ((double)w_angle / 180.0) * M_PI;
    double ay = ((double)h_angle / 180.0) * -M_PI;

    CcTransform skew = cc_transform_skew(ax, ay);
    CcTransform final = cc_transform_concat(scale, skew);

    CcBitmap b = cc_bitmap_transform(&layer->bitmap, final, bg_color);
    cc_layer_set_bitmap(layer, &b);
}

void cc_layer_resize(CcLayer* layer, int new_w, int new_h, uint32_t bg_color)
{
    CcBitmap b = {
        .w = new_w,
        .h = new_h
    };
    cc_bitmap_alloc(&b);
    cc_bitmap_clear(&b, bg_color);

    cc_bitmap_blit(&layer->bitmap, &b, 0, 0, 0, 0, layer->bitmap.w, layer->bitmap.h, COLOR_BLEND_REPLACE);
    cc_layer_set_bitmap(layer, &b);
}

void cc_layer_ensure_size(CcLayer* layer, int w, int h)
{
    if (layer->bitmap.w != w ||
        layer->bitmap.h != h)
    {
        CcBitmap b = {
            .w = w,
            .h = h
        };
        cc_bitmap_alloc(&b);
        cc_layer_set_bitmap(layer, &b);
    }
}

typedef struct
{
    char* name;
    unsigned char* data;
} FontEntry;

static
const FontEntry font_table_[] = {
    { "Serif", fonts_seshat_otf },
    { "Sans-Serif", fonts_vegur_regular_otf },
    { "Monospace", fonts_monospace_otf },
};

const char* paint_font_name(int index)
{
    return font_table_[index].name;
}

int paint_font_count(void)
{
    return sizeof(font_table_) / sizeof(FontEntry);
}


void cc_text_composite(CcText *text, CcLayer* layer)
{
    assert(text);
    assert(layer);

    if (!layer->bitmap.w) return;

    cc_bitmap_clear(&layer->bitmap, COLOR_CLEAR);

    if (!text->text) return;

    if (DEBUG_LOG) {
        fprintf(stderr, "building text\n");
    }

    if (text->cached_font_ - 1 != text->font) {
        if (DEBUG_LOG) {
            fprintf(stderr, "building font\n");
        }

        const FontEntry* entry = font_table_ + text->font;
        text->cached_font_ = text->font + 1;

        if (!stbtt_InitFont(&text->font_info_, entry->data, 0))
        {
            text->cached_font_ = 0;
            fprintf(stderr, "failed to load font\n");
        }
    }

    if (text->cached_font_ == 0) {
        return;
    }

    cc_text_render(
            &layer->bitmap,
            text->text,
            &text->font_info_,
            text->font_size,
            text->font_align,
            text->font_color
            );
}

void cc_text_set_string(CcText *layer, const wchar_t* string)
{
    if (!string)
    {
        if (layer->text) {
            layer->dirty = 1;
            free(layer->text);
        }
        layer->text = NULL;
        layer->text_buffer_size = 0;
    }
    else
    {
        layer->dirty = 1;

        size_t len = wcslen(string) + 1;
        if (len > layer->text_buffer_size)
        {
            if (layer->text) free(layer->text);
            layer->text = malloc(len * sizeof(wchar_t));
        }
        wcsncpy(layer->text, string, len);
    }
}

