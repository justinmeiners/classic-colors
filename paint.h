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

#ifndef PAINT_H
#define PAINT_H

#include "layer.h"
#include "undo_queue.h"

/* no Xlib allowed here */


/*
TODO:

- [ ] fix window lifetime
- [ ] zoom in/out scrollbar coordinates/center point
- [ ] copy paste/files undo issue

- [x] use widechar on text layers. XmTextGetStringWcs
- [x] use glyph functions for lookup instead of codepoint (stb_truetype header)
- [x] only undo when active layer is main
- [x] review workflow for including pieces of other images
      copy, open new image, paste
- [x] adjust eraser sizes to be bigger (and more extreme)
- [x] selecting out of bounds segfault
- [x] optimize font rendering buffer stuff
- [x] draggable text
- [x] dotted lines for selection box.
- [x] Zoom. Figure out box and blit/composite into destination.
- [x] switch tool then stop selection box.
- [x] draw ellipse
- [x] crop to selection
- [x] About window. (name author, etc)
- [x] composite mode for inverting colors below it
- [x] select copy/blend mode
- [x] fix magnifier
- [x] image save/load color shift
- [x] word wrap and alignment
- [x] x11 find out color format.
  Either pick a format we want, or in the composite step swap bytes.
- [x] Cut/Copy/Paste with clipboard (last chapter of motif book)
  Consider not integrating with OS.
- [x] open filename provided by argv
- [x] spray can timer
- [x] cleanup shm (at all costs)
- [x] new should clear open file path.
- [x] open/save should use open file path.
- [x] icon
- [x] font box life cycle
- [x] clear text when start new box.
- [x] resizing layers should update text.
- [x] cache stbtt fonts
- [x] window resize performance
- [x] undo checkpoints
- [x] initial text blit is wrong!!
- [x] don't bounds check square brush
- [x] type in color RGB

FUTURE: 

- [x] compress undo/redo patches
- [x] text tool
- [x] tool icons (no icons for optioos)
- [ ] run unit test in debug
- [ ] redraw eyedropper

- polygon tool. 
- polyogn select tool. (both need point in polygon tests.)

*/


typedef enum
{
    TOOL_PENCIL = 0,
    TOOL_BRUSH,
    TOOL_PAINT_BUCKET,
    TOOL_SPRAY_CAN,
    TOOL_MAGNIFIER,
    TOOL_EYE_DROPPER,
    TOOL_LINE,
    TOOL_TEXT,
    TOOL_RECTANGLE,
    TOOL_ELLIPSE,
    TOOL_ERASER,
    TOOL_POLYGON,
    TOOL_SELECT_RECTANGLE,
    TOOL_COUNT,
} PaintTool;

typedef enum
{
    SELECT_IGNORE_BG = 0,
    SELECT_KEEP_BG,
} SelectMode;


typedef enum
{
    SHAPE_FILL = 1 << 0,
    SHAPE_STROKE = 1 << 1,
} ShapeFlags;

typedef enum
{
    BUCKET_GLOBAL = 0,
    BUCKET_CONTIGUOUS,
} BucketMode;


#define OS_PATH_MAX 2048

enum {
    LAYER_COMPOSITE,
    LAYER_INTERMEDIATE,

    LAYER_MAIN,
    LAYER_OVERLAY,
    LAYER_COUNT
};

typedef struct
{
    Layer layers[LAYER_COUNT];
    int active_layer;

    PaintTool tool;
    PaintTool previous_tool;

    ShapeFlags shape_flags;
    SelectMode select_mode;
    BucketMode bucket_mode;

    Viewport viewport;

    int max_undo;

    int zoom_level;
    int line_width;
    int brush_width;
    int eraser_width;

    int mouse_button;
    int tool_x;
    int tool_y;

    int tool_min_x;
    int tool_min_y;

    int tool_max_x;
    int tool_max_y;

    int tool_force_align;

    int request_tool_timer;

    int line_x;
    int line_y;

    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t view_bg_color;

    size_t paste_board_size;
    unsigned char* paste_board_data;

    CcCoord* polygon_points;
    int polygon_count;
    int polygon_capacity;

    UndoQueue undo;

    char open_file_path[OS_PATH_MAX];
} PaintContext;

void paint_undo(PaintContext* ctx);
void paint_redo(PaintContext* ctx);

const char* paint_file_path(PaintContext* ctx);
int paint_open_file(PaintContext* ctx, const char* path, const char** error_message);
int paint_save_file_as(PaintContext* ctx, const char* path);
int paint_save_file(PaintContext* ctx);

int paint_init(PaintContext* ctx);

void paint_invert_colors(PaintContext* ctx);
void paint_flip(PaintContext* ctx, int horiz);
void paint_rotate_90(PaintContext* ctx, int repeat);
void paint_rotate_angle(PaintContext* ctx, double angle);

void paint_stretch(PaintContext* ctx, int w, int h, int w_angle, int h_angle);
void paint_clear(PaintContext* ctx);
void paint_resize(PaintContext* ctx, int new_w, int new_h);

void paint_set_tool(PaintContext* ctx, PaintTool tool);
void paint_tool_down(PaintContext* ctx, int x, int y, int button);
void paint_tool_move(PaintContext* ctx, int x, int y);
void paint_tool_update(PaintContext* ctx);
void paint_tool_up(PaintContext* ctx, int x, int y, int button);

int paint_w(const PaintContext* ctx);
int paint_h(const PaintContext* ctx);

void paint_crop(PaintContext* ctx);

void paint_composite(PaintContext* ctx);

void paint_copy(PaintContext* ctx);
void paint_cut(PaintContext* ctx);
void paint_paste(PaintContext* ctx);

void paint_select_all(PaintContext* ctx);
void paint_select(PaintContext* ctx, int x, int y, int w, int h);
void paint_select_clear(PaintContext* ctx);

void paint_set_color(PaintContext* ctx, uint32_t color, int fg);
void paint_set_font(PaintContext* ctx, Layer* layer, int font);

const char* paint_font_name(int index);
int paint_font_count();


#endif
