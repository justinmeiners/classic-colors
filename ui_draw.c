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

// UGLY CODE
// We need to discern X11 rules and follow them,
// so lots of checks and citations.

#ifdef FEATURE_SHM
// https://www.x.org/releases/X11R7.7/doc/xextproto/shm.html
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/errno.h>
#include <X11/extensions/XShm.h>
#endif

#include <Xm/DrawingA.h>
#include <Xm/ScrollBar.h>
#include <Xm/ScrolledW.h>

typedef struct
{
    int use_shm;
    XImage* x_image;
    Visual* x_visual;
    GC x_gc;
    XVisualInfo x_visual_info;

#ifdef FEATURE_SHM
    // "it will have a lifetime at least as long as that of the ... XImage"
    XShmSegmentInfo shminfo;
#endif
} DrawInfo;


DrawInfo g_buffer;
Widget draw_area = NULL;
Widget scroll_w = NULL;
XtIntervalId g_down_timer = 0;

static
void copy_bitmap_to_ximage_(XImage* dest, const Bitmap* src, const XVisualInfo* info)
{
    int comps[4];
    int i;

    // as we find more pixel formats we will need to add additional cases here
    if (info->red_mask == 0x00FF0000
            && info->green_mask == 0x0000FF00
            && info->blue_mask == 0x000000FF)
    {
        if (dest->bitmap_unit == 32 && dest->format == ZPixmap)
        {
            // Fast path
            // https://groups.google.com/g/comp.windows.x/c/c4tjX7UiuVU
            uint8_t* raw_data = (uint8_t*)src->data;
            memcpy(dest->data, raw_data + 1, src->w * src->h * sizeof(uint32_t));
        }
        else
        {
            // General path
            for (int y = 0; y < src->h; ++y)
            {
                for (int x = 0; x < src->w; ++x)
                {
                    XPutPixel(dest, x, y, src->data[x + y * src->w] >> 8);
                }
            }
        }
    }
    else
    {
        fprintf(stderr, "X11 is not in a compatible pixel format.");
        exit(2);
    }
}


#define DRAW_PADDING 64

static
void update_scroll_()
{
    int n = 0;
    Arg args[UI_ARGS_MAX];
    PaintContext* ctx = &g_paint_ctx;

    int max_w = paint_w(ctx) + DRAW_PADDING;
    int max_h = paint_h(ctx) + DRAW_PADDING;

    int h_slider_size = MIN(ctx->viewport.w / ctx->viewport.zoom, max_w);
    int v_slider_size = MIN(ctx->viewport.h / ctx->viewport.zoom, max_h);

    ctx->viewport.paint_x = MAX(ctx->viewport.paint_x, 0);
    ctx->viewport.paint_y = MAX(ctx->viewport.paint_y, 0);

    ctx->viewport.paint_x = MIN(ctx->viewport.paint_x, max_w - h_slider_size);
    ctx->viewport.paint_y = MIN(ctx->viewport.paint_y, max_h - v_slider_size);


    Widget hsb, vsb;
    n = 0;
    XtSetArg(args[n], XmNhorizontalScrollBar, &hsb); ++n;
    XtSetArg(args[n], XmNverticalScrollBar, &vsb); ++n;
    XtGetValues(scroll_w, args, n);

    n = 0;
    XtSetArg(args[n], XmNvalue, ctx->viewport.paint_x); ++n;
    XtSetArg(args[n], XmNmaximum, max_w); ++n;
    XtSetArg(args[n], XmNsliderSize, h_slider_size); ++n;
    XtSetArg(args[n], XmNincrement, 8); ++n;
    XtSetValues(hsb, args, n);

    n = 0;
    XtSetArg(args[n], XmNvalue, ctx->viewport.paint_y); ++n;
    XtSetArg(args[n], XmNmaximum, max_h); ++n;
    XtSetArg(args[n], XmNsliderSize, v_slider_size); ++n;
    XtSetArg(args[n], XmNincrement, 8); ++n;
    XtSetValues(vsb, args, n);
}

static
void resize_view_()
{
    int n = 0;
    Arg args[UI_ARGS_MAX];
    PaintContext* ctx = &g_paint_ctx;

    int view_w = 0;
    int view_h = 0;

    n = 0;
    XtSetArg(args[n], XmNwidth, &view_w); ++n;
    XtSetArg(args[n], XmNheight, &view_h); ++n;
    XtGetValues(draw_area, args, n);

    ctx->viewport.w = view_w;
    ctx->viewport.h = view_h;
}

#define TIMER_INTERVAL 25

static
void timer_fire_(XtPointer client_data, XtIntervalId* id)
{
    g_down_timer = 0;

    PaintContext* ctx = &g_paint_ctx;

    PaintTool tool = (PaintTool)client_data;
    if (tool == ctx->tool && ctx->request_tool_timer)
    {
        paint_tool_update(ctx);
        ui_refresh_drawing(0);
        g_down_timer = XtAppAddTimeOut(ui_app(), TIMER_INTERVAL, timer_fire_, (XtPointer)ctx->tool);
    }
}

static
void cb_draw_stroke_(Widget w, void* client_data, XEvent* event, Boolean* continue_dispatch)
{
    Display *dpy = event->xany.display;
    PaintContext* ctx = &g_paint_ctx;

    int x, y;
    int shouldRefresh = 0;
    // http://xahlee.info/linux/linux_x11_mouse_button_number.html
    if (event->xbutton.button > 3) {
        return;
    }

    switch (event->type)
    {
        case ButtonPress:
            ctx->tool_force_align = (event->xbutton.state & ShiftMask);

            viewport_coord_to_paint(&ctx->viewport, event->xbutton.x, event->xbutton.y, &x, &y);
            paint_tool_down(ctx, x, y, event->xbutton.button);

            if (g_down_timer != 0)
            {
                XtRemoveTimeOut(g_down_timer);
            }
            if (ctx->request_tool_timer)
            {
                timer_fire_((XPointer)ctx->tool, NULL);
            }

            shouldRefresh = 1;
            break;
        case ButtonRelease:
        {
            int should_update_scroll = (ctx->tool == TOOL_MAGNIFIER);

            viewport_coord_to_paint(&ctx->viewport, event->xbutton.x, event->xbutton.y, &x, &y);
            paint_tool_up(ctx, x, y, event->xbutton.button);

            if (g_down_timer != 0)
            {
                XtRemoveTimeOut(g_down_timer);
            }

            ui_refresh_tool();
            shouldRefresh = 1;

            if (should_update_scroll) update_scroll_();

            if (ctx->tool == TOOL_TEXT &&
                    ctx->active_layer == LAYER_OVERLAY)
                ui_setup_text_dialog();
            break;
        }
        case MotionNotify:
            ctx->tool_force_align = (event->xbutton.state & ShiftMask);

            viewport_coord_to_paint(&ctx->viewport, event->xmotion.x, event->xmotion.y, &x, &y);
            paint_tool_move(ctx, x, y);
            shouldRefresh = 1;
            break;
    }


    if (shouldRefresh)
    {
        *continue_dispatch = 0;
        if (ctx->tool == TOOL_EYE_DROPPER)
        {
            ui_set_color(main_w, ctx->fg_color, 1);
            ui_set_color(main_w, ctx->bg_color, 0);
        }
        ui_refresh_drawing(0);
    }
}

void ui_cb_draw_update(Widget widget, XtPointer client_data, XtPointer call_data)
{
    if (g_ready)
    {
        ui_refresh_drawing(1);
    }
}


static
int verify_visual_(Display* display, const Visual* visual, XVisualInfo* out_info)
{
    int visual_count;
    XVisualInfo template;
    template.visualid = XVisualIDFromVisual(g_buffer.x_visual);

    // http://www.mirbsd.org/htman/i386/man3/XGetVisualInfo.htm
    XVisualInfo* info_list = XGetVisualInfo (display, VisualIDMask, &template, &visual_count);
    assert(visual_count == 1);
    *out_info = info_list[0];
    XFree(info_list);


    // 24 or 32 bit depth
    if (info_list->depth != 24 && info_list->depth != 32)
    {
        fprintf(stderr, "XVisual has invalid depth: %d\n", info_list->depth);
        return 0;
    }

    // 8 bits per component
    if (info_list->bits_per_rgb != 8)
    {
        fprintf(stderr, "XVisual has invalid bits per rgb: %d\n", info_list->bits_per_rgb);
        return 0;
    }

    //  TrueColor and DirectColor are the only non-color mapped:
    //  https://tronche.com/gui/x/xlib/window/visual-types.html
    if (info_list->class != TrueColor && info_list->class != DirectColor)
    {
        fprintf(stderr, "XVisual is using a strange: %d\n", info_list->class);
        return 0;
    }

    return 1;
}

Widget ui_setup_draw_area(Widget parent)
{
    DrawInfo* buffer = &g_buffer;
    memset(buffer, 0, sizeof(DrawInfo));

    int n = 0;
    Arg args[UI_ARGS_MAX];

    draw_area = XmCreateDrawingArea(parent, "drawing_area", args, n);

    XtAddCallback(draw_area, XmNinputCallback, ui_cb_draw_update, NULL);
    XtAddEventHandler(
            draw_area,
            ButtonPressMask | ButtonReleaseMask | Button1MotionMask | Button3MotionMask,
            False,
            cb_draw_stroke_,
            NULL
            );
    XtAddCallback(draw_area, XmNexposeCallback, ui_cb_draw_update, NULL);
    XtAddCallback(draw_area, XmNresizeCallback, ui_cb_draw_update, NULL);

    Display* display = XtDisplay(draw_area);

    XGCValues gcv;
    gcv.foreground = BlackPixelOfScreen(XtScreen(draw_area));
    buffer->x_gc = XCreateGC(display, RootWindowOfScreen(XtScreen(draw_area)), GCForeground, &gcv);

    // about visuals
    // https://docs.oracle.com/cd/E19620-01/805-3921/6j3nm4qiu/index.html
 
    // We don't need to pick a visual. It is provided by motif.
    // Just verify it's sane.
    g_buffer.x_visual = DefaultVisual(display, 0);

    if (!verify_visual_(display, g_buffer.x_visual, &g_buffer.x_visual_info))
    {
        exit(1);
    }

    buffer->x_image = NULL;

#ifdef FEATURE_SHM
    buffer->use_shm = XShmQueryExtension(display) == True;
#else
    buffer->use_shm = 0;
#endif
    if (DEBUG_LOG)
    {
        printf("X11 color mask. red: %08lx. green: %08lx. blue: %08lx\n", g_buffer.x_visual_info.red_mask, g_buffer.x_visual_info.green_mask, g_buffer.x_visual_info.blue_mask);
    }

    XtManageChild(draw_area);

    return draw_area;
}

static
void scrolled_(Widget scrollbar, XtPointer client_data, XtPointer call_data)
{
    PaintContext* ctx = &g_paint_ctx;

    int orientation = (int)client_data;
    XmScrollBarCallbackStruct *cbs = (XmScrollBarCallbackStruct *) call_data;
    if (orientation == XmHORIZONTAL)
    {
        ctx->viewport.paint_x = cbs->value;
    }
    else
    {
        ctx->viewport.paint_y = cbs->value;
    }
    ui_refresh_drawing(0);
}

static
void shm_shutdown_(DrawInfo* ctx, Display* dpy)
{
#ifdef FEATURE_SHM
    if (ctx->x_image)
    {
        XShmDetach(dpy, &ctx->shminfo);
        XDestroyImage(ctx->x_image);
        shmdt(ctx->shminfo.shmaddr);
        shmctl(ctx->shminfo.shmid, IPC_RMID, 0);
        ctx->x_image = NULL;
    }
#endif
}

static
int shm_prepare_(DrawInfo* ctx, Display* dpy, const Layer* composite)
{
#ifndef FEATURE_SHM
    assert(0);
    return 0;
#else 
    int w = composite->bitmaps->w;
    int h = composite->bitmaps->h;

    int needs_to_resize = !ctx->x_image
        || (ctx->x_image->width != w)
        || (ctx->x_image->height != h);

    if (!needs_to_resize || w <= 0 || h <= 0)
    {
        return 1;
    }
    shm_shutdown_(ctx, dpy);

    // https://www.x.org/releases/X11R7.7/doc/xextproto/shm.html
    // data: "unless you have already allocated ... you should pass in NULL for the "data" pointer."
    //
    // "there are no "offset", "bitmap_pad", or "bytes_per_line" arguments.
    // These quantities will be defined by the server ... your code needs to abide by them."
    XImage* x_image = XShmCreateImage(dpy, ctx->x_visual, 24, ZPixmap, NULL, &ctx->shminfo, w, h);

    if (!x_image) goto cleanup;
    if (x_image->xoffset != 0 || x_image->bitmap_pad != 32)
    {
        fprintf(stderr, "xshm gave invalid offset or bitmap_pad. format is unusable.\n");
        goto cleanup;
    }

	size_t size = x_image->bytes_per_line * x_image->height;
    ctx->shminfo.shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0777);
    if (ctx->shminfo.shmid == -1)
    {
		// Mac sometimes fails here. with errno = 12 (ENOMEM)
		// https://www.postgresql.org/message-id/F05411AD-3771-4185-A096-26CCB254A1B4%40speakeasy.net
		// https://lists.apple.com/archives/darwin-kernel/2007/Apr/msg00021.html
        // https://www.deepanseeralan.com/tech/playing-with-shared-memory/
		// The space is tight (4mb), but it should have enough.
		// Verify cleanup:
		// ipcs
		// ipcrm -m ...
        fprintf(stderr, "failed to create shared memory: %d. (shmget) %d\n", (int)size, errno);
#ifdef __APPLE__
        if (errno == ENOMEM)
        {
            fprintf(stderr, "detected shared memory failure due to Apple SYSV limit.\n");
        }
#endif
        goto cleanup;
    }

    x_image->data = shmat(ctx->shminfo.shmid, 0, 0);
    ctx->shminfo.shmaddr = x_image->data;
    ctx->shminfo.readOnly = 0;

    if (XShmAttach(dpy, &ctx->shminfo) == 0)
    {
        fprintf(stderr, "failed to attach shared memory\n");
        goto cleanup;
    }


    ctx->x_image = x_image;

    return 1;

cleanup:
    if (x_image)
        XDestroyImage(x_image);

    if (ctx->shminfo.shmid != -1)
    {
	    shmdt(ctx->shminfo.shmaddr);
        shmctl(ctx->shminfo.shmid, IPC_RMID, 0);
    }

    ctx->x_image = NULL;
    return 0;
#endif

}

void ui_refresh_drawing(int clear)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];
    PaintContext* ctx = &g_paint_ctx;

    Window window = XtWindow(draw_area);
    Display* dpy = XtDisplay(draw_area);

    DrawInfo* buffer = &g_buffer;

    if (clear)
    {
        resize_view_();
        update_scroll_();
    }

    paint_composite(ctx);

    const Layer* composite = ctx->layers + LAYER_COMPOSITE;

    int w = composite->bitmaps->w;
    int h = composite->bitmaps->h;
   
    if (buffer->use_shm)
    {
        int result = shm_prepare_(buffer, dpy, composite);

        if (result)
        {
            copy_bitmap_to_ximage_(buffer->x_image, composite->bitmaps, &buffer->x_visual_info);
#ifdef FEATURE_SHM
            XShmPutImage(dpy, window, buffer->x_gc, buffer->x_image, 0, 0, 0, 0, w, h, 0);
#else
			assert(0);
#endif
        }
        else
        {
            fprintf(stderr, "xshm failed. falling back to copying images.\n");
            buffer->use_shm = 0;
        }
    }

    if (!buffer->use_shm)
    {
        size_t s = w * h * 4;
        char* data = malloc(s);
        // What is bitmap_pad? 
        // the documentation isn't clear, but I found:
        // "This is a very roundabout way of describing the pixel size in bits."
        // https://handmade.network/wiki/2834-tutorial_a_tour_through_xlib_and_related_technologies
        XImage* x_image = XCreateImage(dpy, buffer->x_visual, 24, ZPixmap, 0, data, w, h, 32, 0);

        copy_bitmap_to_ximage_(x_image, composite->bitmaps, &buffer->x_visual_info);

        if (!x_image)
        {
            fprintf(stderr, "failed to create display buffer");
            exit(1);
        }

        XPutImage(dpy, window, buffer->x_gc, x_image, 0, 0, 0, 0, w, h);
        XDestroyImage(x_image);
    }

    if (ctx->active_layer == LAYER_OVERLAY)
    {
        const Layer* l = ctx->layers + ctx->active_layer;
        if (l->bitmaps != NULL)
        {
            char dash_pattern[] = { 4, 2 };
            XSetDashes(dpy, buffer->x_gc, 0, dash_pattern, 2);
            XSetLineAttributes(dpy, buffer->x_gc, 1, LineOnOffDash, CapButt, JoinMiter);

            XDrawRectangle(
                    dpy,
                    window,
                    buffer->x_gc,
                    (l->x - ctx->viewport.paint_x) * ctx->viewport.zoom,
                    (l->y - ctx->viewport.paint_y) * ctx->viewport.zoom,
                    (l->bitmaps->w) * ctx->viewport.zoom - 1,
                    (l->bitmaps->h) * ctx->viewport.zoom - 1
           );
        }
    }

    XFlush(dpy);
}



Widget ui_setup_scroll_area(Widget parent)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];

    n = 0;
    XtSetArg(args[n], XmNscrollingPolicy, XmAPPLICATION_DEFINED); n++; 
    XtSetArg(args[n], XmNvisualPolicy, XmVARIABLE); n++; 

    scroll_w = XmCreateScrolledWindow(parent, "scroll", args, n);
    Widget draw_area = ui_setup_draw_area(scroll_w);

    n = 0;
    XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
    Widget vsb = XmCreateScrollBar(scroll_w, "vsb", args, n);
    XtManageChild(vsb);

    n = 0;
    XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
    Widget hsb = XmCreateScrollBar(scroll_w, "hsb", args, n);
    XtManageChild(hsb);

    XtVaSetValues(scroll_w, 
            XmNverticalScrollBar, vsb,
            XmNhorizontalScrollBar, hsb,
            XmNworkWindow, draw_area,
            XmNpaneMaximum, 4096,
            NULL); 

    XtAddCallback(vsb, XmNvalueChangedCallback, scrolled_, (XtPointer)XmVERTICAL);
    XtAddCallback(hsb, XmNvalueChangedCallback, scrolled_, (XtPointer)XmHORIZONTAL);
    XtAddCallback(vsb, XmNdragCallback, scrolled_, (XtPointer)XmVERTICAL);
    XtAddCallback(hsb, XmNdragCallback, scrolled_, (XtPointer)XmHORIZONTAL);

    XtManageChild(scroll_w);
    return scroll_w;
}


void ui_drawing_cleanup(void)
{
    if (draw_area)
    {
        Display* dpy = XtDisplay(draw_area);
        DrawInfo* buffer = &g_buffer;
        shm_shutdown_(buffer, dpy);
    }
}
