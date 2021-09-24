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

static
void edit_menu_(Widget w, XtPointer a, XtPointer b)
{
    size_t item = (size_t)a;

    switch (item) 
    {
        case 0:
            paint_undo(&g_paint_ctx);
            ui_refresh_drawing(1);
            break;
        case 1:
            paint_redo(&g_paint_ctx);
            ui_refresh_drawing(1);
            break;
        case 2:
            paint_cut(&g_paint_ctx);
            ui_refresh_drawing(0);
            break;
        case 3:
            paint_copy(&g_paint_ctx);
            break;
        case 4:
            paint_paste(&g_paint_ctx);
            ui_refresh_drawing(0);
            ui_refresh_tool();
            break;
        case 5:
            paint_select_clear(&g_paint_ctx);
            ui_refresh_drawing(0);
            ui_refresh_tool();
            break;
        case 6:
            paint_select_all(&g_paint_ctx);
            ui_refresh_drawing(0);
            ui_refresh_tool();
            break;
    }
}


void ui_setup_edit_menu(Widget menubar)
{
    // accelerator format: https://www.x.org/releases/X11R7.5/doc/Xt/Xt.pdf
    XmString undo_str = XmStringCreateLocalized("Undo");
    XmString undo_key = XmStringCreateLocalized("Ctrl+Z");

    XmString redo_str = XmStringCreateLocalized("Redo");
    XmString redo_key = XmStringCreateLocalized("Ctrl+Y");

    XmString cut_str = XmStringCreateLocalized("Cut");
    XmString cut_key = XmStringCreateLocalized("Ctrl+X");

    XmString copy_str = XmStringCreateLocalized("Copy");
    XmString copy_key = XmStringCreateLocalized("Ctrl+C");

    XmString paste_str = XmStringCreateLocalized("Paste");
    XmString paste_key = XmStringCreateLocalized("Ctrl+V");

    XmString clear_str = XmStringCreateLocalized("Clear Selection");
    XmString clear_key = XmStringCreateLocalized("Ctrl+Shift+A");

    XmString select_str = XmStringCreateLocalized("Select All");
    XmString select_key = XmStringCreateLocalized("Ctrl+A");


    XmVaCreateSimplePulldownMenu(menubar, "edit_menu", 1, edit_menu_,
            XmVaPUSHBUTTON, undo_str, 'U', "Ctrl<Key>z", undo_key,
            XmVaPUSHBUTTON, redo_str, 'R', "Ctrl<Key>y", redo_key,
            XmVaSEPARATOR,
            XmVaPUSHBUTTON, cut_str, 't', "Ctrl<Key>x", cut_key,
            XmVaPUSHBUTTON, copy_str, 'C', "Ctrl<Key>c", copy_key,
            XmVaPUSHBUTTON, paste_str, 'P', "Ctrl<Key>v", paste_key,
            XmVaPUSHBUTTON, clear_str, 'l', "Ctrl Shift<Key>a", clear_key,
            XmVaPUSHBUTTON, select_str, 'A', "Ctrl<Key>a", select_key,

            NULL);

    XmStringFree(paste_str);
    XmStringFree(paste_key);

    XmStringFree(copy_str);
    XmStringFree(copy_key);

    XmStringFree(cut_str);
    XmStringFree(cut_key);

    XmStringFree(redo_str);
    XmStringFree(redo_key);

    XmStringFree(undo_key);
    XmStringFree(undo_str);
}


