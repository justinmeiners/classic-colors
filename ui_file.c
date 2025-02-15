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
#include <libgen.h>
#include <Xm/FileSB.h> 
#include "Fsb.h"

#include <sys/stat.h>

static
Widget open_dialog;

static
Widget save_dialog;

static
char potential_save_path[OS_PATH_MAX];

void ui_new(Widget widget)
{
    PaintContext* ctx = &g_paint_ctx;
    if (!paint_open_file(ctx, NULL, NULL))
    {
        exit(1);
    }
    ui_refresh_drawing(1);
}

static void cb_open_file_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];

    PaintContext* ctx = &g_paint_ctx;

    XmFileSelectionBoxCallbackStruct *cbs = (XmFileSelectionBoxCallbackStruct *)call_data;

    char *filepath = NULL;
    XmStringGetLtoR(cbs->value, XmSTRING_DEFAULT_CHARSET, &filepath);

    const char* error = NULL;
    if (!paint_open_file(ctx, filepath, &error))
    {
        XmString message = XmStringCreateLocalized(error);

        n = 0;
        XtSetArg(args[n], XmNmessageString, message); ++n;
        Widget dialog = XmCreateErrorDialog(widget, "open_error_dialog", args, n);

        XtUnmanageChild(XtNameToWidget(dialog, "Help"));
        XtUnmanageChild(XtNameToWidget(dialog, "Cancel"));
        XtManageChild(dialog);
    }
    else
    {
        XtUnmanageChild(widget);
        XtDestroyWidget(widget);

        ui_refresh_title();
        ui_refresh_drawing(1);
    }

    if (filepath)
    {
        free(filepath);
    }
}

static void cb_open_cancel_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XtUnmanageChild(widget);
    XtDestroyWidget(widget);
    open_dialog = NULL;
}

static void cb_open_destroy_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    open_dialog = NULL;
}

static Widget setup_open_dialog_(Widget parent)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];

    XtSetArg(args[n], XnNshowHidden, False); ++n;
    XtSetArg(args[n], XmNresizePolicy, XmRESIZE_NONE); ++n;
    XtSetArg(args[n], XnNfsbType, FILEDIALOG_OPEN); n++;

    Widget dialog = XnCreateFileSelectionDialog (parent, "open_file", args, n);
    XtAddCallback(dialog, XmNcancelCallback, cb_open_cancel_, NULL);
    XtAddCallback(dialog, XmNokCallback, cb_open_file_, NULL);
    XtAddCallback(dialog, XmNdestroyCallback, cb_open_destroy_, NULL);

    XtUnmanageChild(XnFileSelectionBoxGetChild(dialog, XnFSB_HELP_BUTTON));

    XnFileSelectionBoxAddFilter(dialog, "*.png");
    XnFileSelectionBoxAddFilter(dialog, "*.jpg");
    XnFileSelectionBoxAddFilter(dialog, "*.gif");
    XnFileSelectionBoxAddFilter(dialog, "*.bmp");
    XnFileSelectionBoxAddFilter(dialog, "*.tga");

    Widget detailButton = XnFileSelectionBoxGetChild(dialog, XnFSB_DETAIL_TOGGLE_BUTTON);
    XtSetSensitive(detailButton, False);

    const char* path = paint_file_path(&g_paint_ctx);
    if (path)
    {
        char* copy = strndup(path, OS_PATH_MAX);
        XtVaSetValues(dialog, XnNdirectory, dirname(copy), NULL);
        free(copy);
    }
 
    return dialog;
}

static int finalize_save_(const char* filepath, Widget widget)
{
    PaintContext* ctx = &g_paint_ctx;
    if (!paint_save_file_as(ctx, filepath))
    {
        int n = 0;
        Arg args[UI_ARGS_MAX];
        XmString message = XmStringCreateLocalized("Failed to open file.");
        XtSetArg(args[n], XmNmessageString, message); ++n;
        Widget dialog = XmCreateErrorDialog(widget, "save_error_dialog", args, n);

        XtUnmanageChild(XtNameToWidget(dialog, "Help"));
        XtUnmanageChild(XtNameToWidget(dialog, "Cancel"));
        XtManageChild(dialog);

        XtFree(message);

        return 0;
    }
    else
    {
        return 1;
    }
}

static void confirm_overwrite_ok_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XtUnmanageChild(widget);
    XtDestroyWidget(widget);

    if (finalize_save_(potential_save_path, save_dialog))
    {
        XtUnmanageChild(save_dialog);
        XtDestroyWidget(save_dialog);
        ui_refresh_title();
    }
}

static void confirm_overwrite_cancel_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XtUnmanageChild(widget);
    XtDestroyWidget(widget);
}

static void cb_save_file_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];

    PaintContext* ctx = &g_paint_ctx;

    XmFileSelectionBoxCallbackStruct *cbs = (XmFileSelectionBoxCallbackStruct *)call_data;

    char *filepath = NULL;
    XmStringGetLtoR(cbs->value, XmSTRING_DEFAULT_CHARSET, &filepath);
    strncpy(potential_save_path, filepath, OS_PATH_MAX);
    XtFree(filepath);

    FILE* test_file = fopen(potential_save_path, "rb");
    if (!test_file)
    {
        // File does not exist
        if (finalize_save_(potential_save_path, widget))
        {
            // success. close.
            XtUnmanageChild(widget);
            XtDestroyWidget(widget);
            ui_refresh_title();
        }
    }
    else
    {
        // File does exist
        fclose(test_file);

        // confirm we want to overwrite
        Widget confirm = XmCreateQuestionDialog(widget, "dialog", NULL, 0);
        XmString yes = XmStringCreateLocalized("Overwrite");
        XmString no = XmStringCreateLocalized("Cancel");
        XmString message = XmStringCreateLocalized("File exists. Are you sure you want to overwrite?");

        XtVaSetValues(
                confirm,
                XmNmessageString, message,
                XmNokLabelString, yes,
                XmNcancelLabelString, no,
                NULL
                );

        XtAddCallback(confirm, XmNokCallback, confirm_overwrite_ok_, NULL);
        XtAddCallback(confirm, XmNcancelCallback, confirm_overwrite_cancel_, NULL);
        XtUnmanageChild(XtNameToWidget(confirm, "Help"));

        XtManageChild(confirm);

        XmStringFree(yes);
        XmStringFree(no);
        XmStringFree(message);
    }
}

static void cb_save_cancel_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XtUnmanageChild(widget);
    XtDestroyWidget(widget);
    save_dialog = NULL;
}

static void cb_save_destroy_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    save_dialog = NULL;
}

static Widget setup_save_dialog_(Widget parent)
{
	// Dialog resizes itself all the time (annoying)
	// http://www.44342.com/vms-f1152-t2430-p1.htm
	// http://users.polytech.unice.fr/~buffa/cours/X11_Motif/motif-faq/part5/faq-doc-16.html
    
    int n = 0;
    Arg args[UI_ARGS_MAX];

    const char* path = paint_file_path(&g_paint_ctx);
    if (path)
    {
        XtSetArg(args[n], XnNselectedPath, path); ++n;
    }

    XtSetArg(args[n], XnNshowHidden, False); ++n;
    XtSetArg(args[n], XmNresizePolicy, XmRESIZE_NONE); ++n;
    XtSetArg(args[n], XnNfsbType, FILEDIALOG_SAVE); n++;

    Widget dialog = XnCreateFileSelectionDialog (parent, "open_file", args, n);
    XtAddCallback(dialog, XmNcancelCallback, cb_save_cancel_, NULL);
    XtAddCallback(dialog, XmNokCallback, cb_save_file_, NULL);
    XtAddCallback(dialog, XmNdestroyCallback, cb_save_destroy_, NULL);

    XtUnmanageChild(XnFileSelectionBoxGetChild(dialog, XnFSB_HELP_BUTTON));

    XnFileSelectionBoxAddFilter(dialog, "*.png");
    XnFileSelectionBoxAddFilter(dialog, "*.jpg");
    XnFileSelectionBoxAddFilter(dialog, "*.gif");
    XnFileSelectionBoxAddFilter(dialog, "*.bmp");
    XnFileSelectionBoxAddFilter(dialog, "*.tga");
    
    Widget detailButton = XnFileSelectionBoxGetChild(dialog, XnFSB_DETAIL_TOGGLE_BUTTON);
    XtSetSensitive(detailButton, False);

    return dialog;
}

void ui_cb_file_menu(Widget w, XtPointer a, XtPointer b)
{
    PaintContext* ctx = &g_paint_ctx;

    size_t item = (size_t)a;

    switch (item) 
    {
        case 0:
            ui_new(w);
            break;
        case 1:
            if (!open_dialog)
            {
                open_dialog = setup_open_dialog_(g_main_w);
            }

            XtManageChild(open_dialog);
            break;
        case 2:
            if (paint_file_path(ctx))
            {
                paint_save_file(ctx);
                break;
            }
            // fallthrough
        case 3:
            if (!save_dialog)
            {
                save_dialog = setup_save_dialog_(g_main_w);
            }
            XtManageChild(save_dialog);
            break;
        case 4:
            XtAppSetExitFlag(g_app);
            break;

    }
}

void ui_setup_file_menu(Widget menubar)
{
    XmString new_str = XmStringCreateLocalized("New");
    XmString open_str = XmStringCreateLocalized("Open");
    XmString save_str = XmStringCreateLocalized("Save");
    XmString save_key = XmStringCreateLocalized("Ctrl+S");
    XmString save_as_str = XmStringCreateLocalized("Save As");
    XmString exit_str = XmStringCreateLocalized("Exit");

    XmVaCreateSimplePulldownMenu(menubar, "file_menu", 0, ui_cb_file_menu,
            XmVaPUSHBUTTON, new_str, 'N', NULL, NULL,
            XmVaPUSHBUTTON, open_str, 'O', NULL, NULL,
            XmVaPUSHBUTTON, save_str, 'S', "Ctrl<Key>s", save_key,
            XmVaPUSHBUTTON, save_as_str, 'A', NULL, NULL,
            XmVaSEPARATOR,
            XmVaPUSHBUTTON, exit_str, 'x', NULL, NULL,
            NULL);

    XmStringFree(exit_str);
    XmStringFree(new_str);
    XmStringFree(open_str);
    XmStringFree(save_key);
    XmStringFree(save_str);
    XmStringFree(save_as_str);
}


