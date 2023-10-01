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

#define MAX_ZOOM 32
#define MIN_ZOOM 1

static
void cb_view_menu_(Widget widget, XtPointer a, XtPointer b)
{
    PaintContext* ctx = &g_paint_ctx;

    size_t item = (size_t)a;

    switch (item) 
    {
        case 0:
        {
            int new_zoom = MIN(ctx->viewport.zoom * 2, MAX_ZOOM);
            ctx->viewport = cc_viewport_zoom_centered(&ctx->viewport, new_zoom);
            break;
        }
        case 1:
        {
            int new_zoom = MAX(ctx->viewport.zoom / 2, MIN_ZOOM);
            ctx->viewport = cc_viewport_zoom_centered(&ctx->viewport, new_zoom);
            break;
        }
        case 2:
            ctx->viewport.zoom = 1;
            ctx->viewport.paint_x = ctx->viewport.paint_y = 0;
            break;
        case 3:
            break;
    }

    ui_refresh_drawing(1);
}
 
void ui_setup_view_menu(Widget menubar)
{
    XmString zoom_in_str = XmStringCreateLocalized("Zoom In");
    XmString zoom_in_key = XmStringCreateLocalized("+");
    XmString zoom_out_str = XmStringCreateLocalized("Zoom Out");
    XmString zoom_out_key = XmStringCreateLocalized("-");
    XmString zoom_reset_str = XmStringCreateLocalized("Reset");
	XmString grid_toggle_str = XmStringCreateLocalized("Show grid");
    XmString grid_toggle_key = XmStringCreateLocalized("G");


    XmVaCreateSimplePulldownMenu(menubar, "view_menu", 2, cb_view_menu_,
            XmVaPUSHBUTTON, zoom_in_str, 'Z', "<Key>plus", zoom_in_key,
            XmVaPUSHBUTTON, zoom_out_str, 'O', "<Key>minus", zoom_out_key,
            XmVaPUSHBUTTON, zoom_reset_str, 'R', NULL, NULL, 
            
	    	XmVaTOGGLEBUTTON, grid_toggle_str, 'G', "<Key>G", grid_toggle_key,
            NULL, NULL, NULL, NULL, NULL);

    XmStringFree(zoom_reset_str);
    XmStringFree(zoom_in_str);
    XmStringFree(zoom_in_key);
    XmStringFree(zoom_out_str);
    XmStringFree(zoom_out_key);
    XmStringFree(grid_toggle_str);
    XmStringFree(grid_toggle_key);

}

