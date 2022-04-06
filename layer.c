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
#include <ctype.h>
#include <assert.h>
#include <assert.h>
#include <wchar.h>

#include "layer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define STB_TRUETYPE_IMPLEMENTATION 
#include "stb_truetype.h" /* http://nothings.org/stb/stb_truetype.h */

void cc_layer_init(CcLayer* layer, int x, int y)
{
    layer->bitmaps = NULL;
    layer->x = x;
    layer->y = y;
    layer->blend = COLOR_BLEND_OVERLAY;

    layer->text = NULL;
    layer->text_buffer_size = 0;

    layer->font_size = 12;
    layer->font_align = TEXT_ALIGN_LEFT;
    layer->font = -1;
}

void cc_layer_shutdown(CcLayer* layer)
{
    cc_layer_set_text(layer, NULL); 
    cc_layer_set_bitmap(layer, NULL);
}

void cc_layer_reset(CcLayer* layer)
{
    cc_layer_set_text(layer, NULL); 
    cc_layer_set_bitmap(layer, NULL);
    layer->blend = COLOR_BLEND_OVERLAY;
}

void cc_layer_set_bitmap(CcLayer* layer, CcBitmap* new_bitmap)
{
    if (layer->bitmaps)
    {
        cc_bitmap_destroy(layer->bitmaps);
    }
    layer->bitmaps = new_bitmap;
    cc_layer_render(layer);
}

void cc_layer_flip(CcLayer* layer, int horiz)
{
    CcBitmap* n = cc_bitmap_create(layer->bitmaps->w, layer->bitmaps->h);
    if (horiz)
    {
        cc_bitmap_flip_horiz(layer->bitmaps, n);
    }
    else
    {
        cc_bitmap_flip_vert(layer->bitmaps, n);
    }
    cc_layer_set_bitmap(layer, n);
}

void cc_layer_rotate_90(CcLayer* layer, int repeat)
{
    CcBitmap* current = cc_bitmap_create_copy(layer->bitmaps);
    CcBitmap* next = cc_bitmap_create(layer->bitmaps->h, layer->bitmaps->w);

    while (repeat > 0) 
    {
        cc_bitmap_rotate_90(current, next);

        CcBitmap* temp = current;
        current = next;
        next = temp;
        --repeat;
    }

    cc_layer_set_bitmap(layer, current);
    cc_bitmap_destroy(next);
}


void cc_layer_rotate_angle(CcLayer* layer, double angle, uint32_t bg_color)
{
    CcTransform rotate = cc_transform_rotate(-M_PI * angle / 180.0);
    CcBitmap* b = cc_bitmap_transform(layer->bitmaps, NULL, rotate, bg_color);
    cc_layer_set_bitmap(layer, b);
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

    CcBitmap* b = cc_bitmap_transform(layer->bitmaps, NULL, final, bg_color);
    cc_layer_set_bitmap(layer, b);
}


void cc_layer_resize(CcLayer* layer, int new_w, int new_h, uint32_t bg_color)
{
    const CcBitmap* src = layer->bitmaps;
    CcBitmap* b = cc_bitmap_create(new_w, new_h);

    cc_bitmap_clear(b, bg_color);
    cc_bitmap_blit(src, b, 0, 0, 0, 0, src->w, src->h, COLOR_BLEND_REPLACE);

    cc_layer_set_bitmap(layer, b);
}

void cc_layer_ensure_size(CcLayer* layer, int w, int h)
{
    if (layer->bitmaps == NULL ||
        layer->bitmaps->w != w ||
        layer->bitmaps->h != h)
    {
        cc_layer_set_bitmap(layer, cc_bitmap_create(w, h));
    }
}

void cc_layer_render(CcLayer* layer)
{
    if (layer->text && layer->bitmaps && layer->font != -1)
    {
        cc_bitmap_clear(layer->bitmaps, COLOR_CLEAR);
        cc_text_render(
                layer->bitmaps,
                layer->text,
                &layer->font_info,
                layer->font_size,
                layer->font_align,
                layer->font_color
                );
    }
}

void cc_layer_set_text(CcLayer* layer, const wchar_t* text)
{
    if (!text)
    {
        if (layer->text) free(layer->text);
        layer->text = NULL;
        layer->text_buffer_size = 0;
    }
    else
    {
        size_t len = wcslen(text) + 1;
        if (len > layer->text_buffer_size)
        {
            if (layer->text) free(layer->text);
            layer->text = malloc(len * sizeof(wchar_t));
        }
        wcsncpy(layer->text, text, len);
    }
    cc_layer_render(layer);
}


typedef struct
{
    size_t size;
    size_t capacity;
    unsigned char* data;
} CompressContext;

static void write_(void* context, void* chunk_data, int chunk_size)
{
    CompressContext* ctx = context;
    if (ctx->size + chunk_size > ctx->capacity)
    {
        ctx->capacity = ((ctx->size + chunk_size) * 3) / 2;
        ctx->data = realloc(ctx->data, ctx->capacity);
    }
    memcpy(ctx->data + ctx->size, chunk_data, chunk_size);
    ctx->size += chunk_size;
}


unsigned char* cc_bitmap_compress(const CcBitmap* b, size_t* out_size)
{
    assert(b);
    
    CompressContext ctx;
    ctx.size = 0;
    ctx.capacity = 256;
    ctx.data = malloc(ctx.capacity);

    size_t stride = b->w * sizeof(uint32_t);
    stbi_write_png_compression_level = 5;
    if (stbi_write_png_to_func(write_, &ctx, b->w, b->h, 4, b->data, stride))
    {
        if (ctx.capacity > ctx.size)
        {
            ctx.data = realloc(ctx.data, ctx.size);
        }

        if (DEBUG_LOG)
        {
            printf("compress. before: %d. after: %lu\n", b->w * b->h * 4, ctx.size);
        }

        *out_size = ctx.size;
        return ctx.data;
    }

    return NULL;
}

CcBitmap* cc_bitmap_decompress(unsigned char* compressed_data, size_t compressed_size)
{
    int w, h, comps;
    unsigned char* data = stbi_load_from_memory(compressed_data, compressed_size, &w, &h, &comps, 0);
    assert(comps == 4);

    CcBitmap* b = cc_bitmap_create(w, h);
    // TODO: byte swap
    memcpy(b->data, data, sizeof(uint32_t) * w * h);
	free(data);
    return b;
}

// - Don't modify the string, just give ranges (we only take away).
// - Every character should be printed, except:
//      1. new lines
//      2. spaces which have been replaced with new lines by break. 

typedef struct {
    int start;
    int end;
    int width;
} LineRange;

int text_wordwrap(
        LineRange* lines,
        int max_lines,
        const wchar_t* text,
        int width,
        int (*advance)(const wchar_t* str, int i, void* ctx),
        void* ctx)
{
    int n = 0;

    int i = 0;
    int line_start = 0;
    int line_width = 0;

    // INVARIANTS
    // Every line ends with a word (possibly empty) and whitespace (possibly empty).
    // Every line starts with a word, or just, after a line break.
    
    // Therefore: only leading space after newlines

    while (1) 
    {
        int space_width = 0;
        int space_width_without_last = 0;
        while (iswspace(text[i]))
        {
            if (text[i] == '\n' || text[i] == '\r')
            {
                if (lines && n < max_lines)
                {
                    lines[n].start = line_start;
                    lines[n].end = i;
                    lines[n].width = line_width + space_width;
                }
                ++n;
                ++i;

                if (text[i] + text[i + 1] == '\n' + '\r') {
                  ++i;
                }

                line_start = i;
                line_width = 0;
                space_width = 0;
                space_width_without_last = 0;
            }
            else
            {
                space_width_without_last = space_width;
                space_width += advance(text, i, ctx);
                ++i;
            }
        }

        if (!text[i]) {
            line_width += space_width; 
            break;
        }

        int word_start = i;
        int word_width = 0;

        while (text[i] && !iswspace(text[i]))
        {
            word_width += advance(text, i, ctx);
            ++i;
        }

        if (line_width + space_width + word_width > width) {
            // break at end of last word
            if (lines && n < max_lines)
            {
                lines[n].start = line_start;
                lines[n].end = word_start;
                lines[n].width = line_width + space_width_without_last;
            }
            ++n;

            line_start = word_start;
            line_width = word_width;

            if (word_width > width)
            {
                // word is longer than a line
                // just make a line for it
                if (lines && n < max_lines)
                {
                    lines[n].start = line_start;
                    lines[n].end = i;
                    lines[n].width = word_width;
                }
                ++n;

                line_start = i;
                line_width = 0;
            }
        } else {
           line_width += space_width + word_width;
        }

        assert(line_width <= width);
    }

    if (lines && n < max_lines)
    {
      lines[n].start = line_start;
      lines[n].end = i;
      lines[n].width = line_width;
    }
    ++n;

    return n;
#undef I
}

static
int test_advance_(const wchar_t* text, int i, void* ctx) {
    return 1;
}

void test_text_wordwrap(void)
{
    const wchar_t* text = L"the quick\nbrown fox, jumped over the lazy dog.";
    int width = 30;

    LineRange lines[40];

    int n = text_wordwrap(lines, 40, text, width, test_advance_, NULL);
    printf("%d\n", n);

    for (int i = 0; i < n; ++i)
    {
        printf("%d, %d\n", lines[i].start, lines[i].end);
    }

    wchar_t buffer[1024];
    for (int i = 0; i < n; ++i)
    {
        LineRange r = lines[i];
        size_t l = r.end - r.start;
        memcpy(buffer, text + r.start, l * sizeof(wchar_t));
        buffer[l] = '\0';
        printf("%ls\n", buffer);
    }
}

static
void render_(
        const stbtt_fontinfo* font_info,
        int line_height,
        CcTextAlign alignment,
        uint32_t color,
        const wchar_t* text,
        const LineRange* lines,
        int line_count,
        CcBitmap* bitmap
        )
{
    float scale = stbtt_ScaleForPixelHeight(font_info, line_height);


    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(font_info, &ascent, &descent, &lineGap);

    ascent = (int)roundf(ascent * scale);
    descent = (int)roundf(descent * scale);

    CcBitmap* glyph_map = cc_bitmap_create(line_height * 3, line_height);
    unsigned char* glyph_mask = malloc(glyph_map->w * glyph_map->h);

    int i = 0;
    int y = 0; 

    while (i < line_count && y + line_height < bitmap->h)
    {
        LineRange r = lines[i];

        int j = lines[i].start;
        int end = lines[i].end;

        int x = 0;
        if (alignment == TEXT_ALIGN_CENTER)
        {
            x = (bitmap->w - r.width) / 2;
        }
        else if (alignment == TEXT_ALIGN_RIGHT)
        {
            x = bitmap->w - r.width;
        }

        int glyph_index = stbtt_FindGlyphIndex(font_info, text[j]);
        while (j != end)
        {
            int ax;
            int lsb;
            stbtt_GetGlyphHMetrics(font_info, glyph_index, &ax, &lsb);

            if (!iswspace(text[j]))
            {
                /* get bounding box for character (may be offset to account for chars that dip above or below the line */
                int c_x1, c_y1, c_x2, c_y2;
                stbtt_GetGlyphBitmapBox(font_info, glyph_index, scale, scale, &c_x1, &c_y1, &c_x2, &c_y2);
                stbtt_MakeGlyphBitmap(
                        font_info,
                        glyph_mask,
                        c_x2 - c_x1,
                        c_y2 - c_y1,
                        glyph_map->w,
                        scale,
                        scale,
                        glyph_index
                        );
                // to properly handle character overlap
                // we alpha composite
                assert(c_y2 - c_y1 <= glyph_map->w);
                assert(c_x2 - c_x1 <= glyph_map->h);

                cc_bitmap_copy_mask(glyph_map, glyph_mask, color);
                cc_bitmap_blit(
                        glyph_map,
                        bitmap,
                        0,
                        0,
                        x + roundf(lsb * scale), 
                        y + ascent + c_y1,
                        c_x2 - c_x1,
                        c_y2 - c_y1,
                        COLOR_BLEND_FULL
                        );
            }

            int next_glyph_index = stbtt_FindGlyphIndex(font_info, text[j + 1]);
            int kern = stbtt_GetGlyphKernAdvance(font_info, glyph_index, next_glyph_index);
            x += roundf((ax + kern) * scale);
            j += 1;
            glyph_index = next_glyph_index;
        }

        y += line_height;
        i += 1;
    }

    free(glyph_mask);
    cc_bitmap_destroy(glyph_map);
}

typedef struct
{
    const stbtt_fontinfo* font_info;
    float scale;
} WordWrapData;

static
int kern_advance_(const wchar_t* text, int i, void* ctx)
{
    const WordWrapData* a = ctx;
    const stbtt_fontinfo* font_info = a->font_info;
    float scale = a->scale;

    int ax;
    int lsb;
    stbtt_GetCodepointHMetrics(font_info, text[i], &ax, &lsb);

    int kern = stbtt_GetCodepointKernAdvance(font_info, text[i], text[i + 1]);
    return  (int)roundf((ax + kern) * scale);
}

void cc_text_render(
        CcBitmap* bitmap,
        const wchar_t* text,
        const stbtt_fontinfo* font_info,
        int font_size,
        CcTextAlign align,
        uint32_t font_color
    )
{
    if (!text || text[0] == '\0') {
        return;
    }

    int line_height = (font_size * 2);

    WordWrapData d;
    d.font_info = font_info;
    d.scale = stbtt_ScaleForPixelHeight(font_info, line_height);

    LineRange lines[2048];

    int n = text_wordwrap(lines, 2048, text, bitmap->w, kern_advance_, &d);
    render_(font_info, line_height, align, font_color, text, lines, n, bitmap);
}


