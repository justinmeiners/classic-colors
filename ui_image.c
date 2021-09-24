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

Widget flip_dialog = NULL;
Widget stretch_dialog = NULL;
Widget attributes_dialog = NULL;
size_t flip_option = 0;

static void cb_flip_ok_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    switch (flip_option)
    {
        case 0:
            paint_flip(&g_paint_ctx, 1);
            break;
        case 1:
            paint_flip(&g_paint_ctx, 0);
            break;
        case 2:
            paint_rotate_90(&g_paint_ctx, 1);
            break;
        case 3:
            paint_rotate_90(&g_paint_ctx, 2);
            break;
        case 4:
            paint_rotate_90(&g_paint_ctx, 3);
            break;
        case 5:
        {
            Widget angle = XtNameToWidget(widget, "*angle");
            char* angle_str = XmTextFieldGetString(angle);
            double a = atof(angle_str);
            paint_rotate_angle(&g_paint_ctx, a);
            break;
        }
    }
    ui_refresh_drawing(1);


    XtDestroyWidget(flip_dialog);
    flip_dialog = NULL;
}

static void cb_flip_cancel_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XtDestroyWidget(flip_dialog);
    flip_dialog = NULL;
}

void ui_cb_flip_selection(Widget widget, XtPointer client_data, XtPointer call_data)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];


    XmToggleButtonCallbackStruct* state = (XmToggleButtonCallbackStruct*)call_data;
    if (state->set == XmSET)
    {
        flip_option = (size_t)client_data;

        int show_angle = 0;

        switch (flip_option)
        {
            case 5:
                show_angle = 1;
               break;
            default:
                show_angle = 0;
                break;
        }

        Widget angle = XtNameToWidget(flip_dialog, "*angle");
        n = 0;
        XtSetArg (args[n], XmNeditable, show_angle); ++n;
        XtSetValues(angle, args, n);

        if (show_angle)
        {
            XmProcessTraversal(angle, XmTRAVERSE_CURRENT);
        }
    }
}

static Widget setup_flip_dialog_(Widget parent)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];

    Widget dialog = XmCreateDialogShell(parent, "Flip and Rotate", args, n);

    n = 0;

    XtSetArg(args[n], XmNnoResize, 1); ++n;
    Widget box = XmCreateMessageBox(dialog, "message_box", args, n);
    XtUnmanageChild(XtNameToWidget(box, "Help"));

    Widget rowcol = XmCreateRowColumn(box,"rowcol", NULL, 0);
    XmString horiz_str = XmStringCreateLocalized("Flip horizontal");
    XmString vert_str = XmStringCreateLocalized("Flip vertical");
    XmString rotate_90_str = XmStringCreateLocalized("Rotate 90");
    XmString rotate_180_str = XmStringCreateLocalized("Rotate 180");
    XmString rotate_270_str = XmStringCreateLocalized("Rotate 270");
    XmString rotate_angle_str = XmStringCreateLocalized("Rotate Angle");


    Widget radio_box = XmVaCreateSimpleRadioBox(
            rowcol,
            "flip_option_radio",
            0,
            ui_cb_flip_selection,
            XmVaRADIOBUTTON, horiz_str, NULL, NULL, NULL,
            XmVaRADIOBUTTON, vert_str, NULL, NULL, NULL,
            XmVaRADIOBUTTON, rotate_90_str, NULL, NULL, NULL,
            XmVaRADIOBUTTON, rotate_180_str, NULL, NULL, NULL,
            XmVaRADIOBUTTON, rotate_270_str, NULL, NULL, NULL,
            XmVaRADIOBUTTON, rotate_angle_str, NULL, NULL, NULL,
            NULL);

    XmStringFree(vert_str);
    XmStringFree(horiz_str);
    XmStringFree(rotate_90_str);
    XmStringFree(rotate_180_str);
    XmStringFree(rotate_270_str);
    XmStringFree(rotate_angle_str);

    XtManageChild(radio_box);

    n = 0;
    XtSetArg(args[n], XmNorientation, XmHORIZONTAL); ++n;

    Widget angle_row = XmCreateRowColumn (rowcol, "angle_row", args, n);

    n = 0;
    XtSetArg(args[n], XmNeditable, 0); ++n;
    Widget angle = XtCreateManagedWidget("angle", xmTextFieldWidgetClass, angle_row, args, n);

    XtCreateManagedWidget("Degrees", xmLabelWidgetClass, angle_row, NULL, 0);
 
    XtManageChild(angle_row);
    XtManageChild(rowcol);
    XtManageChild(box);

    XtAddCallback(box, XmNcancelCallback, cb_flip_cancel_, NULL);
    XtAddCallback(box, XmNokCallback, cb_flip_ok_, NULL);

    return dialog;
}

static void stretch_cancel_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XtDestroyWidget(stretch_dialog);
    stretch_dialog = NULL;
}

static void stretch_destroy_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    stretch_dialog = NULL;
}

#define MAX_STRETCH 1000

static void stretch_ok_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    Widget width = XtNameToWidget(stretch_dialog, "*width");
    char* width_str = XmTextFieldGetString(width);
    int w = MIN(atoi(width_str), MAX_STRETCH);


    Widget height = XtNameToWidget(stretch_dialog, "*height");
    char* height_str = XmTextFieldGetString(height);
    int h = MIN(atoi(height_str), MAX_STRETCH);

    Widget x_angle = XtNameToWidget(stretch_dialog, "*x_angle");
    char* x_angle_str = XmTextFieldGetString(x_angle);
    int x = atoi(x_angle_str);


    Widget y_angle = XtNameToWidget(stretch_dialog, "*y_angle");
    char* y_angle_str = XmTextFieldGetString(y_angle);
    int y = atoi(y_angle_str);

    XtFree(width_str);
    XtFree(height_str);
    XtFree(x_angle_str);
    XtFree(y_angle_str);

    paint_stretch(&g_paint_ctx, w, h, x, y);
    ui_refresh_drawing(1);

    XtDestroyWidget(stretch_dialog);
    stretch_dialog = NULL;
}


static Widget setup_stretch_dialog_(Widget parent)
{
    Arg dialog_args[] = {
        { XmNdeleteResponse, XmDESTROY },
    };

    Widget dialog = XmCreateDialogShell(parent, "Stretch and Skew", dialog_args, XtNumber(dialog_args));

    Arg message_args[] = {
        { XmNnoResize, 1 },
    };
    Widget box = XmCreateMessageBox(dialog, "message_box", message_args, XtNumber(message_args));
    XtUnmanageChild(XtNameToWidget(box, "Help"));

    Widget rowcol_v = XmCreateRowColumn (box, "row_v", NULL, 0);
    {
        Widget width_row = XtVaCreateWidget("width_row", xmRowColumnWidgetClass, rowcol_v, XmNorientation, XmHORIZONTAL, NULL);
        Widget label_w = XmCreateLabel (width_row, "Width:", NULL, 0);
        XtManageChild (label_w);

        Widget width = XmCreateTextField (width_row, "width", NULL, 0);
        XmTextSetString(width, "100");
        XtManageChild(width);

        Widget percent_w = XmCreateLabel (width_row, "%", NULL, 0);
        XtManageChild(percent_w);
        XtManageChild(width_row);

        Widget height_row = XtVaCreateWidget("height_row", xmRowColumnWidgetClass, rowcol_v, XmNorientation, XmHORIZONTAL, NULL);
        Widget label_h = XmCreateLabel (height_row, "Height:", NULL, 0);
        XtManageChild(label_h);

        Widget height = XmCreateTextField(height_row, "height", NULL, 0);
        XmTextSetString(height, "100");
        XtManageChild(height);

        Widget percent_h = XmCreateLabel(height_row, "%", NULL, 0);
        XtManageChild(percent_h);
        XtManageChild(height_row);
        Widget sep = XmCreateSeparator(rowcol_v, "sep", NULL, 0);
        XtManageChild(sep);
    }
    {
        Widget widget;
        Widget x_row = XtVaCreateWidget("x_row", xmRowColumnWidgetClass, rowcol_v, XmNorientation, XmHORIZONTAL, NULL);
        XtCreateManagedWidget("Horizontal:", xmLabelWidgetClass, x_row, NULL, 0);

        Widget x = XtCreateManagedWidget("x_angle", xmTextFieldWidgetClass, x_row, NULL, 0);
        XmTextSetString(x, "0");

        XtCreateManagedWidget("deg", xmLabelWidgetClass, x_row, NULL, 0);
        XtManageChild(x_row);

        Widget y_row = XtVaCreateWidget("y_row", xmRowColumnWidgetClass, rowcol_v, XmNorientation, XmHORIZONTAL, NULL);
        XtCreateManagedWidget("Vertical:", xmLabelWidgetClass, y_row, NULL, 0);

        Widget y = XtCreateManagedWidget("y_angle", xmTextFieldWidgetClass, y_row, NULL, 0);
        XmTextSetString(y, "0");

        XtCreateManagedWidget("deg", xmLabelWidgetClass, y_row, NULL, 0);
        XtManageChild(y_row);
    }

    XtManageChild(rowcol_v);
    XtManageChild(box);

    XtAddCallback(box, XmNcancelCallback, stretch_cancel_, NULL);
    XtAddCallback(box, XmNokCallback, stretch_ok_, NULL);
    XtAddCallback(dialog, XmNdestroyCallback, stretch_destroy_, NULL);

    return dialog;
}

static void attributes_ok_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    Widget width = XtNameToWidget(attributes_dialog, "*width");
    char* width_str = XmTextFieldGetString(width);
    int w = MIN(atoi(width_str), INT16_MAX);

    Widget height = XtNameToWidget(attributes_dialog, "*height");
    char* height_str = XmTextFieldGetString(height);
    int h = MIN(atoi(height_str), INT16_MAX);

    paint_resize(&g_paint_ctx, w, h);
    ui_refresh_drawing(1);

    XtDestroyWidget(attributes_dialog);
    attributes_dialog = NULL;
}

static void attributes_cancel_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XtDestroyWidget(attributes_dialog);
    attributes_dialog = NULL;
}

static void attributes_destroy_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    attributes_dialog = NULL;
}

#define SCRATCH_MAX 1024

static Widget setup_attributes_dialog_(Widget parent)
{
    Arg args[] = {
        { XmNdeleteResponse, XmDESTROY },
    };

    Widget dialog = XmCreateDialogShell(parent, "Attributes", args, XtNumber(args));

    Arg message_args[] = {
        { XmNnoResize, 1 },
    };
    Widget box = XmCreateMessageBox(dialog, "message_box", message_args, XtNumber(message_args));

    XtUnmanageChild(XtNameToWidget(box, "Help"));

    Widget rowcol_v = XtVaCreateWidget("row_v", xmRowColumnWidgetClass, box, NULL);

    {
        const Layer* l = g_paint_ctx.layers + g_paint_ctx.active_layer;

        char scratch_buffer[SCRATCH_MAX];
        Widget size_row = XtVaCreateWidget("x_row", xmRowColumnWidgetClass, rowcol_v, XmNorientation, XmHORIZONTAL, NULL);
        XtCreateManagedWidget("Width:", xmLabelWidgetClass, size_row, NULL, 0);

        Widget w = XtCreateManagedWidget("width", xmTextFieldWidgetClass, size_row, NULL, 0);
        snprintf(scratch_buffer, SCRATCH_MAX, "%d", l->bitmaps->w);
        XmTextSetString(w, scratch_buffer);

        XtCreateManagedWidget("Height:", xmLabelWidgetClass, size_row, NULL, 0);

        Widget h = XtCreateManagedWidget("height", xmTextFieldWidgetClass, size_row, NULL, 0);
        snprintf(scratch_buffer, SCRATCH_MAX, "%d", l->bitmaps->h);
        XmTextSetString(h, scratch_buffer);
        XtManageChild(size_row);

    }

    XtManageChild(rowcol_v);
    XtManageChild(box);

    XtAddCallback(box, XmNcancelCallback, attributes_cancel_, NULL);
    XtAddCallback(box, XmNokCallback, attributes_ok_, NULL);
    XtAddCallback(dialog, XmNdestroyCallback, attributes_destroy_, NULL);

    return dialog;
}


void ui_cb_image_menu(Widget widget, XtPointer a, XtPointer b)
{
    size_t item = (size_t)a;

    switch (item) 
    {
        case 0:
            if (!flip_dialog)
            {
                flip_dialog = setup_flip_dialog_(g_main_w);
            }
            XtManageChild(flip_dialog);
            break;
        case 1:
            if (!stretch_dialog)
            {
                stretch_dialog = setup_stretch_dialog_(g_main_w);
            }
            XtManageChild(stretch_dialog);
            break;
        case 2:
            paint_invert_colors(&g_paint_ctx);
            ui_refresh_drawing(0);
            break;
        case 3:
            if (!attributes_dialog)
            {
                attributes_dialog = setup_attributes_dialog_(g_main_w);
            }

            XtManageChild(attributes_dialog);
            break;
        case 4:
            paint_clear(&g_paint_ctx);
            ui_refresh_drawing(0);
            break;
        case 5:
            paint_crop(&g_paint_ctx);
            ui_refresh_drawing(1);
            break;
    }
}


void ui_setup_image_menu(Widget menubar)
{
    XmString flip_str = XmStringCreateLocalized("Flip/Rotate");
    XmString stretch_str = XmStringCreateLocalized("Stretch/Skew");
    XmString invert_str = XmStringCreateLocalized("Invert Colors");
    XmString invert_key = XmStringCreateLocalized("Ctrl+I");
    XmString attributes_str = XmStringCreateLocalized("Attributes");
    XmString attributes_key = XmStringCreateLocalized("Ctrl+E");
    XmString clear_str = XmStringCreateLocalized("Clear Image");
    XmString clear_key = XmStringCreateLocalized("Del");
    XmString crop_str = XmStringCreateLocalized("Crop Image");


    XmVaCreateSimplePulldownMenu(menubar, "image_menu", 3, ui_cb_image_menu,
            XmVaPUSHBUTTON, flip_str, 'F', NULL, NULL,
            XmVaPUSHBUTTON, stretch_str, 'S', NULL, NULL,
            XmVaPUSHBUTTON, invert_str, 'I', "Ctrl<Key>i", invert_key,
            XmVaPUSHBUTTON, attributes_str, 'A', "Ctrl<Key>E", attributes_key,
            XmVaPUSHBUTTON, clear_str, 'C', "<Key>Delete", clear_key,
            XmVaPUSHBUTTON, crop_str, 'r', NULL, NULL,
            NULL);

    XmStringFree(clear_str);
    XmStringFree(clear_key);
    XmStringFree(attributes_str);
    XmStringFree(attributes_key);
    XmStringFree(invert_key);
    XmStringFree(invert_str);
    XmStringFree(stretch_str);
    XmStringFree(crop_str);
    XmStringFree(flip_str);

}

