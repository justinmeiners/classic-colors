/* 
 * Copyright (c) 2021 Justin Meiners
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ui.h"

#include <Xm/List.h>
#include <Xm/ToggleB.h> 
#include <Xm/ToggleBG.h> 

#include <X11/xpm.h>

// http://manpages.ubuntu.com/manpages/precise/man1/pixmap.1.html
// https://www.motif.ics.com/forum/location-xpm-motif-21
#include "icons/icon_select_rectangle.xpm"
#include "icons/icon_eraser.xpm"
#include "icons/icon_paint_bucket.xpm"
#include "icons/icon_eye_dropper.xpm"
#include "icons/icon_magnifier.xpm"
#include "icons/icon_pencil.xpm"
#include "icons/icon_paint_brush.xpm"
#include "icons/icon_line.xpm"
#include "icons/icon_rectangle.xpm"
#include "icons/icon_ellipse.xpm"
#include "icons/icon_airbrush.xpm"
#include "icons/icon_text.xpm"

typedef enum
{
    OPTION_NONE = 0,
    OPTION_LINE_WIDTH,
    OPTION_BRUSH_WIDTH,
    OPTION_ERASER_WIDTH,
    OPTION_SHAPE_FLAGS,
    OPTION_ZOOM_LEVEL,
    OPTION_SELECT_MODE,
    OPTION_BUCKET_MODE,
    OPTION_COUNT,
} ToolOption;

static void set_option_(PaintContext* ctx, ToolOption option, int value)
{
    switch (option)
    {
        case OPTION_LINE_WIDTH:
            ctx->line_width = value;
            break;
        case OPTION_BRUSH_WIDTH:
            ctx->brush_width = value;
            break;
        case OPTION_ERASER_WIDTH:
            ctx->eraser_width = value;
            break;
        case OPTION_SHAPE_FLAGS:
            ctx->shape_flags = (ShapeFlags)value;
            break;
        case OPTION_ZOOM_LEVEL:
            ctx->zoom_level = value;
            break;
        case OPTION_SELECT_MODE:
            ctx->select_mode = (SelectMode)value;
            break;
        case OPTION_BUCKET_MODE:
            ctx->bucket_mode = (BucketMode)value;
            break;
    }
}

static int get_option_(const PaintContext* ctx, ToolOption option)
{
    switch (option)
    {
        case OPTION_LINE_WIDTH:
            return ctx->line_width;
        case OPTION_BRUSH_WIDTH:
            return ctx->brush_width;
        case OPTION_ERASER_WIDTH:
            return ctx->eraser_width;
        case OPTION_SHAPE_FLAGS:
            return (int)ctx->shape_flags;
        case OPTION_ZOOM_LEVEL:
            return ctx->zoom_level;
        case OPTION_SELECT_MODE:
            return (int)ctx->select_mode;
        case OPTION_BUCKET_MODE:
            return (int)ctx->bucket_mode;
    }

    return -1;
}




Widget g_tool_radio = NULL;

typedef struct
{
    const char* text;
    int value;
} ToolOptionInfo;

ToolOptionInfo g_line_options[] = {
    { "1 px", 1 },
    { "2 px", 2 },
    { "4 px", 4 },
    { "6 px", 6 },
    { "8 px", 8 },
    { "16 px", 16 },
};

enum {
    LINE_OPTION_COUNT = sizeof(g_line_options) / sizeof(ToolOptionInfo)
};


ToolOptionInfo g_brush_options[] = {
    { "1 px", 1 },
    { "4 px", 4 },
    { "6 px", 6 },
    { "12 px", 12 },
    { "16 px", 16 },
    { "32 px", 32 },
};

enum {
    BRUSH_OPTION_COUNT = sizeof(g_brush_options) / sizeof(ToolOptionInfo)
};

ToolOptionInfo g_eraser_options[] = {
    { "4 px", 4 },
    { "8 px", 8 },
    { "16 px", 16 },
    { "32 px", 32 },
    { "64 px", 64 },
    { "96 px", 96 },
};

enum {
    ERASER_OPTION_COUNT = sizeof(g_eraser_options) / sizeof(ToolOptionInfo)
};

ToolOptionInfo g_shape_options[] = {
    { "stroke", SHAPE_STROKE },
    { "fill", SHAPE_FILL },
    { "shape + stroke", SHAPE_STROKE | SHAPE_FILL  },
};

enum {
    SHAPE_OPTION_COUNT = sizeof(g_shape_options) / sizeof(ToolOptionInfo)
};

ToolOptionInfo g_zoom_options[] = {
    { "1x", 1 },
    { "2x", 2 },
    { "4x", 4 },
    { "8x", 8 },
    { "16x", 16 },
    { "32x", 32 },
};

enum {
    ZOOM_OPTION_COUNT = sizeof(g_zoom_options) / sizeof(ToolOptionInfo)
};

ToolOptionInfo g_select_options[] = {
    { "ignore bg", SELECT_IGNORE_BG },
    { "everything", SELECT_KEEP_BG }
};

enum {
    SELECT_OPTION_COUNT = sizeof(g_select_options) / sizeof(ToolOptionInfo)
};


ToolOptionInfo g_bucket_options[] = {
    { "neighbors", BUCKET_CONTIGUOUS },
    { "everywhere", BUCKET_GLOBAL }
};

enum {
    BUCKET_OPTION_COUNT = sizeof(g_bucket_options) / sizeof(ToolOptionInfo)
};


typedef struct
{
    PaintTool tool;
    ToolOption option_set;
    char* text;
    char** pixmap;
    char* help;
} ToolInfo;

static const ToolOptionInfo* get_options_(ToolOption option_set, int* out_count)
{
    switch (option_set)
    {
        case OPTION_LINE_WIDTH:
            *out_count = LINE_OPTION_COUNT;
            return g_line_options;
        case OPTION_BRUSH_WIDTH:
            *out_count = BRUSH_OPTION_COUNT;
            return g_brush_options;
        case OPTION_ERASER_WIDTH:
            *out_count = ERASER_OPTION_COUNT;
            return g_eraser_options;
        case OPTION_SHAPE_FLAGS:
            *out_count = SHAPE_OPTION_COUNT;
            return g_shape_options;
        case OPTION_ZOOM_LEVEL:
            *out_count = ZOOM_OPTION_COUNT;
            return g_zoom_options;
        case OPTION_BUCKET_MODE:
            *out_count = BUCKET_OPTION_COUNT;
            return g_bucket_options;
        case OPTION_SELECT_MODE:
            *out_count = SELECT_OPTION_COUNT;
            return g_select_options;
    }
    *out_count = 0;
    return NULL;
}


ToolInfo g_tools[] = {
    {
        TOOL_BRUSH,
        OPTION_BRUSH_WIDTH,
        "Paint Brush",
        icon_paint_brush,
        "Click and drag to draw brush strokes."
    },
    {
        TOOL_PENCIL,
        OPTION_LINE_WIDTH,
        "Pencil",
        icon_pencil,
        "Click and drag to draw free-form lines."
    },
    { 
        TOOL_ERASER,
        OPTION_ERASER_WIDTH,
        "Eraser",
        icon_eraser,
        "Click and drag to erase portions of the image."
    },
    {
        TOOL_EYE_DROPPER, 
        OPTION_NONE,
        "Eyedropper",
        icon_eye_dropper,
        "Click to select foreground or background color from a pixel in the image."
    },
    {
        TOOL_SPRAY_CAN,
        OPTION_BRUSH_WIDTH,
        "Airbrush", 
        icon_airbrush,
        "Click and drag to spray with the airbrush."
    },
    {
        TOOL_PAINT_BUCKET,
        OPTION_BUCKET_MODE,
        "Paint Bucket",
        icon_paint_bucket,
        "Click to fill a connected region (neighbors) or to replace a color in the entire image (everywhere)."
    },
    { 
        TOOL_SELECT_RECTANGLE,
        OPTION_SELECT_MODE,
        "Select Area",
        icon_select_rectangle,
        "Click and drag to select a region. Click and drag the selection to move it."
    },
    {
        TOOL_MAGNIFIER, 
        OPTION_ZOOM_LEVEL,
        "Magnifier",
        icon_magnifier,
        "Left-click to magnify. Right-click to reset. Hold left-click to see preview of viewport."
    },
    {
        TOOL_TEXT,
        OPTION_NONE,
        "Test",
        icon_text,
        "Click and drag to create a textbox. Configure text and font in the text window."
    },
    { 
        TOOL_LINE,
        OPTION_LINE_WIDTH,
        "Line",
        icon_line,
        "Click and drag a straight line from start to end. Hold shift to snap to angles."
    },
    { 
        TOOL_RECTANGLE,
        OPTION_SHAPE_FLAGS,
        "Rectangle",
        icon_rectangle,
        "Click and drag to make a rectangle from start to end. Hold shift to snap to square."
    },
    { 
        TOOL_ELLIPSE,
        OPTION_SHAPE_FLAGS,
        "Ellipse",
        icon_ellipse,
        "Click and drag to draw an ellipse from start to end. Hold shift to snap to circle."
    }

};

const int g_tools_count = sizeof(g_tools) / sizeof(ToolInfo);

static void update_tool_options_(Widget option_list, ToolOption option_set)
{
    PaintContext* ctx = &g_paint_ctx;
    int n = 0;
    Arg args[UI_ARGS_MAX];

    int count;
    const ToolOptionInfo* set = get_options_(option_set, &count);

    int selected_value = get_option_(ctx, option_set);
    int selected_item = -1;
    XmStringTable options = XtMalloc(count * sizeof(XmString));
    for (int i = 0; i < count; ++i)
    {
        if (set[i].value == selected_value)
            selected_item = i;
        options[i] = XmStringCreateLocalized(set[i].text);
    }

    n = 0;
    XtSetArg(args[n], XmNvisibleItemCount, count); n++;
    XtSetArg(args[n], XmNitemCount, count); n++;
    XtSetArg(args[n], XmNitems, options); n++;
 
    XtSetValues(option_list, args, n);

    if (selected_item != -1)
        XmListSelectPos(option_list, selected_item + 1, 0);

	for (int i = 0; i < count; ++i) XmStringFree(options[i]);
	XtFree(options);
}

static void change_tool_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    // todo: double calll
    PaintContext* ctx = &g_paint_ctx;
    size_t selection = (size_t)client_data;
    XmToggleButtonCallbackStruct* cbs = (XmToggleButtonCallbackStruct*)call_data;

    if (cbs->set != XmSET) return;

    const ToolInfo* tool = g_tools + selection;

    paint_set_tool(ctx, tool->tool);

    if (tool->tool != TOOL_TEXT) ui_hide_text_dialog();

    Widget options_list = XtNameToWidget(g_main_w, "*tool_options");
    update_tool_options_(options_list, tool->option_set);

    Widget command_message = XtNameToWidget(g_main_w, "*command_message");
    XmTextFieldSetString(command_message, tool->help);
    ui_refresh_drawing(0);
}

static void change_tool_option_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    PaintContext* ctx = &g_paint_ctx;
    XmListCallbackStruct *cbs = (XmListCallbackStruct *) call_data;

    size_t option_index = cbs->item_position - 1;

    // linear search for which tool
    int selection = 0;
    while (selection < g_tools_count)
    {
        if (g_tools[selection].tool == ctx->tool) break;
        ++selection;
    }

    assert(selection >= 0 && selection < TOOL_COUNT);

    const ToolInfo* tool = g_tools + selection;

    int count;
    const ToolOptionInfo* set = get_options_(tool->option_set, &count);

    if (set)
    {
        int value = set[option_index].value;

        if (DEBUG_LOG)
        {
            printf("changing setting: %d %d %d\n", tool->option_set, option_index, value);
        }
        set_option_(ctx, tool->option_set, value);
    }
}

void ui_refresh_tool(void)
{
    PaintTool tool = g_paint_ctx.tool;

    if (tool != TOOL_TEXT)
    {
        ui_hide_text_dialog();
    }

    char name[UI_NAME_MAX];
    snprintf(name, UI_NAME_MAX, "tool_%d", tool);
    Widget widget = XtNameToWidget(g_tool_radio, name);

    if (widget)
    {
        XmToggleButtonSetState(widget, 1, 1);
    }
}


Widget ui_setup_tool_area(Widget parent)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];

    XtSetArg(args[n], XmNorientation, XmVERTICAL); ++n;
    XtSetArg(args[n], XmNpaneMaximum, 240); ++n;
    XtSetArg(args[n], XmNshowSash, 0); ++n;
    Widget tool_option_column = XmCreateRowColumn(parent, "tool_option_column", args, n);

    Pixel fg, bg;
    XtVaGetValues (tool_option_column, XmNforeground, &fg, XmNbackground, &bg, NULL);

    n = 0;
    XtSetArg(args[n], XmNnumColumns, 2); ++n;
    XtSetArg(args[n], XmNpacking, XmPACK_COLUMN);


    Widget radio_box = XtVaCreateWidget(
            "tool_column",
            xmRowColumnWidgetClass,
            tool_option_column,
            XmNspacing, 0,
            XmNpacking, XmPACK_COLUMN,
            XmNnumColumns, 2,
            XmNisHomogeneous, 1,
            //XmNbackground, WhitePixel(XtDisplay(parent), 0),
            XmNradioBehavior, 1,
            NULL);

    char name[UI_NAME_MAX];
    for (size_t i = 0; i < g_tools_count; ++i)
    {
        snprintf(name, UI_NAME_MAX, "tool_%d", g_tools[i].tool);

        n = 0;
        if (g_tools[i].pixmap)
        {
            XtSetArg(args[n], XmNlabelType, XmPIXMAP); ++n;
            Pixmap p;
            XpmCreatePixmapFromData(XtDisplay(parent), RootWindowOfScreen(XtScreen(parent)), g_tools[i].pixmap, &p, NULL, NULL);
            XtSetArg(args[n], XmNlabelPixmap, p); ++n;
        }
        else
        {
            XmString text = XmStringCreateLocalized(g_tools[i].text);
            XtSetArg(args[n], XmNlabelString, text); ++n;
            XmStringFree(text);
        }
        XtSetArg(args[n], XmNvisibleWhenOff, 0); ++n;
        XtSetArg(args[n], XmNindicatorOn, XmINDICATOR_CHECK); ++n;
        XtSetArg(args[n], XmNshadowThickness, 2); ++n;
        XtSetArg(args[n], XmNbackground, WhitePixel(XtDisplay(parent), 0)); ++n;
        XtSetArg(args[n], XmNhighlightThickness, 0); ++n;


        Widget added = XtCreateManagedWidget(
                name,
                xmToggleButtonWidgetClass,
                radio_box,
                args,
                n);
        XtAddCallback(added, XmNvalueChangedCallback, change_tool_, (XtPointer)i);
    }

    g_tool_radio = radio_box;
    XtManageChild(radio_box);

    n = 0;
    XtSetArg(args[n], XmNselectionPolicy, XmSINGLE_SELECT); n++;
    Widget options_list = XmCreateList(tool_option_column, "tool_options", args, n);
    
    // called by mouse
    XtAddCallback(options_list, XmNsingleSelectionCallback, change_tool_option_, NULL);
    // called by arrows + enter on keyboard
    XtAddCallback(options_list, XmNdefaultActionCallback, change_tool_option_, NULL);
    update_tool_options_(options_list, OPTION_LINE_WIDTH);
    XtManageChild(options_list);

    XtManageChild(tool_option_column);
    return tool_option_column;
}


