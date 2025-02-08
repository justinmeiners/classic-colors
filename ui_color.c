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


#include "ui.h"

#include <Xm/ColorS.h>


static
Widget g_color_picker = NULL;

static
int g_edit_fg = 0;

static
char* rgb_txt_path = NULL;

#define XCOLOR_NAME_MAX (4 + 6 + 4)

// https://tronche.com/gui/x/xlib/color/structures.html
static
CcPixel xcolor_to_color_(XColor c)
{
    int32_t comps_16[] = {
        c.red,
        c.green,
        c.blue,
        UINT16_MAX
    };

    uint8_t comps_8[4];
    for (int i = 0; i < 4; ++i)
    {
        comps_8[i] = (comps_16[i] * 255) / UINT16_MAX;
    }
    return cc_color_pack(comps_8);
}

static
void color_to_xname_(char* out, CcPixel color)
{
    uint8_t comp[4];
    cc_color_unpack(color, comp);
    snprintf(out, XCOLOR_NAME_MAX, "rgb:%02X/%02X/%02X", comp[0], comp[1], comp[2]);
}

static
int find_rgb_txt_path(char* buffer, size_t max)
{
    //.rgb files were removed?
    // https://forums.freebsd.org/threads/where-did-rgb-txt-go.53802/
    const char* to_try[] = {
        "share/X11/rgb.txt",
        "etc/X11/rgb.txt",
        "lib/X11/rgb.txt"
    };

    size_t n = sizeof(to_try) / sizeof(*to_try);

    for (int i = 0; i < n; ++i)
    {
        snprintf(buffer, max, "%s/%s", X11_PATH_PREFIX, to_try[i]);

        FILE* f = fopen(buffer, "r");

        if (f)
        {
            fclose(f);
            return 1;
        }
    }
    return 0;
}

void ui_set_color(Widget window, CcPixel color, int fg)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];

    PaintContext* ctx = &g_paint_ctx;
    paint_set_color(ctx, color, fg);

    Widget widget = XtNameToWidget(window, fg ? "*fg_color" : "*bg_color");
    Display* display = XtDisplay(window);
    Colormap screen_colormap = DefaultColormap(display, DefaultScreen(display));

    n = 0;
    Pixmap old_pixmap;
    XtSetArg(args[n], XmNlabelPixmap, &old_pixmap); ++n;
    XtGetValues(widget, args, n);

    char name[XCOLOR_NAME_MAX];
    color_to_xname_(name, color);

    // ParseColor is always RED GREEN BLUE
    // https://linux.die.net/man/3/xparsecolor
    XColor xcolor;
    XParseColor(display, screen_colormap, name, &xcolor);
    XAllocColor(display, screen_colormap, &xcolor);

    Pixmap pixmap = XmGetPixmap(XtScreen(widget), "color_well", xcolor.pixel, xcolor.pixel);

    n = 0;
    XtSetArg(args[n], XmNlabelType, XmPIXMAP); ++n;
    XtSetArg(args[n], XmNlabelPixmap, pixmap); ++n;
    XtSetValues(widget, args, n);

    // release old pixmap (ref counted)
    XmDestroyPixmap(XtScreen(widget), old_pixmap);

    if (g_color_picker && fg == g_edit_fg)
    {
        char color_name[XCOLOR_NAME_MAX];
        color_to_xname_(color_name, color);

        n = 0;
        XtSetArg(args[n], XmNcolorName, color_name); n++;
        Widget color_selector = XtNameToWidget(g_color_picker, "*color_selector");
        XtSetValues(color_selector, args, n);
    }

    ui_refresh_drawing(0);
}


static void color_picker_event_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];

    XmAnyCallbackStruct* cbs = (XmAnyCallbackStruct*)call_data;

    if (cbs->reason == XmCR_OK)
    {
        String color_name;
        Widget selector = XtNameToWidget(g_color_picker, "*color_selector");

        n = 0;
        XtSetArg(args[n], XmNcolorName, &color_name); ++n;
        XtGetValues(selector, args, n);

        Display* display = XtDisplay(widget);
        Colormap screen_colormap = DefaultColormap(display, DefaultScreen(display));

        XColor xcolor;
        XParseColor(display, screen_colormap, color_name, &xcolor);
        ui_set_color(g_main_w, xcolor_to_color_(xcolor), g_edit_fg);
    }
    else if (cbs->reason == XmCR_CANCEL)
    {
        XtDestroyWidget(g_color_picker);
    }

    //Colormap map = DefaultColormapOfScreen(XtScreen(main_w));

    //XParseColor(XtDisplay(main_w), map, color_name, &(holder.the_color));
    //XAllocColor(XtDisplay(main_w), map, &(holder.the_color));
    
    g_color_picker = NULL;
}

static uint8_t parse_color_component(const char *str)
{
    return (uint8_t)MAX(MIN(atoi(str), 255), 0);
}

static
void rgb_changed_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    if (!g_color_picker) return;

    int n = 0;
    Arg args[UI_ARGS_MAX];

    Widget red = XtNameToWidget(g_color_picker, "*red");
    Widget green = XtNameToWidget(g_color_picker, "*green");
    Widget blue = XtNameToWidget(g_color_picker, "*blue");

    char* red_str = XmTextFieldGetString(red);
    char* green_str = XmTextFieldGetString(green);
    char* blue_str = XmTextFieldGetString(blue);

    uint8_t comps[4] = {
        parse_color_component(red_str),
        parse_color_component(green_str),
        parse_color_component(blue_str),
        255
    };
    CcPixel color = cc_color_pack(comps);

    char name[XCOLOR_NAME_MAX];
    color_to_xname_(name, color);
    n = 0;
    XtSetArg(args[n], XmNcolorName, name); n++;
    Widget color_selector = XtNameToWidget(g_color_picker, "*color_selector");
    XtSetValues(color_selector, args, n);
}


#define SCRATCH_MAX 1024

static Widget setup_color_picker_(Widget parent, CcPixel color)
{
    if (!rgb_txt_path)
    {
        rgb_txt_path = malloc(sizeof(char) * OS_PATH_MAX);
        if (find_rgb_txt_path(rgb_txt_path, OS_PATH_MAX))
        {
            if (DEBUG_LOG)
            {
                printf("found x11 rgb.txt %s\n", rgb_txt_path);
            }
        }
    }

    int n = 0;
    Arg args[UI_ARGS_MAX];

    Widget dialog = XmCreateDialogShell(parent, "Select Colors", args, n);

    n = 0;
    XtSetArg(args[n], XmNnoResize, 1); ++n;
    Widget box = XmCreateMessageBox(dialog, "message_box", args, n);
    XtAddCallback(box, XmNcancelCallback, color_picker_event_, NULL);
    XtAddCallback(box, XmNokCallback, color_picker_event_, NULL);
    XtUnmanageChild(XtNameToWidget(box, "Help"));

    Arg pane_args[] = {
        { XmNorientation, XmHORIZONTAL },
        { XmNseparatorOn, 1 },
    };

    Widget split_pane = XtCreateWidget("split_pane", xmPanedWidgetClass, box, pane_args, XtNumber(pane_args));


    char color_name[XCOLOR_NAME_MAX];
    color_to_xname_(color_name, color);

    n = 0;
    XtSetArg(args[n], XmNpaneMaximum, 240); ++n;
    XtSetArg(args[n], XmNshowSash, 0); ++n;
    XtSetArg(args[n], XmNcolorName, color_name); ++n;

    if (rgb_txt_path)
    {
        XtSetArg(args[n], XmNrgbFile, rgb_txt_path); ++n;
    }

    Widget color_selector = XtCreateManagedWidget("color_selector",
					    xmColorSelectorWidgetClass, 
					    split_pane,
                        args,
                        n);

 
    XtManageChild(color_selector);


    // extended column
    Widget rowcol_v = XmCreateRowColumn(split_pane, "row_v", NULL, 0);
    uint8_t color_comps[4];
    cc_color_unpack(color, color_comps);

    char scratch_buffer[SCRATCH_MAX];
    {
        snprintf(scratch_buffer, SCRATCH_MAX, "%d", color_comps[0]);

        Widget red_row = XtVaCreateWidget("red_row", xmRowColumnWidgetClass, rowcol_v, XmNorientation, XmHORIZONTAL, NULL);
        Widget red_label = XmCreateLabel (red_row, "Red:", NULL, 0);
        XtManageChild (red_label);

        Widget red = XmCreateTextField (red_row, "red", NULL, 0);
        XmTextSetString(red, scratch_buffer);
        XtAddCallback(red, XmNvalueChangedCallback, rgb_changed_, NULL);
        XtManageChild(red);

        XtManageChild(red_row);
    }
    {
        snprintf(scratch_buffer, SCRATCH_MAX, "%d", color_comps[1]);

        Widget green_row = XtVaCreateWidget("green_row", xmRowColumnWidgetClass, rowcol_v, XmNorientation, XmHORIZONTAL, NULL);
        Widget green_label = XmCreateLabel (green_row, "Green:", NULL, 0);
        XtManageChild (green_label);

        Widget green = XmCreateTextField (green_row, "green", NULL, 0);
        XmTextSetString(green, scratch_buffer);
        XtAddCallback(green, XmNvalueChangedCallback, rgb_changed_, NULL);
        XtManageChild(green);

        XtManageChild(green_row);
    }
    {
        snprintf(scratch_buffer, SCRATCH_MAX, "%d", color_comps[1]);

        Widget blue_row = XtVaCreateWidget("blue_row", xmRowColumnWidgetClass, rowcol_v, XmNorientation, XmHORIZONTAL, NULL);
        Widget blue_label = XmCreateLabel (blue_row, "Blue:", NULL, 0);
        XtManageChild (blue_label);

        Widget blue = XmCreateTextField (blue_row, "blue", NULL, 0);
        XmTextSetString(blue, scratch_buffer);
        XtAddCallback(blue, XmNvalueChangedCallback, rgb_changed_, NULL);
        XtManageChild(blue);

        XtManageChild(blue_row);
    }

    XtManageChild(rowcol_v);

    XtManageChild(split_pane);
    XtManageChild(box);

    return dialog;
}



const char* g_default_colors[] = {
    /* grays */
    "rgb:00/00/00",
    "rgb:44/44/44",
    "rgb:80/80/80",
    "rgb:BB/BB/BB",
    "rgb:DD/DD/DD",
    "rgb:FF/FF/FF",

    /* selections */
    "rgb:FF/80/80",
    "rgb:EC/AA/9F",
    "rgb:80/FF/80",
    "rgb:80/80/FF",
    "rgb:7C/05/F2",

    "rgb:FF/66/00",
    "rgb:70/5E/78",
    "rgb:3B/82/BF",
    "rgb:B3/DA/F2",
    "rgb:F2/05/9F",
    "rgb:D9/C2/AD",
    "rgb:66/73/67",
    "rgb:F2/CF/1D",
    "rgb:8D/A6/98",
    "rgb:CA/D9/A9",



    /* base colors*/
    "rgb:80/00/00",
    "rgb:BB/00/00",
    "rgb:FF/00/00",

    "rgb:80/80/00",
    "rgb:BB/BB/00",
    "rgb:FF/FF/00",

    "rgb:80/40/00",
    "rgb:BB/5D/00",
    "rgb:FF/C0/00",

    "rgb:00/80/00",
    "rgb:00/BB/00",
    "rgb:00/FF/00",

    "rgb:00/80/80",
    "rgb:00/BB/BB",
    "rgb:00/FF/FF",

    "rgb:00/00/80",
    "rgb:00/00/BB",
    "rgb:00/00/FF",

    "rgb:80/00/80",
    "rgb:BB/00/BB",
    "rgb:FF/00/FF",
};

enum {
    DEFAULT_COLOR_COUNT  = sizeof(g_default_colors) / sizeof(const char*),
};

static
void test_colors_(Display *display)
{
    printf("testing colors\n");
    Colormap screen_colormap = DefaultColormap(display, DefaultScreen(display));
    XColor xcolor;
    XParseColor(display, screen_colormap, "rgb:00/FF/00", &xcolor);
    assert(xcolor.green == UINT16_MAX);

    CcPixel p = xcolor_to_color_(xcolor);
    assert(p == COLOR_GREEN);
}


static
void select_color_well_(Widget widget, size_t color_index, int fg)
{
    Display* display = XtDisplay(widget);
    if (DEBUG_LOG) {
        test_colors_(display);
    }

    Colormap screen_colormap = DefaultColormap(display, DefaultScreen(display));
    XColor xcolor;
    XParseColor(display, screen_colormap, g_default_colors[color_index], &xcolor);
    CcPixel color = xcolor_to_color_(xcolor);
    ui_set_color(g_main_w, color, fg);
}

static
void color_clicked_(Widget widget, void* client_data, XEvent* event, Boolean* continue_dispatch)
{
    // handle right click
    switch (event->type)
    {
    case ButtonPress:
        if (event->xbutton.button == 3)
        {
            select_color_well_(widget, (size_t)client_data, 0);
            *continue_dispatch = 0;
        }
        break;
    }
}

// Double click broken in Motif? https://bugzilla.redhat.com/show_bug.cgi?id=184143
static void color_cell_activate_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XmPushButtonCallbackStruct* cbs = (XmPushButtonCallbackStruct*)call_data;
    select_color_well_(widget, (size_t)client_data, 1);
}

static void color_swap_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    PaintContext* ctx = &g_paint_ctx;

    CcPixel temp = ctx->bg_color;
    ui_set_color(g_main_w, ctx->fg_color, 0);
    ui_set_color(g_main_w, temp, 1);
}

static void color_change_activate_(Widget widget, XtPointer client_data, XtPointer call_data)
{
     int n = 0;
     Arg args[UI_ARGS_MAX];

     PaintContext* ctx = &g_paint_ctx;
     size_t index = (size_t)client_data;

     CcPixel color;

     switch (index)
     {
     case 0:
         color = ctx->fg_color;
         g_edit_fg = 1;
         break;
     case 1:
         color = ctx->bg_color;
         g_edit_fg = 0;
         break;
     }

     if (!g_color_picker)
     {
        g_color_picker = setup_color_picker_(g_main_w, color);
     }

     XtManageChild(g_color_picker);
}

#define BITMAP_ALIGN_BYTES 8

#define COLOR_WELL_W 32
#define COLOR_WELL_STRIDE (ALIGN_UP(COLOR_WELL_W, BITMAP_ALIGN_BYTES * 8) / 8)
#define COLOR_WELL_SMALL_W 16
#define COLOR_WELL_SMALL_STRIDE (ALIGN_UP(COLOR_WELL_SMALL_W, BITMAP_ALIGN_BYTES * 8) / 8)

static
char color_well_buffer_[COLOR_WELL_W * COLOR_WELL_STRIDE];
static
char color_well_buffer_small_[COLOR_WELL_SMALL_W * COLOR_WELL_SMALL_STRIDE];

static
void install_color_images_(Widget parent)
{
    // We previously used background color for these wells.
    // But when skinning motif it breaks.

    // use a solid white image and recolor it.
    memset(color_well_buffer_, 0xFF, sizeof(color_well_buffer_));
    memset(color_well_buffer_small_, 0xFF, sizeof(color_well_buffer_small_));

    Display* display = XtDisplay(parent);

    const size_t bits_per_bytes = 8;
    XImage *color_well = XCreateImage(display, DefaultVisual(display, DefaultScreen(display)), 1, XYBitmap, 0, color_well_buffer_,
                                      COLOR_WELL_W, COLOR_WELL_W, bits_per_bytes, BITMAP_ALIGN_BYTES);

    XImage *color_well_small = XCreateImage(display, DefaultVisual(display, DefaultScreen(display)), 1, XYBitmap, 0, color_well_buffer_small_,
                                            COLOR_WELL_SMALL_W, COLOR_WELL_SMALL_W, bits_per_bytes, BITMAP_ALIGN_BYTES);

    if (!color_well || !color_well_small) {
        fprintf(stderr, "failed to create color image\n");
        exit(1);
    }

    XmInstallImage(color_well, "color_well");
    XmInstallImage(color_well_small, "color_well_small");
}

Widget ui_setup_command_area(Widget parent)
{
    install_color_images_(parent);

    int n = 0;
    Arg args[UI_ARGS_MAX];

    Widget command_area = XmCreateRowColumn(parent, "command", NULL, 0);

    Widget all_split = XtVaCreateWidget(
            "all_split",
            xmRowColumnWidgetClass,
            command_area,
            XmNorientation, XmHORIZONTAL,
            NULL);

    char name[UI_NAME_MAX];
    XmString empty = XmStringCreateLocalized("");

    n = 0;
    XtSetArg(args[n], XmNlabelString, empty); n++;

    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    XtSetArg(args[n], XmNmarginHeight, 0); n++;
    XtSetArg(args[n], XmNmultiClick, XmMULTICLICK_KEEP); n++;
    XtSetArg(args[n], XmNshadowThickness, 1); n++;
    XtSetArg(args[n], XmNhighlightThickness, 0); n++;

    Widget selected_frame = XtCreateWidget(
            "selected_frame",
            xmFrameWidgetClass,
            all_split,
            NULL,
            0);

    Widget selected_split = XtVaCreateWidget(
            "selected_split",
            xmRowColumnWidgetClass,
            selected_frame,
            XmNorientation, XmHORIZONTAL,
            XmNspacing, 4,
            NULL);

    Widget fg_color = XtCreateManagedWidget(
            "fg_color",
            xmPushButtonWidgetClass,
            selected_split,
            args,
            n);
    XtAddCallback(fg_color, XmNactivateCallback, color_change_activate_, 0);

    Widget bg_color = XtCreateManagedWidget(
            "bg_color",
            xmPushButtonWidgetClass,
            selected_split,
            args,
            n);
    XtAddCallback(bg_color, XmNactivateCallback, color_change_activate_, (XtPointer)1);

    Widget swap = XtVaCreateManagedWidget(
            "]",
            xmPushButtonWidgetClass,
            selected_split,
            XmNshadowThickness, 0,
            XmNhighlightThickness, 0,
            NULL);

    XtAddCallback(swap, XmNactivateCallback, color_swap_, NULL);

    XtManageChild(selected_split);
    XtManageChild(selected_frame);

    Widget color_row = XtVaCreateWidget(
            "color_row",
            xmRowColumnWidgetClass,
            all_split,
            XmNorientation, XmHORIZONTAL,
            XmNspacing, 0,
            XmNpacking, XmPACK_COLUMN,
            XmNnumColumns, 2,
            NULL);

    n = 0;
    XtSetArg(args[n], XmNlabelString, empty); n++;
    XtSetArg(args[n], XmNindicatorOn, XmINDICATOR_NONE); n++;
    XtSetArg(args[n], XmNmarginWidth, 0); n++;
    XtSetArg(args[n], XmNmarginHeight, 0); n++;
    XtSetArg(args[n], XmNmultiClick, XmMULTICLICK_KEEP); n++;
    XtSetArg(args[n], XmNshadowThickness, 1); n++;

    for (int i = 0; i < DEFAULT_COLOR_COUNT; ++i)
    {
        snprintf(name, UI_NAME_MAX, "color_btn_%d", i);

        Widget added = XtCreateManagedWidget(
                name,
                xmPushButtonWidgetClass,
                color_row,
                args,
                n);

        XtAddEventHandler(added, ButtonPressMask, False, color_clicked_, (XtPointer)i);
        XtAddCallback(added, XmNactivateCallback, color_cell_activate_, (XtPointer)i);
    }
    XmStringFree(empty);

    Display* display = XtDisplay(parent);
    Colormap screen_colormap = DefaultColormap(display, DefaultScreen(display));

    for (int i = 0; i < DEFAULT_COLOR_COUNT; ++i)
    {
        snprintf(name, UI_NAME_MAX, "color_btn_%d", i);
        Widget cell = XtNameToWidget(color_row, name);

        XColor color;
        XParseColor(display, screen_colormap, g_default_colors[i], &color);
        XAllocColor(display, screen_colormap, &color);

        Pixmap pixmap = XmGetPixmap(XtScreen(parent), "color_well_small", color.pixel, color.pixel);

        n = 0;
        XtSetArg(args[n], XmNlabelType, XmPIXMAP); ++n;
        XtSetArg(args[n], XmNlabelPixmap, pixmap); ++n;
        XtSetValues(cell, args, n);
    }

    XtManageChild(color_row);

    XtVaCreateManagedWidget( "command_message",
            xmTextFieldWidgetClass,
            command_area,
            XmNeditable, False,
            XmNmaxLength, 256,
            NULL);

    XtManageChild(all_split);
    XtManageChild(command_area);

    return command_area;
}


