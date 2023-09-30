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


Widget g_color_picker = NULL;
int g_edit_fg = 0;
char* rgb_txt_path = NULL;


#define COLOR_NAME_MAX 10

static
uint32_t xcolor_to_color(const XColor* c)
{
    int comps[] = {
        c->red ,
        c->green,
        c->blue,
        65535
    };

    for (int i = 0; i < 4; ++i)
    {
        comps[i] = (comps[i] * 256) / 65536;
    }
    return cc_color_pack(comps);
}


static
void format_color_hex_string(char* out, uint32_t color)
{
    int comp[4];
    cc_color_unpack(color, comp);
    snprintf(out, COLOR_NAME_MAX, "#%02X%02X%02X", comp[0], comp[1], comp[2]);
}

/*
static
uint32_t parse_color_string(const char* str)
{
    return (((uint32_t)strtol(str + 1, NULL, 16)) << 8) | 0x000000FF;
}
*/

static
int find_rgb_txt_path(char* buffer, size_t max)
{
    //.rgb files were removed?
    // https://forums.freebsd.org/threads/where-did-rgb-txt-go.53802/
    const char* to_try[] = {
        //File will be be based upon the motif main directory libaries.
        "share/X11/rgb.txt",
        "etc/X11/rgb.txt",
        "lib/X11/rgb.txt",
	"/opt/X11/share/X11/rbg.txt"
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

void ui_set_color(Widget window, uint32_t color, int fg)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];

    PaintContext* ctx = &g_paint_ctx;

    paint_set_color(ctx, color, fg);

    Widget widget = XtNameToWidget(window, fg ? "*fg_color" : "*bg_color");
    Display* display = XtDisplay(window);
    Colormap screen_colormap = DefaultColormap(display, DefaultScreen(display));

    char name[COLOR_NAME_MAX];
    format_color_hex_string(name, color);

    // ParseColor is always RED GREEN BLUE
    // https://linux.die.net/man/3/xparsecolor
    XColor xcolor;
    XParseColor(display, screen_colormap, name, &xcolor);
    XAllocColor(display, screen_colormap, &xcolor);

    n = 0;
    XtSetArg(args[n], XmNbackground, xcolor.pixel); n++;
    XtSetValues(widget, args, n);

    if (g_color_picker && fg == g_edit_fg)
    {
        char color_name[COLOR_NAME_MAX];
        format_color_hex_string(color_name, color);

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
        ui_set_color(g_main_w, xcolor_to_color(&xcolor), g_edit_fg);
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

static void rgb_changed_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    if (!g_color_picker) return;

    int n = 0;
    Arg args[UI_ARGS_MAX];

    Widget red = XtNameToWidget(g_color_picker, "*red");
    Widget green = XtNameToWidget(g_color_picker, "*green");
    Widget blue = XtNameToWidget(g_color_picker, "*blue");
    //Widget hex = XtNameToWidget(, "*");

    int comps[4];
    comps[3] = 255;

    char* red_str = XmTextFieldGetString(red);
    comps[0] = MAX(MIN(atoi(red_str), 255), 0);

    char* green_str = XmTextFieldGetString(green);
    comps[1] = MAX(MIN(atoi(green_str), 255), 0);

    char* blue_str = XmTextFieldGetString(blue);
    comps[2] = MAX(MIN(atoi(blue_str), 255), 0);


    uint32_t color = cc_color_pack(comps);

    char name[COLOR_NAME_MAX];
    format_color_hex_string(name, color);
    n = 0;
    XtSetArg(args[n], XmNcolorName, name); n++;
    Widget color_selector = XtNameToWidget(g_color_picker, "*color_selector");
    XtSetValues(color_selector, args, n);
}


#define SCRATCH_MAX 1024

static Widget setup_color_picker_(Widget parent, uint32_t color)
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


    char color_name[COLOR_NAME_MAX];
    format_color_hex_string(color_name, color);

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
    int color_comps[4];
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
    "#000000",
    "#444444",
    "#808080",
    "#BBBBBB",
    "#DDDDDD",
    "#FFFFFF",

    /* selections */
    "#FF8080",
    "#ECAA9F",
    "#80FF80",
    "#8080FF",
    "#7C05F2",

    "#FF6600",
    "#705E78",
    "#3B82BF",
    "#B3DAF2",
    "#F2059F",
    "#D9C2AD",
    "#667367",
    "#F2CF1D",
    "#8DA698",
    "#CAD9A9",



    /* base colors*/
    "#800000",
    "#BB0000",
    "#FF0000",

    "#808000",
    "#BBBB00",
    "#FFFF00",

    "#804000",
    "#BB5D00",
    "#FFC000",

    "#008000",
    "#00BB00",
    "#00FF00",

    "#008080",
    "#00BBBB",
    "#00FFFF",

    "#000080",
    "#0000BB",
    "#0000FF",

    "#800080",
    "#BB00BB",
    "#FF00FF",

    /*Special Colors*/
    /*"#",*/
};

enum {
    DEFAULT_COLOR_COUNT  = sizeof(g_default_colors) / sizeof(const char*),
};


static void color_clicked_(Widget widget, void* client_data, XEvent* event, Boolean* continue_dispatch)
{
    Display* display = XtDisplay(widget);
    Colormap screen_colormap = DefaultColormap(display, DefaultScreen(display));


    size_t index = (size_t)client_data;
    XColor xcolor;
    XParseColor(display, screen_colormap, g_default_colors[index], &xcolor);

    uint32_t color = xcolor_to_color(&xcolor);

    int fg = 0;

    switch (event->type)
    {
    case ButtonPress:
        if (event->xbutton.button == 1)
        {
            fg = 1;
        }
        else if (event->xbutton.button == 3)
        {
            fg = 0;
        }
        *continue_dispatch = 0;
        break;
    }

    ui_set_color(g_main_w, color, fg);
}

// Double click broken in Motif? https://bugzilla.redhat.com/show_bug.cgi?id=184143
static void color_cell_activate_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XmPushButtonCallbackStruct* cbs = (XmPushButtonCallbackStruct*)call_data;

}

static void color_swap_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    PaintContext* ctx = &g_paint_ctx;

    uint32_t temp = ctx->bg_color;
    ui_set_color(g_main_w, ctx->fg_color, 0);
    ui_set_color(g_main_w, temp, 1);
}

static void color_change_activate_(Widget widget, XtPointer client_data, XtPointer call_data)
{
     int n = 0;
     Arg args[UI_ARGS_MAX];


     PaintContext* ctx = &g_paint_ctx;
     size_t index = (size_t)client_data;

     uint32_t color;

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



/*
typedef struct
{
    char color_name[64];
    char button_name[64];
    int index;
} ColorCell;

ColorCell g_cells[DEFAULT_COLOR_COUNT];
*/


Widget ui_setup_command_area(Widget parent)
{
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

    XtSetArg(args[n], XmNmarginWidth, 16); n++;
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
    XtSetArg(args[n], XmNmarginWidth, 6); n++;
    XtSetArg(args[n], XmNmarginHeight, 6); n++;
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

    //XtSetMultiClickTime(display, 200);
    //
    Display* display = XtDisplay(parent);
    Colormap screen_colormap = DefaultColormap(display, DefaultScreen(display));

    for (int i = 0; i < DEFAULT_COLOR_COUNT; ++i)
    {
        snprintf(name, UI_NAME_MAX, "color_btn_%d", i);
        Widget cell = XtNameToWidget(color_row, name);

        XColor color;
        XParseColor(display, screen_colormap, g_default_colors[i], &color);
        XAllocColor(display, screen_colormap, &color);

        n = 0;
        XtSetArg(args[n], XmNbackground, color.pixel); n++;
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


