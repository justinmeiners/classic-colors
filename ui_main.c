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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>

#include "ui.h"

#include <Xm/MainW.h>
#include <Xm/PanedW.h>
#include <Xm/CascadeB.h>

#include <X11/xpm.h>
#include "icons/icon_app.xpm"


PaintContext g_paint_ctx;
Widget g_main_w = NULL;
XtAppContext g_app = NULL;
int g_ready = 0;


Widget about_dialog = NULL;

static void about_destroy_()
{
    about_dialog = NULL;
}

static void about_close_()
{
    XtDestroyWidget(about_dialog);
    about_dialog = NULL;
}

static int open_url_(const char* url)
{
    char command[OS_PATH_MAX];
    snprintf(command, OS_PATH_MAX, "xdg-open %s &", url);

    if (system(command) != 0)
    {
        if (DEBUG_LOG) fprintf(stderr, "xdg-open failed..");
        if (system(command + 4) != 0) 
        {
            return 0;
        }
    }
    return 1;
}

static void cb_open_website_()
{
    open_url_("https://github.com/justinmeiners/classic-colors");
    XtDestroyWidget(about_dialog);
    about_dialog = NULL;
}

static char* about_string_ =
"Classic Colors v1.0.0\n\
\n\
Created by Justin Meiners (https://www.jmeiners.com) in 2021. \n\
Classic Colors is free software available under the GPL 2 license.\n\
\n\
To provide feedback or file a bug please visit the project website:\n\
https://github.com/justinmeiners/classic-colors\n\
\n\
Additional thanks:\n\
Sean Barrett (http://nothings.org) for stb libs \n\
MagnetarRocket (https://github.com/MagnetarRocket)\n";

static Widget setup_about_dialog_(Widget parent)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];

    XmString about_string = XmStringCreateLtoR(about_string_, XmSTRING_DEFAULT_CHARSET);

    XmString title = XmStringCreateLocalized("About");
    XmString website = XmStringCreateLocalized("Website");
    XmString close = XmStringCreateLocalized("Close");

    n = 0;
    XtSetArg(args[n], XmNdeleteResponse, XmDESTROY); ++n;
    Widget dialog =  XmCreateDialogShell(parent, "About", args, n);
    XmStringFree(title);

    n = 0;
    XtSetArg(args[n], XmNmessageString, about_string); ++n;
    XtSetArg(args[n], XmNokLabelString, website); ++n;
    XtSetArg(args[n], XmNcancelLabelString, close); ++n;
    XtSetArg(args[n], XmNnoResize, 1); ++n;
    Widget pane = XmCreateMessageBox(dialog, "message_box", args, n);

    XtAddCallback(pane, XmNokCallback, cb_open_website_, NULL);
    XtAddCallback(pane, XmNcancelCallback, about_close_, NULL);
    XtAddCallback(dialog, XmNdestroyCallback, about_destroy_, NULL);

    XtManageChild(pane);
    XtManageChild(dialog);

    XmStringFree(website);
    XmStringFree(close);
    XmStringFree(about_string);

    return dialog;
}

static void cb_help_menu_(Widget widget, XtPointer a, XtPointer call_data)
{
    size_t item = (size_t)a;

    switch (item) 
    {
        case 0:
        {
            open_url_("/usr/local/share/classic-colors/help/classic-colors_help-en.html");
            break;
        }
        case 1:
        {
            if (!about_dialog)
            {
                about_dialog = setup_about_dialog_(g_main_w);
            }
            XtManageChild(about_dialog);
            break;
        }
    }
}


XtAppContext ui_app()
{
    return g_app;
}

void ui_refresh_title(void)
{
    char title[OS_PATH_MAX];
    const char* path = paint_file_path(&g_paint_ctx);
    if (path)
    {
        char* copy = strndup(path, OS_PATH_MAX);
        snprintf(title, OS_PATH_MAX, "%s - Classic Colors", basename(copy));
        free(copy);
    }
    else
    {
        snprintf(title, OS_PATH_MAX, "Classic Colors");
    }
    XtVaSetValues(XtParent(g_main_w), XmNtitle, title, NULL);
}

Widget ui_setup_menu(Widget parent)
{

    XmString file_str = XmStringCreateLocalized("File");
    XmString edit_str = XmStringCreateLocalized("Edit");
    XmString view_str = XmStringCreateLocalized("View");
    XmString image_str = XmStringCreateLocalized("Image");
    XmString help_str = XmStringCreateLocalized("Help");

    Widget menubar = XmVaCreateSimpleMenuBar(parent, "menubar",
            XmVaCASCADEBUTTON, file_str, 'F',
            XmVaCASCADEBUTTON, edit_str, 'E',
            XmVaCASCADEBUTTON, view_str, 'V',
            XmVaCASCADEBUTTON, image_str, 'I',
            NULL);

    XmStringFree(help_str);
    XmStringFree(image_str);
    XmStringFree(view_str);
    XmStringFree(edit_str);
    XmStringFree(file_str);

    ui_setup_file_menu(menubar);
    ui_setup_edit_menu(menubar);
    ui_setup_image_menu(menubar);
    ui_setup_view_menu(menubar);

    XmString manual_str = XmStringCreateLocalized("Manual");
    XmString about_str = XmStringCreateLocalized("About");

    Widget help_button = XmCreateCascadeButton(menubar, "Help", NULL, 0);
    XtVaSetValues(menubar, XmNmenuHelpWidget, help_button, NULL);

    Widget help_bar = XmVaCreateSimplePulldownMenu(menubar, "help_menu",
            -1, cb_help_menu_,
            XmVaPUSHBUTTON, manual_str, 'M', NULL, NULL,
            XmVaPUSHBUTTON, about_str, 'A', NULL, NULL,
            NULL);

    XtVaSetValues(help_button, XmNsubMenuId, help_bar, NULL);

    XmStringFree(manual_str);
    XmStringFree(about_str);

    XtManageChild(help_button);
    XtManageChild(menubar);

    return menubar;
}

#if DEBUG_LOG
void run_tests()
{
    test_text_wordwrap();
    color_blending_test();
}
#endif

int main(int argc, char **argv)
{ 
#if DEBUG_LOG
    run_tests();
#endif
    XtSetLanguageProc(NULL, NULL, NULL);

    int n = 0;
    Arg args[UI_ARGS_MAX];

    Widget top_wid = XtOpenApplication(
            &g_app,
            "Paint",
            NULL,
            0,
            &argc,
            argv,
            NULL,
            sessionShellWidgetClass,
            NULL,
            0);

/* pledge doesn't support shm
#ifdef __OpenBSD__
    if (pledge("stdio rpath wpath cpath tmppath proc exec unix", NULL) == -1)
    {
        fprintf(stderr, "failed to pledge\n");
    }
#endif
*/

    //icon
    Pixmap icon_pixmap;
    Pixmap icon_mask;
    Screen* top_screen = XtScreen(top_wid);
    XpmCreatePixmapFromData(XtDisplay(top_wid), RootWindowOfScreen(top_screen), icon_app, &icon_pixmap, &icon_mask, NULL);
    XtSetArg(args[n], XmNiconPixmap, icon_pixmap); ++n;
    XtSetArg(args[n], XmNiconMask, icon_mask); ++n;

    // https://www.oreilly.com/openbook/motif/vol6a/Vol6a_html/ch16.html
    XtSetArg(args[n], XmNminWidth, 640); ++n;
    XtSetArg(args[n], XmNminHeight, 480); ++n;
    XtSetArg(args[n], XmNbaseWidth, 16); ++n;
    XtSetArg(args[n], XmNbaseHeight, 16); ++n;
    XtSetArg(args[n], XmNwidthInc, 16); ++n;
    XtSetArg(args[n], XmNheightInc, 16); ++n;

    int default_width, default_height;
    if (top_screen->width < 1024)
    {
        default_width = 640;
    }
    else
    {
        default_width = 960;
    }
    if (top_screen->height < 768)
    {
        default_height = 480;
    }
    else
    {
        default_height = 720;
    }

    XtSetArg(args[n], XmNwidth, default_width); ++n;
    XtSetArg(args[n], XmNheight, default_height); ++n;

    XtSetValues(top_wid, args, n);

    g_main_w = XtCreateManagedWidget(
            "main_window",
            xmMainWindowWidgetClass,
            top_wid,
            NULL,
            0
    );


    Widget menu = ui_setup_menu(g_main_w);

    Arg pane_args[] = {
        { XmNorientation, XmHORIZONTAL },
        { XmNseparatorOn, 1 },
        { XmNspacing, 0 },
    };

    Widget work_area_pane = XtCreateWidget("work_area_pane", xmPanedWidgetClass, g_main_w, pane_args, XtNumber(pane_args));

    ui_setup_tool_area(work_area_pane);
    ui_setup_scroll_area(work_area_pane);

    XtManageChild(work_area_pane);

    Widget command_area = ui_setup_command_area(g_main_w);

    XtVaSetValues(g_main_w,
            XmNmenuBar, menu,
            XmNworkWindow, work_area_pane,
            XmNmessageWindow, command_area,
            NULL);

    XtManageChild(g_main_w);
    XtRealizeWidget(top_wid);

    if (!paint_init(&g_paint_ctx))
    {
        fprintf(stderr, "failed to setup paint context");
        exit(1);
    }

	atexit(ui_drawing_cleanup);

    if (argc >= 2)
    {
        const char* open_path = argv[1];
        const char* error_message = NULL;
        if (!paint_open_file(&g_paint_ctx, open_path, &error_message))
        {
            fprintf(stderr, "%s: %s\n", error_message, open_path);
            exit(1);
        }
    }

    ui_refresh_title();

    g_ready = 1;

    ui_refresh_drawing(1);
    ui_set_color(g_main_w, g_paint_ctx.fg_color, 1);
    ui_set_color(g_main_w, g_paint_ctx.bg_color, 0);
    ui_refresh_tool();
    XtAppMainLoop(g_app);
	return 0;
}
