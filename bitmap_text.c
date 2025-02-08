#include "bitmap_text.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h" /* http://nothings.org/stb/stb_truetype.h */

#include <wchar.h>
#include <ctype.h>

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

    CcBitmap glyph_map = {
        .w = line_height * 3,
        .h = line_height
    };
    cc_bitmap_alloc(&glyph_map);
    cc_bitmap_clear(&glyph_map, color);

    CcGrayBitmap glyph_mask = {
        .w = glyph_map.w,
        .h = glyph_map.h,
    };

    cc_gray_bitmap_alloc(&glyph_mask);

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
                        (unsigned char *)glyph_mask.data,
                        c_x2 - c_x1,
                        c_y2 - c_y1,
                        glyph_mask.stride,
                        scale,
                        scale,
                        glyph_index
                        );
                // to properly handle character overlap
                // we alpha composite
                assert(c_y2 - c_y1 <= glyph_map.w);
                assert(c_x2 - c_x1 <= glyph_map.h);

                cc_bitmap_copy_channel(&glyph_map, 0, &glyph_mask);
                cc_bitmap_blit(
                        &glyph_map,
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

    cc_gray_bitmap_free(&glyph_mask);
    cc_bitmap_free(&glyph_map);
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
