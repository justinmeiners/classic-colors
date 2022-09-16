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
#include <Xm/ComboBox.h>
#include <Xm/SSpinB.h>
#include <Xm/PanedW.h>
#include <Xm/TextF.h>
#include <X11/Shell.h>

Widget g_window = NULL;

static void update_text_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    PaintContext* ctx = &g_paint_ctx;
    if (ctx->tool != TOOL_TEXT) return;

    const wchar_t* text = XmTextGetStringWcs(widget);

    CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
    cc_layer_set_text(overlay, text);

    ui_refresh_drawing(0);
}

static void update_font_size_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    PaintContext* ctx = &g_paint_ctx;
    if (ctx->tool != TOOL_TEXT) return;

    XmSpinBoxCallbackStruct* cbs = (XmSpinBoxCallbackStruct*)call_data;

    CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
    overlay->font_size = cbs->position;
    cc_layer_render(overlay);

    ui_refresh_drawing(0);
}

static void update_font_(Widget widget,  XtPointer client_data, XtPointer call_data)
{
    PaintContext* ctx = &g_paint_ctx;
    if (ctx->tool != TOOL_TEXT) return;

    XmComboBoxCallbackStruct* cbs = (XmComboBoxCallbackStruct *)call_data;

    CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
    paint_set_font(ctx, overlay, cbs->item_position);
    cc_layer_render(overlay);
    ui_refresh_drawing(0);
}


static void update_font_alignment_(Widget widget,  XtPointer client_data, XtPointer call_data)
{
    PaintContext* ctx = &g_paint_ctx;
    if (ctx->tool != TOOL_TEXT) return;

    XmComboBoxCallbackStruct* cbs = (XmComboBoxCallbackStruct *)call_data;

    int align; 
    switch (cbs->item_position)
    {
        case 1:
            align = TEXT_ALIGN_CENTER;
            break;
        case 2:
            align = TEXT_ALIGN_RIGHT;
            break;
        default:
            align = TEXT_ALIGN_LEFT;
            break;
    }

    CcLayer* overlay = ctx->layers + LAYER_OVERLAY;
    overlay->font_align = align;
    cc_layer_render(overlay);
    ui_refresh_drawing(0);
}

static void font_destroy_(Widget widget,  XtPointer client_data, XtPointer call_data)
{
    g_window = NULL;
}

static Widget setup_text_window_(Widget parent)
{
    PaintContext* ctx = &g_paint_ctx;
    CcLayer* overlay = ctx->layers + LAYER_OVERLAY;

    // The text/font window is a top-level window, not a dialog.
    // Dialog's are primarily transient, but this window is not.
    // See chapter 7 of the Motif book.

    int n = 0;
    Arg args[UI_ARGS_MAX];

    XtSetArg(args[n], XmNdeleteResponse, XmDESTROY); ++n;
    XtSetArg(args[n], XmNminWidth, 528); ++n;
    XtSetArg(args[n], XmNminHeight, 256); ++n;
    XtSetArg(args[n], XmNwidthInc, 16); ++n;
    XtSetArg(args[n], XmNheightInc, 16); ++n;


    Widget dialog = XtCreatePopupShell("Classic Color - Text options", topLevelShellWidgetClass, parent, args, n);
    Widget pane = XtVaCreateWidget("pane", xmPanedWindowWidgetClass, dialog,
            XmNseparatorOn, 1,
            // from motif book
            XmNsashWidth, 1,
            XmNsashHeight, 1,
            XmNdefaultPosition, 1,
            NULL);

    Widget x_row = XtVaCreateWidget(
            "x_row",
            xmRowColumnWidgetClass,
            pane,
            XmNorientation, XmHORIZONTAL,
            XmNpaneMaximum, 50,
            NULL);


    {
        int font_count = paint_font_count();

        XmString* xms = (XmString*)XtMalloc(font_count * sizeof(XmString));
        for (int i = 0; i < font_count; i++)
            xms[i] = XmStringCreateLocalized((char*)paint_font_name(i));

        n = 0;
        XtSetArg(args[n], XmNitems, xms); ++n;
        XtSetArg(args[n], XmNitemCount, font_count); ++n;
        XtSetArg(args[n], XmNselectedPosition, overlay->font); ++n;

        Widget font = XmCreateDropDownComboBox(x_row, "font", args, n);
        XtAddCallback(font, XmNselectionCallback, update_font_, 0);
        XtManageChild(font);

        for (int i = 0; i < font_count; ++i)
            XmStringFree(xms[i]);
    }

    {
        Widget size = XtVaCreateManagedWidget(
                "font_size",
                xmSimpleSpinBoxWidgetClass,
                x_row,
                XmNspinBoxChildType, XmNUMERIC,
                XmNpositionType, XmPOSITION_VALUE,
                XmNminimumValue, 2,
                XmNmaximumValue, 120,
                XmNposition, overlay->font_size,
                XmNeditable, 1,
                NULL);

        XtAddCallback(size, XmNvalueChangedCallback, update_font_size_, 0);
    }

    {
        XmString options[] = {
            XmStringCreateLocalized("Left"),
            XmStringCreateLocalized("Center"),
            XmStringCreateLocalized("Right")
        };

        int selected_alignment = 0;
        switch (overlay->font_align)
        {
            case TEXT_ALIGN_CENTER:
                selected_alignment = 1;
                break;
            case TEXT_ALIGN_RIGHT:
                selected_alignment = 2;
                break;
            default:
                selected_alignment = 0;
                break;
        }


        n = 0;
        XtSetArg(args[n], XmNitems, options); ++n;
        XtSetArg(args[n], XmNitemCount, 3); ++n;
        XtSetArg(args[n], XmNselectedPosition, selected_alignment); ++n;
        Widget alignment = XmCreateDropDownComboBox(x_row, "alignment", args, n);
        XtAddCallback(alignment, XmNselectionCallback, update_font_alignment_, 0);
        XtManageChild(alignment);

        for (int i = 0; i < 3; ++i)
            XmStringFree(options[i]);
    }
    XtManageChild(x_row);


    Widget text;
    {
        Arg args[] = {
            { XmNeditMode, XmMULTI_LINE_EDIT },
            { XmNwordWrap, 1 },
            { XmNrows, 4 }
        };

        text = XmCreateText(pane, "text_area", args, XtNumber(args));
        XtManageChild(text);
        XtAddCallback(text, XmNvalueChangedCallback, update_text_, 0);

    }

    XtManageChild(pane);
    XtAddCallback(dialog, XmNdestroyCallback, font_destroy_, NULL);

    XmProcessTraversal(text, XmTRAVERSE_CURRENT);
    return dialog;
}


Widget ui_start_editing_text(void)
{
    PaintContext* ctx = &g_paint_ctx;

    if (!g_window)
    {
        g_window = setup_text_window_(g_main_w);
    }
    Widget text = XtNameToWidget(g_window, "*text_area");
    wchar_t* current_text = ctx->layers[LAYER_OVERLAY].text;
    XmTextSetStringWcs(text, current_text == NULL ? L"" : current_text);

    XtVaSetValues(text, XmNeditable, 1, NULL);

    XtManageChild(g_window);
    return g_window;
}


void ui_disable_editing_text(void)
{
    if (!g_window) return;

    Widget text = XtNameToWidget(g_window, "*text_area");
    XtVaSetValues(text, XmNeditable, 0, NULL);
}

