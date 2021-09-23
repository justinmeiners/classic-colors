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
#include <libgen.h>
#include <Xm/FileSB.h> 

#include <sys/stat.h>

Widget open_dialog;
Widget save_dialog;
char* potential_save_path = NULL;

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

    char* filepath = (char *)XmStringUnparse(
            cbs->value,
            XmFONTLIST_DEFAULT_TAG,
            XmCHARSET_TEXT,
            XmCHARSET_TEXT,
            NULL, 0, XmOUTPUT_ALL
            );


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
        XtDestroyWidget(widget);
        ui_refresh_drawing(1);
    }


    if (filepath)
    {
        XtFree(filepath);
    }
}

static void cb_open_cancel_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XtDestroyWidget(widget);
    open_dialog = NULL;
}

static void cb_open_destroy_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    open_dialog = NULL;
}

static XmString paint_dirname_()
{
    if (paint_file_path(&g_paint_ctx))
    {
        char* copy = strndup(paint_file_path(&g_paint_ctx), OS_PATH_MAX);
        char* dir = dirname(copy);

        XmString dir_string = XmStringCreateLocalized(dir);
        free(copy);

        return dir_string;
    }
    else
    {
        /* default is CWD which is prefferable to home (I think)
        char* dir = getenv("HOME");
        if (dir)
        {
            return XmStringCreateLocalized(dir);
        }
        else
        {
            return NULL;
        }
        */
       return NULL;
    }
}

static Widget setup_open_dialog_(Widget parent)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];

    XmString pattern = XmStringCreateLocalized("*.png, *.jpg");

    n = 0;
    XtSetArg(args[n], XmNpathMode, XmPATH_MODE_RELATIVE); ++n;
    XtSetArg(args[n], XmNfileTypeMask, XmFILE_REGULAR); ++n;
    XtSetArg(args[n], XmNdeleteResponse, XmDESTROY); ++n;
    XtSetArg(args[n], XmNresizePolicy, XmRESIZE_NONE); ++n;

    // not sure how to get patterns to do multiple extensions
    // https://www.ibm.com/support/pages/apar/PQ61558
    // XtSetArg (args[n], XmNpattern, pattern); ++n;

    Widget dialog = XmCreateFileSelectionDialog (parent, "open_file", args, n);
    XtAddCallback(dialog, XmNcancelCallback, cb_open_cancel_, NULL);
    XtAddCallback(dialog, XmNokCallback, cb_open_file_, NULL);
    XtAddCallback(dialog, XmNdestroyCallback, cb_open_destroy_, NULL);
    XmStringFree(pattern);

    XmString dir = paint_dirname_();
    if (dir)
    {
        n = 0;
        XtSetArg(args[n], XmNdirectory, dir); ++n;
        XtSetValues(dialog, args, n);

        XmStringFree(dir);
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

static void confirm_save_ok_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XtDestroyWidget(widget);

    if (finalize_save_(potential_save_path, save_dialog))
    {
        XtDestroyWidget(save_dialog);
    }

    if (potential_save_path) XtFree(potential_save_path);
    potential_save_path = NULL;
}

static void confirm_save_cancel_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XtDestroyWidget(widget);
}

static void cb_save_file_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];

    PaintContext* ctx = &g_paint_ctx;

    XmFileSelectionBoxCallbackStruct *cbs = (XmFileSelectionBoxCallbackStruct *)call_data;

    char* filepath = (char *) XmStringUnparse (cbs->value,
            XmFONTLIST_DEFAULT_TAG,
            XmCHARSET_TEXT,
            XmCHARSET_TEXT,
            NULL, 0, XmOUTPUT_ALL);


    FILE* test_file = fopen(filepath, "rb");
    if (!test_file)
    {
        // File does not exist
        if (finalize_save_(filepath, widget))
        {
            // success. close.
            XtDestroyWidget(save_dialog);
        }
        if (filepath) XtFree(filepath);
    }
    else
    {
        // File does exist
        fclose(test_file);
        potential_save_path = filepath;

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

        XtAddCallback(confirm, XmNokCallback, confirm_save_ok_, NULL);
        XtAddCallback(confirm, XmNcancelCallback, confirm_save_cancel_, NULL);
        XtUnmanageChild(XtNameToWidget(confirm, "Help"));

        XtManageChild(confirm);

        XmStringFree(yes);
        XmStringFree(no);
        XmStringFree(message);
    }
}

static void cb_save_cancel_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XtDestroyWidget(widget);
    save_dialog = NULL;
}

static void cb_save_destroy_(Widget widget, XtPointer client_data, XtPointer call_data)
{
    save_dialog = NULL;
}

static Widget setup_save_dialog_(Widget parent)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];


	// Dialog resizes itself all the time (annoying)
	// http://www.44342.com/vms-f1152-t2430-p1.htm
	// http://users.polytech.unice.fr/~buffa/cours/X11_Motif/motif-faq/part5/faq-doc-16.html
    n = 0;
    XtSetArg(args[n], XmNpathMode, XmPATH_MODE_RELATIVE); ++n;
    XtSetArg(args[n], XmNdeleteResponse, XmDESTROY); ++n;
    XtSetArg(args[n], XmNresizePolicy, XmRESIZE_NONE); ++n;

    XmString pattern = XmStringCreateLocalized("*.png");
    //XtSetArg(args[n], XmNpattern, pattern); ++n;

    Widget dialog = XmCreateFileSelectionDialog(parent, "Save File", args, n);
    XtAddCallback(dialog, XmNcancelCallback, cb_save_cancel_, NULL);
    XtAddCallback(dialog, XmNokCallback, cb_save_file_, NULL);
    XtAddCallback(dialog, XmNdestroyCallback, cb_save_destroy_, NULL);
    XmStringFree(pattern);


    XmString dir = paint_dirname_();
    
    if (dir)
    {
        n = 0;
        XtSetArg(args[n], XmNdirectory, dir); ++n;
        XtSetValues(dialog, args, n);

        XmStringFree(dir);
    }

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


