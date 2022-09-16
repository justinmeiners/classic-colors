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

#include "paint.h"

#include "stb_image.h"
#include "stb_image_write.h"

#include "fonts/seshat.h"
#include "fonts/vegur-regular.h"
#include "fonts/monospace.h"

typedef struct
{
    char* name;
    unsigned char* data;
} FontEntry;

static
const FontEntry font_table[] = {
    { "Serif", fonts_seshat_otf },
    { "Sans-Serif", fonts_vegur_regular_otf },
    { "Monospace", fonts_monospace_otf },
};

const char* paint_font_name(int index)
{
    return font_table[index].name;
}

int paint_font_count(void)
{
    return sizeof(font_table) / sizeof(FontEntry);
}

void paint_undo_save(PaintContext* ctx, int x, int y, int w, int h)
{
    CcRect r = {
        x, y, w, h
    };

    const CcLayer* l = ctx->layers + LAYER_MAIN;

    CcRect to_save;
    if (!cc_rect_intersect(cc_layer_rect(l), r, &to_save)) return;

    UndoPatch* patch = undo_patch_create(ctx->layers[LAYER_MAIN].bitmaps, to_save);
    cc_undo_queue_push(&ctx->undo, patch);
}

static
void paint_undo_save_full(PaintContext* ctx)
{
    if (ctx->active_layer == LAYER_MAIN)
    {
        paint_undo_save(ctx, 0, 0, paint_w(ctx), paint_h(ctx));
    }
}

void paint_undo(PaintContext* ctx)
{
    cc_undo_queue_trim(&ctx->undo, ctx->max_undo);
    cc_undo_queue_undo(&ctx->undo, ctx->layers + LAYER_MAIN);

    ctx->active_layer = LAYER_MAIN;
    cc_layer_reset(ctx->layers + LAYER_OVERLAY);
    cc_polygon_clear(&ctx->polygon);
}

void paint_redo(PaintContext* ctx)
{
    cc_undo_queue_redo(&ctx->undo, ctx->layers + LAYER_MAIN);

    ctx->active_layer = LAYER_MAIN;
    cc_layer_reset(ctx->layers + LAYER_OVERLAY);
}


const char* paint_file_path(PaintContext* ctx)
{
    if (ctx->open_file_path[0] == '\0')
    {
        return NULL;
    }
    return ctx->open_file_path;
}


int paint_open_file(PaintContext* ctx, const char* path, const char** error_message)
{
    CcBitmap* b;
    if (path == NULL)
    {
        ctx->open_file_path[0] = '\0';
        b = cc_bitmap_create(640, 480);
        cc_bitmap_clear(b, ctx->bg_color);
    }
    else
    {
        int w, h;
        unsigned char* data = stbi_load(path, &w, &h, NULL, 4);
        if (!data)
        {
            if (error_message)
            {
                *error_message = stbi_failure_reason();
            }
            return 0;
        }

        b = cc_bitmap_create(w, h);
        cc_bitmap_copy_buffer(b, data);
        strncpy(ctx->open_file_path, path, OS_PATH_MAX);
		free(data);
    }

    cc_layer_set_bitmap(ctx->layers + LAYER_MAIN, b);
    cc_layer_reset(ctx->layers + LAYER_OVERLAY);
    ctx->active_layer = LAYER_MAIN;

    cc_viewport_init(&ctx->viewport);

    cc_undo_queue_clear(&ctx->undo);
    paint_undo_save_full(ctx);
	return 1;
}

enum {
    SAVE_PNG = 0,
    SAVE_BMP = 1,
    SAVE_TGA = 2,
    SAVE_JPG = 3,
};

#define JPG_QUALITY 80

static
int save_file_(PaintContext* ctx, const char* path)
{
    if (!path) return 0;

    int mode = SAVE_PNG;
    const char* extension = strchr(path, '.');
    if (extension)
    {
        char lower[OS_PATH_MAX];
        strncpy(lower, extension + 1,  OS_PATH_MAX);
        for (char *c = lower; *c != '\0'; ++c)
            *c = tolower(*c);

        if (strcmp(lower, "png") == 0)
        {
            mode = SAVE_PNG;
        }
        else if (strcmp(lower, "jpg") == 0)
        {
            mode = SAVE_JPG;
        }
        else if (strcmp(lower, "tga") == 0)
        {
            mode = SAVE_TGA;
        }
        else if (strcmp(lower, "bmp") == 0)
        {
            mode = SAVE_BMP;
        }
    }

    if (DEBUG_LOG)
    {
        printf("saving file: %s %d\n", path, mode);
    }

    const CcLayer* l = ctx->layers + LAYER_MAIN;
    const CcBitmap* b = l->bitmaps;


    int n = b->w * b->h;
    unsigned char* to_save = malloc(n * 4);
    int comps[4];
    for (int i = 0; i < n; ++i)
    {
        cc_color_unpack(b->data[i], comps);
        for (int j = 0; j < 4; ++j)
        {
            to_save[i * 4 + j] = comps[j];
        }
    }

    int comp = 4;
    int stride = b->w * sizeof(uint32_t);

    int success;
    switch (mode)
    {
        case SAVE_PNG:
            stbi_write_png_compression_level = 10;
            success = stbi_write_png(path, b->w, b->h, comp, to_save, stride);
            break;
        case SAVE_BMP:
            success = stbi_write_bmp(path, b->w, b->h, comp, to_save);
            break;
        case SAVE_TGA:
            success = stbi_write_tga(path, b->w, b->h, comp, to_save);
            break;
        case SAVE_JPG:
            success = stbi_write_jpg(path, b->w, b->h, comp, to_save, JPG_QUALITY);
            break;
    }

    free(to_save);
    return success;
}

int paint_save_file_as(PaintContext* ctx, const char* path)
{
    strncpy(ctx->open_file_path, path, OS_PATH_MAX);
    return save_file_(ctx, paint_file_path(ctx));
}

int paint_save_file(PaintContext* ctx)
{
    int result = save_file_(ctx, paint_file_path(ctx));
    cc_undo_queue_trim(&ctx->undo, ctx->max_undo);
    return result;
}

int paint_init(PaintContext* ctx)
{
    ctx->line_width = 1;
    ctx->brush_width = 4;
    ctx->eraser_width = 16;
    ctx->zoom_level = 8;
    ctx->paste_board_data = NULL;
    ctx->paste_board_size = 0;

    ctx->max_undo = 80;
    ctx->tool_force_align = 0;

    cc_viewport_init(&ctx->viewport);

    ctx->tool = TOOL_BRUSH;
    ctx->previous_tool = TOOL_BRUSH;

    ctx->shape_flags = SHAPE_STROKE;
    ctx->bucket_mode = BUCKET_CONTIGUOUS;

    ctx->fg_color = COLOR_BLACK;
    ctx->bg_color = COLOR_WHITE;
    ctx->view_bg_color = COLOR_GRAY;

    ctx->select_mode = SELECT_KEEP_BG;
    ctx->request_tool_timer = 0;

    for (int i = 0; i < LAYER_COUNT; ++i)
        cc_layer_init(ctx->layers + i, 0, 0);

    cc_polygon_init(&ctx->polygon);
    cc_undo_queue_init(&ctx->undo);
    paint_open_file(ctx, NULL, NULL);
    return 1;
}

void paint_invert_colors(PaintContext* ctx)
{
    CcLayer* l = ctx->layers + ctx->active_layer;
    cc_bitmap_invert_colors(l->bitmaps);
    paint_undo_save_full(ctx);
}

void paint_flip(PaintContext* ctx, int horiz)
{
    CcLayer* l = ctx->layers + ctx->active_layer;
    cc_layer_flip(l, horiz);
    paint_undo_save_full(ctx);
}

void paint_rotate_90(PaintContext* ctx, int repeat)
{
    CcLayer* l = ctx->layers + ctx->active_layer;
    cc_layer_rotate_90(l, repeat);
    paint_undo_save_full(ctx);
}

void paint_rotate_angle(PaintContext* ctx, double angle)
{
    CcLayer* l = ctx->layers + ctx->active_layer;
    cc_layer_rotate_angle(l, angle, ctx->bg_color);
    paint_undo_save_full(ctx);
}

void paint_stretch(PaintContext* ctx, int w, int h, int w_angle, int h_angle)
{
    if (DEBUG_LOG) printf("stretch %d, %d, %d, %d\n", w, h, w_angle, h_angle);
    CcLayer* l = ctx->layers + ctx->active_layer;
    cc_layer_stretch(l, w, h, w_angle, h_angle, ctx->bg_color);
    paint_undo_save_full(ctx);
}

void paint_clear(PaintContext* ctx)
{
    CcLayer* l = ctx->layers + ctx->active_layer;
    cc_bitmap_clear(l->bitmaps, ctx->bg_color);
    paint_undo_save_full(ctx);
}

void paint_resize(PaintContext* ctx, int new_w, int new_h)
{
    CcLayer* l = ctx->layers + ctx->active_layer;
    cc_layer_resize(l, new_w, new_h, ctx->bg_color);
    paint_undo_save_full(ctx);
}

static
uint32_t fg_color_(PaintContext* ctx)
{
    return (ctx->mouse_button == 1) ? ctx->fg_color : ctx->bg_color;
}

static
uint32_t bg_color_(PaintContext* ctx)
{
    return (ctx->mouse_button == 1) ? ctx->bg_color : ctx->fg_color;
}

static
uint32_t shape_stroke_color_(PaintContext* ctx)
{
    return fg_color_(ctx);
}

static
uint32_t shape_fill_color_(PaintContext* ctx)
{
    if ((ctx->shape_flags & SHAPE_FILL) && (ctx->shape_flags & SHAPE_STROKE))
    {
        return bg_color_(ctx);
    }
    else
    {
        return fg_color_(ctx);
    }
}

static
void prepare_empty_overlay_(PaintContext* ctx)
{
    CcLayer* l = ctx->layers + LAYER_OVERLAY;
    l->x = 0;
    l->y = 0;

    cc_layer_ensure_size(l, paint_w(ctx), paint_h(ctx));
    cc_bitmap_clear(l->bitmaps, COLOR_CLEAR);
    l->blend = COLOR_BLEND_OVERLAY;
    cc_layer_set_text(l, NULL);
}

static
void settle_selection_layer_(PaintContext* ctx)
{
    if (ctx->active_layer == LAYER_OVERLAY)
    {
        CcLayer* l = ctx->layers + LAYER_OVERLAY;

        if (DEBUG_LOG)
        {
            printf("settling layer\n");
        }
        CcBitmap* b = ctx->layers[LAYER_MAIN].bitmaps;

        cc_bitmap_blit(l->bitmaps, b, 0, 0, l->x, l->y, cc_layer_w(l), cc_layer_h(l), l->blend);
        paint_undo_save(ctx, l->x, l->y, l->bitmaps->w, l->bitmaps->h);

        cc_layer_reset(l);
        ctx->active_layer = LAYER_MAIN;
    }
}

static
void settle_polygon_(PaintContext* ctx)
{
    cc_polygon_cleanup(&ctx->polygon, 1);
    if (ctx->polygon.count > 0)
    {
        CcBitmap* b = ctx->layers[LAYER_MAIN].bitmaps;

        if (ctx->shape_flags & SHAPE_FILL)
        {
            cc_bitmap_fill_polygon(
                    b, 
                    ctx->polygon.points,
                    ctx->polygon.count, 
                    shape_fill_color_(ctx)
                    );
        }
        if (ctx->shape_flags & SHAPE_STROKE)
        {
            cc_bitmap_stroke_polygon(
                    b, 
                    ctx->polygon.points,
                    ctx->polygon.count, 
                    1,
                    ctx->line_width,
                    shape_stroke_color_(ctx)
                    );
        }

        CcRect r = cc_rect_pad(
            cc_polygon_rect(&ctx->polygon),
            ctx->line_width,
            ctx->line_width
        );
        paint_undo_save(ctx, r.x, r.y, r.w, r.h);
        cc_polygon_clear(&ctx->polygon);
        prepare_empty_overlay_(ctx);
    }
}

void paint_set_tool(PaintContext* ctx, PaintTool tool)
{
    if (ctx->tool == TOOL_POLYGON)
        settle_polygon_(ctx);

    if (tool != ctx->tool)
    {
        ctx->previous_tool = ctx->tool;
        ctx->tool = tool;
    }
    settle_selection_layer_(ctx);
}

static
void redraw_polygon_(PaintContext* ctx)
{
    CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
    cc_bitmap_clear(overlay->bitmaps, COLOR_CLEAR);

    cc_bitmap_stroke_polygon(
            overlay->bitmaps,
            ctx->polygon.points,
            ctx->polygon.count,
            0,
            ctx->line_width,
            fg_color_(ctx)
            );
}

static
void update_tool_min_(PaintContext* ctx, int x, int y)
{
    extend_interval(x, &ctx->tool_min_x, &ctx->tool_max_x);
    extend_interval(y, &ctx->tool_min_y, &ctx->tool_max_y);
}

void paint_tool_down(PaintContext* ctx, int x, int y, int button)
{
    ctx->mouse_button = button;
    CcBitmap* b = ctx->layers[LAYER_MAIN].bitmaps;

    switch (ctx->tool)
    {
        case TOOL_PENCIL:
            cc_bitmap_draw_square(b, x, y, ctx->line_width, fg_color_(ctx));
            break;
        case TOOL_ERASER:
            cc_bitmap_draw_square(b, x, y, ctx->eraser_width, bg_color_(ctx));
            break;
        case TOOL_BRUSH:
            cc_bitmap_draw_circle(b, x, y, ctx->brush_width,  fg_color_(ctx));
            break;
        case TOOL_SPRAY_CAN:
            ctx->request_tool_timer = 1;
            break;
        case TOOL_PAINT_BUCKET:
        {
            switch (ctx->bucket_mode)
            {
                case BUCKET_CONTIGUOUS:
                {
                    CcRect r = cc_bitmap_flood_fill(b, x, y, fg_color_(ctx));
                    paint_undo_save(ctx, r.x, r.y, r.w, r.h);
                    break;
                }
                case BUCKET_GLOBAL:
                 cc_bitmap_replace(b, cc_bitmap_get(b, x, y, 0), fg_color_(ctx));
                 paint_undo_save_full(ctx);
                 break;
            }
            break;
        }
        case TOOL_MAGNIFIER:
        {
            prepare_empty_overlay_(ctx);
            if (ctx->mouse_button == 1)
            {
                ctx->line_x = x;
                ctx->line_y = y;
                paint_tool_move(ctx, x, y);
            }
            break;
        }
        case TOOL_LINE:
        case TOOL_RECTANGLE:
        case TOOL_ELLIPSE:
            prepare_empty_overlay_(ctx);
            ctx->line_x = x;
            ctx->line_y = y;
            break;
        case TOOL_SELECT_POLYGON:
            if (ctx->active_layer == LAYER_OVERLAY)
            {
                const CcLayer* l = ctx->layers + ctx->active_layer;
                if (!cc_rect_contains(cc_layer_rect(l), x, y))
                {
                    settle_selection_layer_(ctx);
                    prepare_empty_overlay_(ctx);
                }
                break;
            }
        case TOOL_POLYGON:
        {
            prepare_empty_overlay_(ctx);

            CcCoord coord = { x, y };
            cc_polygon_add(&ctx->polygon, coord);
            if (ctx->polygon.count == 1) cc_polygon_add(&ctx->polygon, coord);
            redraw_polygon_(ctx);
            break;
        }
        case TOOL_EYE_DROPPER:
        {
            uint32_t c = cc_bitmap_get(b, x, y, ctx->bg_color);
            if (ctx->mouse_button == 1)
            {
                ctx->fg_color = c;
            }
            else
            {
                ctx->bg_color = c;
            }
            break;
        }
        case TOOL_TEXT:
        case TOOL_SELECT_RECTANGLE:
        {
            ctx->line_x = x;
            ctx->line_y = y;

            if (ctx->active_layer == LAYER_OVERLAY)
            {
                const CcLayer* l = ctx->layers + ctx->active_layer;
                if (!cc_rect_contains(cc_layer_rect(l), x, y))
                {
                    settle_selection_layer_(ctx);
                    prepare_empty_overlay_(ctx);
                }
            }
            else
            {
                prepare_empty_overlay_(ctx);
            }

        }
    }

    ctx->tool_x = ctx->tool_min_x = ctx->tool_max_x = x;
    ctx->tool_y = ctx->tool_min_y = ctx->tool_max_y = y;
}

void paint_tool_move(PaintContext* ctx, int x, int y)
{
    extend_interval(x, &ctx->tool_min_x, &ctx->tool_max_x);
    extend_interval(y, &ctx->tool_min_y, &ctx->tool_max_y);

    CcBitmap* b = ctx->layers[LAYER_MAIN].bitmaps;
    switch (ctx->tool)
    {
        case TOOL_PENCIL:
            cc_bitmap_interp_square(b, ctx->tool_x, ctx->tool_y, x, y, ctx->line_width, fg_color_(ctx));
            break;
        case TOOL_ERASER:
            cc_bitmap_interp_square(b, ctx->tool_x, ctx->tool_y, x, y, ctx->eraser_width, bg_color_(ctx));
            break;
        case TOOL_BRUSH:
            cc_bitmap_interp_circle(b, ctx->tool_x, ctx->tool_y, x, y, ctx->brush_width,  fg_color_(ctx));
            break;
        case TOOL_MAGNIFIER:
        {
            if (ctx->mouse_button == 1)
            {
                int w = ctx->viewport.w / ctx->zoom_level;
                int h = ctx->viewport.h / ctx->zoom_level;

                CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
                overlay->blend = COLOR_BLEND_INVERT;
                cc_bitmap_clear(overlay->bitmaps, COLOR_CLEAR);
                cc_bitmap_stroke_rect(overlay->bitmaps,  x - w/2, y - h/2, x + w/2, y + h/2, 1, COLOR_BLACK);
            }
            break;
        }
        case TOOL_LINE:
        {
            if (ctx->tool_force_align)
            {
                cc_line_align_to_45(ctx->line_x, ctx->line_y, x, y, &x, &y);
            }

            CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
            cc_bitmap_clear(overlay->bitmaps, COLOR_CLEAR);
            cc_bitmap_interp_square(overlay->bitmaps, ctx->line_x, ctx->line_y, x, y, ctx->line_width, fg_color_(ctx));
            break;
        }
        case TOOL_RECTANGLE:
        {
            if (ctx->tool_force_align)
            {
                align_rect_to_square(ctx->line_x, ctx->line_y, x, y, &x, &y);
            }
 
            CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
            cc_bitmap_clear(overlay->bitmaps, COLOR_CLEAR);

            if (ctx->shape_flags & SHAPE_FILL)
            {
                cc_bitmap_fill_rect(overlay->bitmaps, ctx->line_x, ctx->line_y, x, y, shape_fill_color_(ctx));
            }
            if (ctx->shape_flags & SHAPE_STROKE)
            {
                cc_bitmap_stroke_rect(overlay->bitmaps, ctx->line_x, ctx->line_y, x, y, ctx->line_width, shape_stroke_color_(ctx));
            }
            break;
        }
        case TOOL_ELLIPSE:
        {
            if (ctx->tool_force_align)
            {
                align_rect_to_square(ctx->line_x, ctx->line_y, x, y, &x, &y);
            }
 
            CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
            cc_bitmap_clear(overlay->bitmaps, COLOR_CLEAR);

            if (ctx->shape_flags & SHAPE_FILL)
            {
                cc_bitmap_fill_ellipse(overlay->bitmaps, ctx->line_x, ctx->line_y, x, y, shape_fill_color_(ctx));
            }
            if (ctx->shape_flags & SHAPE_STROKE)
            {
                cc_bitmap_stroke_ellipse(overlay->bitmaps, ctx->line_x, ctx->line_y, x, y, shape_stroke_color_(ctx));
            }
            break;
        }
        case TOOL_SELECT_POLYGON:
        {
            if (ctx->active_layer == LAYER_OVERLAY)
            {
                CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
                overlay->x += x - ctx->tool_x;
                overlay->y += y - ctx->tool_y;
            }
            else
            {
                CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
                overlay->blend = COLOR_BLEND_INVERT;

                CcCoord coord = { x, y };
                cc_polygon_add(&ctx->polygon, coord);
                cc_bitmap_clear(overlay->bitmaps, COLOR_CLEAR);

                cc_bitmap_stroke_polygon(
                        overlay->bitmaps,
                        ctx->polygon.points,
                        ctx->polygon.count,
                        0,
                        1,
                        COLOR_BLACK
                        );
            }
            break;
        }
        case TOOL_POLYGON:
        {
            CcCoord coord = { x, y };
            cc_polygon_update_last(&ctx->polygon, coord, ctx->tool_force_align);
            redraw_polygon_(ctx);
            break;
        }
        case TOOL_EYE_DROPPER:
        {
            uint32_t c = cc_bitmap_get(b, x, y, ctx->bg_color);
            if (ctx->mouse_button == 1)
            {
                ctx->fg_color = c;
            }
            else
            {
                ctx->bg_color = c;
            }
            break;
        }
        case TOOL_TEXT:
        case TOOL_SELECT_RECTANGLE:
        {
            if (ctx->active_layer == LAYER_OVERLAY)
            {
                CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
                overlay->x += x - ctx->tool_x;
                overlay->y += y - ctx->tool_y;
            }
            else
            {
                CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
                overlay->blend = COLOR_BLEND_INVERT;
                cc_bitmap_clear(overlay->bitmaps, COLOR_CLEAR);
                cc_bitmap_dotted_rect(overlay->bitmaps, ctx->line_x, ctx->line_y, x, y, COLOR_BLACK);
            }
            break;
        }
 
    }
    ctx->tool_x = x;
    ctx->tool_y = y;
    update_tool_min_(ctx, x, y);
}

#define SPRAY_DENSITY 10

void paint_tool_update(PaintContext* ctx)
{
    CcBitmap* b = ctx->layers[LAYER_MAIN].bitmaps;

    switch (ctx->tool)
    {
        case TOOL_SPRAY_CAN:
            cc_bitmap_draw_spray(b, ctx->tool_x, ctx->tool_y, ctx->brush_width, SPRAY_DENSITY, fg_color_(ctx));
            break;
    }
}

static
void push_undo_stroke_(PaintContext* ctx, int radius)
{
    const CcLayer* l = ctx->layers + LAYER_MAIN;
    CcRect r = cc_rect_from_extrema(
            ctx->tool_min_x, ctx->tool_min_y,
            ctx->tool_max_x, ctx->tool_max_y
            );

    r = cc_rect_pad(r, radius, radius);
    cc_rect_intersect(r, cc_layer_rect(l), &r);
    paint_undo_save(ctx, r.x, r.y, r.w, r.h);
}

static
void push_undo_box_(PaintContext* ctx, int end_x, int end_y, int radius)
{
    const CcLayer* l = ctx->layers + LAYER_MAIN;
    CcRect r = cc_rect_around_corners(
            ctx->line_x, ctx->line_y,
            end_x, end_y 
            );

    r = cc_rect_pad(r, radius, radius);
    cc_rect_intersect(r, cc_layer_rect(l), &r);
    paint_undo_save(ctx, r.x, r.y, r.w, r.h);
}

void paint_tool_up(PaintContext* ctx, int x, int y, int button)
{
    ctx->request_tool_timer = 0;
    CcBitmap* b = ctx->layers[LAYER_MAIN].bitmaps;

    switch (ctx->tool)
    {
        case TOOL_PENCIL:
           push_undo_stroke_(ctx, ctx->line_width);
           break;
        case TOOL_ERASER:
           push_undo_stroke_(ctx, ctx->eraser_width);
           break;
        case TOOL_BRUSH:
           push_undo_stroke_(ctx, ctx->brush_width);
           break;
        case TOOL_SPRAY_CAN:
           push_undo_stroke_(ctx, ctx->brush_width);
           break;
        case TOOL_EYE_DROPPER:
           paint_set_tool(ctx, ctx->previous_tool);
           break;
        case TOOL_MAGNIFIER:
           if (ctx->mouse_button == 1)
           {
               int w = ctx->viewport.w / ctx->zoom_level;
               int h = ctx->viewport.h / ctx->zoom_level;

               ctx->viewport.zoom = ctx->zoom_level;
               ctx->viewport.paint_x = MIN(MAX(x - w / 2, 0), paint_w(ctx));
               ctx->viewport.paint_y = MIN(MAX(y - h / 2, 0), paint_h(ctx));
               paint_set_tool(ctx, ctx->previous_tool);
           }
           else
           {
               ctx->viewport.zoom = 1;
               ctx->viewport.paint_x = 0;
               ctx->viewport.paint_y = 0;
           }

           cc_layer_reset(ctx->layers + LAYER_OVERLAY);
           break;
        case TOOL_LINE:
            if (ctx->tool_force_align)
            {
                cc_line_align_to_45(ctx->line_x, ctx->line_y, x, y, &x, &y);
            }

            cc_layer_set_bitmap(ctx->layers + LAYER_OVERLAY, NULL);
            cc_bitmap_interp_square(b, ctx->line_x, ctx->line_y, x, y, ctx->line_width, fg_color_(ctx));
            push_undo_box_(ctx, x, y, ctx->line_width);
            break;
        case TOOL_RECTANGLE:
            if (ctx->tool_force_align)
            {
                align_rect_to_square(ctx->line_x, ctx->line_y, x, y, &x, &y);
            }
            cc_layer_set_bitmap(ctx->layers + LAYER_OVERLAY, NULL);

            if (ctx->shape_flags & SHAPE_FILL)
            {
                cc_bitmap_fill_rect(b, ctx->line_x, ctx->line_y, x, y, shape_fill_color_(ctx));
            }
            if (ctx->shape_flags & SHAPE_STROKE)
            {
                cc_bitmap_stroke_rect(b, ctx->line_x, ctx->line_y, x, y, ctx->line_width, shape_stroke_color_(ctx));
            }
            push_undo_box_(ctx, x, y, ctx->line_width);
            break;
        case TOOL_ELLIPSE:
            if (ctx->tool_force_align)
            {
                align_rect_to_square(ctx->line_x, ctx->line_y, x, y, &x, &y);
            }
            cc_layer_set_bitmap(ctx->layers + LAYER_OVERLAY, NULL);

            if (ctx->shape_flags & SHAPE_FILL)
            {
                cc_bitmap_fill_ellipse(b, ctx->line_x, ctx->line_y, x, y, shape_fill_color_(ctx));
            }
            if (ctx->shape_flags & SHAPE_STROKE)
            {
                cc_bitmap_stroke_ellipse(b, ctx->line_x, ctx->line_y, x, y, shape_stroke_color_(ctx));
            }
            push_undo_box_(ctx, x, y, ctx->line_width);
            break;
        case TOOL_SELECT_POLYGON:
        {
            if (ctx->active_layer == LAYER_MAIN)
            {
                paint_select_polygon(ctx);
            }
            cc_polygon_clear(&ctx->polygon);
            break;
        }
        case TOOL_POLYGON:
            redraw_polygon_(ctx);
            break;
        case TOOL_TEXT:
            if (ctx->active_layer == LAYER_MAIN)
            {
                CcRect rect = cc_rect_around_corners(x, y, ctx->line_x, ctx->line_y);
                if (rect.w > 0 && rect.h > 0)
                {
                    CcBitmap* b = cc_bitmap_create(rect.w, rect.h);
                    cc_bitmap_clear(b, COLOR_CLEAR);

                    CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
                    overlay->blend = COLOR_BLEND_OVERLAY;
                    overlay->x = rect.x;
                    overlay->y = rect.y;
                    overlay->font_color = ctx->fg_color;
                    cc_layer_set_bitmap(overlay, b);
                    cc_layer_set_text(overlay, NULL);

                    if (overlay->font == -1)
                        paint_set_font(ctx, overlay, 0);

                    ctx->active_layer = LAYER_OVERLAY;
                }
            }
            break;
        case TOOL_SELECT_RECTANGLE:
            if (ctx->active_layer == LAYER_MAIN)
            {
                CcRect rect = cc_rect_around_corners(x, y, ctx->line_x, ctx->line_y);
                paint_select(ctx, rect.x, rect.y, rect.w, rect.h);
            }
            break;
    }
}

void paint_tool_cancel(PaintContext* ctx)
{
    if (DEBUG_LOG) printf("CANCEL\n");
}

int paint_w(const PaintContext* ctx)
{
    const CcLayer* l = ctx->layers + LAYER_MAIN;
    return l->bitmaps->w;
}

int paint_h(const PaintContext* ctx)
{
    const CcLayer* l = ctx->layers + LAYER_MAIN;
    return l->bitmaps->h;
}

void paint_crop(PaintContext* ctx)
{
    if (ctx->active_layer == LAYER_OVERLAY)
    {
        CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
        CcLayer* main = ctx->layers + LAYER_MAIN;

        cc_layer_set_bitmap(main, cc_bitmap_create_copy(overlay->bitmaps));
        cc_layer_reset(overlay);

        ctx->active_layer = LAYER_MAIN;
        paint_undo_save_full(ctx);
    }
}

void paint_composite(PaintContext* ctx)
{
    assert(ctx->viewport.w >= 0);
    assert(ctx->viewport.h >= 0);
    assert(ctx->viewport.zoom > 0);

    int vx = ctx->viewport.paint_x;
    int vy = ctx->viewport.paint_y;

    // image may not evenly divide by the zoom factor
    int target_w = int_ceil(ctx->viewport.w, ctx->viewport.zoom);
    int target_h = int_ceil(ctx->viewport.h, ctx->viewport.zoom);

    cc_layer_ensure_size(ctx->layers + LAYER_INTERMEDIATE, target_w, target_h);
    cc_layer_ensure_size(ctx->layers + LAYER_COMPOSITE, ctx->viewport.w, ctx->viewport.h);

    int needs_zoom = ctx->viewport.zoom != 1;

    int target_layer = needs_zoom ? LAYER_INTERMEDIATE : LAYER_COMPOSITE;
    CcLayer* target = ctx->layers + target_layer;
    cc_bitmap_clear(target->bitmaps, ctx->view_bg_color);

    for (int i = LAYER_MAIN; i < LAYER_COUNT; ++i)
    {
        const CcLayer* l = ctx->layers + i;
        if (l->bitmaps != NULL)
        {
            cc_bitmap_blit(
                    l->bitmaps,
                    target->bitmaps,
                    0, 0,
                    l->x - vx, l->y - vy,
                    cc_layer_w(l), cc_layer_h(l),
                    l->blend
                    );
        }
    }

    if (needs_zoom)
    {
        cc_bitmap_zoom(target->bitmaps, ctx->layers[LAYER_COMPOSITE].bitmaps, ctx->viewport.zoom); 
    }
}


void paint_copy(PaintContext* ctx)
{
    if (ctx->active_layer == LAYER_OVERLAY)
    {
        if (ctx->paste_board_data)
        {
            free(ctx->paste_board_data);
        }

        ctx->paste_board_data = cc_bitmap_compress(ctx->layers[LAYER_OVERLAY].bitmaps, &ctx->paste_board_size);
    }
}

void paint_cut(PaintContext* ctx)
{
    paint_copy(ctx);
    cc_layer_reset(ctx->layers + LAYER_OVERLAY);
    ctx->active_layer = LAYER_MAIN;
}

void paint_paste(PaintContext* ctx)
{
    if (ctx->paste_board_data)
    {
        paint_set_tool(ctx, TOOL_SELECT_RECTANGLE);
        CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
        overlay->x = ctx->viewport.paint_x;
        overlay->y = ctx->viewport.paint_y;
        cc_layer_set_bitmap(overlay, cc_bitmap_decompress(ctx->paste_board_data, ctx->paste_board_size));
        ctx->active_layer = LAYER_OVERLAY;
    }
}

void paint_select_all(PaintContext* ctx)
{
    paint_select(ctx, 0, 0, paint_w(ctx), paint_h(ctx));
}

void paint_select(PaintContext* ctx, int x, int y, int w, int h)
{
    paint_set_tool(ctx, TOOL_SELECT_RECTANGLE);
    if (w <= 0 || h <= 0) return;

    CcLayer* l = ctx->layers + LAYER_MAIN;

    CcRect rect = {
        x, y, w, h
    };
    if (!cc_rect_intersect(rect, cc_layer_rect(l), &rect)) return;

    CcBitmap* b = cc_bitmap_create(rect.w, rect.h);
    cc_bitmap_clear(b, bg_color_(ctx));
    cc_bitmap_blit(
            l->bitmaps,
            b,
            rect.x, rect.y,
            0, 0,
            rect.w, rect.h,
            COLOR_BLEND_REPLACE
            );

    if (ctx->select_mode == SELECT_IGNORE_BG)
    {
        /* remove background color */
        cc_bitmap_replace(b, ctx->bg_color, COLOR_CLEAR);
    }

    CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
    overlay->x = rect.x;
    overlay->y = rect.y;
    overlay->blend = COLOR_BLEND_OVERLAY;
    cc_layer_set_bitmap(overlay, b);

    cc_bitmap_fill_rect(
            l->bitmaps,
            rect.x, rect.y,
            rect.x + rect.w - 1,
            rect.y + rect.h - 1,
            bg_color_(ctx)
            );

    paint_undo_save(ctx, overlay->x, overlay->y, cc_layer_w(overlay), cc_layer_h(overlay));
    ctx->active_layer = LAYER_OVERLAY;
}

void paint_select_polygon(PaintContext* ctx)
{
    paint_set_tool(ctx, TOOL_SELECT_POLYGON);

    cc_polygon_cleanup(&ctx->polygon, 1);
    if (ctx->polygon.count < 3) return;

    CcLayer* l = ctx->layers + LAYER_MAIN;
    CcRect rect = cc_polygon_rect(&ctx->polygon);
    if (!cc_rect_intersect(rect, cc_layer_rect(l), &rect)) return;

    /* create mask from polygon */
    CcCoord shift = { -rect.x, -rect.y };
    cc_polygon_shift(&ctx->polygon, shift);

    CcBitmap* mask = cc_bitmap_create(rect.w, rect.h);
    cc_bitmap_clear(mask, COLOR_CLEAR);
    cc_bitmap_fill_polygon(mask, ctx->polygon.points, ctx->polygon.count, COLOR_WHITE);
    cc_bitmap_stroke_polygon(mask, ctx->polygon.points, ctx->polygon.count, 1, 1, COLOR_WHITE);

    /* copy selection into new bitmap */
    CcBitmap* b = cc_bitmap_create(rect.w, rect.h);
    cc_bitmap_clear(b, bg_color_(ctx));
    cc_bitmap_blit(
            l->bitmaps,
            b,
            rect.x, rect.y,
            0, 0,
            rect.w, rect.h,
            COLOR_BLEND_REPLACE
            );

    if (ctx->select_mode == SELECT_IGNORE_BG)
    {
        /* remove background color */
        cc_bitmap_replace(b, ctx->bg_color, COLOR_CLEAR);
    }

    /* apply mask to selection */
    cc_bitmap_blit(mask, b, 0, 0, 0, 0, rect.w, rect.h, COLOR_BLEND_MULTIPLY);

    /* apply mask to image to clear where the selection was */
    cc_bitmap_replace(mask, COLOR_WHITE, bg_color_(ctx));
    cc_bitmap_blit(mask, l->bitmaps, 0, 0, rect.x, rect.y, rect.w, rect.h, COLOR_BLEND_OVERLAY);
    cc_bitmap_destroy(mask);

    CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
    overlay->x = rect.x;
    overlay->y = rect.y;
    overlay->blend = COLOR_BLEND_OVERLAY;
    cc_layer_set_bitmap(overlay, b);

    paint_undo_save(ctx, overlay->x, overlay->y, cc_layer_w(overlay), cc_layer_h(overlay));
    ctx->active_layer = LAYER_OVERLAY;
}

void paint_select_clear(PaintContext* ctx)
{
    paint_set_tool(ctx, TOOL_SELECT_RECTANGLE);
    ctx->active_layer = LAYER_MAIN;
    cc_layer_reset(ctx->layers + LAYER_OVERLAY);
}

void paint_set_color(PaintContext* ctx, uint32_t color, int fg)
{
    if (fg)
    {
        ctx->fg_color = color;

        if (ctx->tool == TOOL_TEXT && ctx->active_layer == LAYER_OVERLAY)
        {
            CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
            overlay->font_color = color;
            cc_layer_render(overlay);
        }
    }
    else
    {
        ctx->bg_color = color;
    }
}

void paint_set_font(PaintContext* ctx, CcLayer* layer, int font)
{
    const FontEntry* entry = font_table + font;
    layer->font = font;

    if (!stbtt_InitFont(&layer->font_info, entry->data, 0))
    {
        layer->font = -1;
        fprintf(stderr, "failed to load font\n");
    }
    cc_layer_render(layer);
}

int paint_is_editing_text(PaintContext* ctx)
{
    return ctx->tool == TOOL_TEXT && ctx->active_layer == LAYER_OVERLAY;
}

