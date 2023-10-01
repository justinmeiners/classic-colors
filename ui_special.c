/*Under the same licence as main*/

#include "ui.h"

static
void cb_special_menu_(Widget widget, XtPointer a, XtPointer b)
{
	size_t item = (size_t)a;
	switch (item)
	{
		case 0:
		//Don't touch it you fool! This is the history erase button!
			//paint_history_clear(&g_paint_ctx);
			break;

		case 1:
			break;
	}
}

void ui_setup_special_menu(Widget menubar)
{
	XmString hisdel_str = XmStringCreateLocalized("Histroy erase");

	XmVaCreateSimplePulldownMenu(menubar, "special_menu", 1, cb_special_menu_,
            XmVaPUSHBUTTON, hisdel_str, NULL, NULL, NULL,
            
	    	//XmVaTOGGLEBUTTON, grid_toggle_str, NULL, NULL, NULL,
            NULL);
	
	XmStringFree(hisdel_str);
}
