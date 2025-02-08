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

#ifndef UI_H
#define UI_H


#include <X11/Xlib.h>
#include "common.h"

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/DialogS.h>
#include <Xm/MessageB.h>
#include <Xm/PushB.h>

#include <Xm/Frame.h>
#include <Xm/Command.h>
#include <Xm/Paned.h>
#include <Xm/TextF.h>
#include <Xm/Text.h>
#include <Xm/Separator.h>

#include "paint.h"

#define UI_ARGS_MAX 32

#define UI_NAME_MAX 1024


// https://www.ibiblio.org/pub/X11/contrib/faqs/Motif-FAQ.html

extern PaintContext g_paint_ctx;
extern Widget g_main_w;
extern XtAppContext g_app;
extern int g_ready;

void ui_setup_file_menu(Widget menubar);
void ui_setup_edit_menu(Widget menubar);
void ui_setup_image_menu(Widget menubar);
void ui_setup_view_menu(Widget menubar);

Widget ui_setup_tool_area(Widget parent);
Widget ui_setup_command_area(Widget parent);

void ui_refresh_drawing(int clear);
void ui_drawing_cleanup(void);

Widget ui_start_editing_text(void);
void ui_disable_editing_text(void);

void ui_set_color(Widget window, uint32_t color, int fg);
Widget ui_setup_scroll_area(Widget parent);

void ui_refresh_tool(void);
void ui_refresh_title(void);
XtAppContext ui_app();

XImage *cc_bitmap_create_ximage(CcBitmap *b, Display *display, Visual *visual);
CcBitmap cc_bitmap_from_ximage(XImage *image);
void cc_bitmap_swap_for_xvisual(CcBitmap *b, const XVisualInfo *info);

#endif
