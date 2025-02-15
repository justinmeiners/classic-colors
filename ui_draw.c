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

#include <errno.h>
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

    Visual* x_visual;
    GC x_gc;
    XVisualInfo x_visual_info;
    XColor select_bright;
    XColor select_dark;

    XImage* x_image;

    size_t shm_index;
    XImage *shm_image[2];

#ifdef FEATURE_SHM
    // "it will have a lifetime at least as long as that of the ... XImage"
    XShmSegmentInfo shminfo;
#endif
} DrawInfo;


DrawInfo g_draw_info;
Widget draw_area = NULL;
Widget scroll_w = NULL;

static
void update_scroll_(void)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];
    PaintContext* ctx = &g_paint_ctx;

    const int draw_padding = 64;

    int max_w = paint_w(ctx) + draw_padding;
    int max_h = paint_h(ctx) + draw_padding;

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
void resize_view_(void)
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

static
void schedule_hold_down_timer_(Time time_interval, PaintTool tool);

static
void fire_hold_down_timer_(XtPointer client_data, XtIntervalId* id)
{
    PaintContext* ctx = &g_paint_ctx;

    PaintTool tool = (PaintTool)client_data;
    if (tool == ctx->tool && ctx->request_tool_timer)
    {
        paint_tool_update(ctx);
        ui_refresh_drawing(0);

        const Time hold_update = 25;
        schedule_hold_down_timer_(hold_update, tool);
    }
}

static
void schedule_hold_down_timer_(Time time_interval, PaintTool tool)
{
    static XtIntervalId timer = 0;

    if (time_interval == 0) {
        if (timer != 0) {
            XtRemoveTimeOut(timer);
            timer = 0;
        }
        fire_hold_down_timer_((XtPointer)tool, &timer);
    }

    timer = XtAppAddTimeOut(ui_app(), time_interval, fire_hold_down_timer_, (XtPointer)tool);
}

static
void ui_cb_draw_input_(Widget scrollbar, XtPointer client_data, XtPointer call_data)
{
    PaintContext* ctx = &g_paint_ctx;

    XmDrawingAreaCallbackStruct *cbs = (XmDrawingAreaCallbackStruct*)call_data;
    XEvent *event = cbs->event;
    if (cbs->reason != XmCR_INPUT)
    {
        if (g_ready) {
            if (DEBUG_LOG) {
                fprintf(stderr, "other draw update\n");
            }
            ui_refresh_drawing(1);
        }
        return;
    }

    int x, y;
    int should_refresh = 0;
    switch (event->type)
    {
        case ButtonPress:
            // http://xahlee.info/linux/linux_x11_mouse_button_number.html
            if (event->xbutton.button > 3) break;
            ctx->tool_force_align = (event->xbutton.state & ShiftMask);

            cc_viewport_coord_to_paint(&ctx->viewport, event->xbutton.x, event->xbutton.y, &x, &y);
            paint_tool_down(ctx, x, y, event->xbutton.button);

            schedule_hold_down_timer_(0, ctx->tool);

            should_refresh = 1;
            break;
        case ButtonRelease:
        {
            if (event->xbutton.button > 3) break;
            int should_update_scroll = (ctx->tool == TOOL_MAGNIFIER);

            cc_viewport_coord_to_paint(&ctx->viewport, event->xbutton.x, event->xbutton.y, &x, &y);
            paint_tool_up(ctx, x, y, event->xbutton.button);

            ui_refresh_tool();
            should_refresh = 1;

            if (should_update_scroll) update_scroll_();
            if (paint_is_editing_text(ctx)) ui_start_editing_text();

            break;
        }
        case MotionNotify:
        {
            // limit framerate to avoid maxing CPU unnecessarily
            const Time min_time_between_frames = 15;

            static Time previous_notify_time = 0;
            if (event->xmotion.time < previous_notify_time + min_time_between_frames) break;

            previous_notify_time = event->xmotion.time;

            ctx->tool_force_align = (event->xbutton.state & ShiftMask);

            cc_viewport_coord_to_paint(&ctx->viewport, event->xmotion.x, event->xmotion.y, &x, &y);
            paint_tool_move(ctx, x, y);
            should_refresh = 1;
            break;
        }
        case KeyPress:
        {
            int key_sym = XLookupKeysym(&event->xkey, 0);
            if (key_sym == XK_Escape)
            {
                paint_tool_cancel(ctx);
            }
            break;
        }
    }

    if (!should_refresh) return;

    if (ctx->tool == TOOL_EYE_DROPPER)
    {
        ui_set_color(g_main_w, ctx->fg_color, 1);
        ui_set_color(g_main_w, ctx->bg_color, 0);
    }
    ui_refresh_drawing(0);
}

void ui_cb_draw_update(Widget scrollbar, XtPointer client_data, XtPointer call_data)
{
    if (!g_ready) {
        return;
    }
    if (DEBUG_LOG) {
        fprintf(stderr, "draw update\n");
    }
    ui_refresh_drawing(1);
}

static
int verify_visual_(Display* display, const Visual* visual, XVisualInfo* out_info)
{
    int visual_count;
    XVisualInfo template;
    template.visualid = XVisualIDFromVisual(g_draw_info.x_visual);

    // http://www.mirbsd.org/htman/i386/man3/XGetVisualInfo.htm
    XVisualInfo* info_list = XGetVisualInfo (display, VisualIDMask, &template, &visual_count);
    assert(visual_count == 1);

    // 24 or 32 bit depth
    if (info_list->depth != 24 && info_list->depth != 32)
    {
        fprintf(stderr, "XVisual has invalid depth: %d\n", info_list->depth);
        XFree(info_list);
        return 0;
    }

    // This check for bits_per_rgb does not provide the expected value.
    // if (info_list->bits_per_rgb != 8)

    //  TrueColor and DirectColor are the only non-color mapped:
    //  https://tronche.com/gui/x/xlib/window/visual-types.html
    if (info_list->class != TrueColor && info_list->class != DirectColor)
    {
        fprintf(stderr, "XVisual is using a strange: %d\n", info_list->class);
        XFree(info_list);
        return 0;
    }

    *out_info = info_list[0];
    XFree(info_list);
    return 1;
}

Widget ui_setup_draw_area(Widget parent)
{
    DrawInfo* buffer = &g_draw_info;
    memset(buffer, 0, sizeof(DrawInfo));

    int n = 0;
    Arg args[UI_ARGS_MAX];

    // Need to set custom translations in order to get mouse motion events.
    // The PDF manual was helpful for setting up these translations.
    // We used to use XtAddEventHandler which worked for mouse motion, but disrupted key events.
    String translations = "<Btn1Motion>: DrawingAreaInput() ManagerGadgetButtonMotion()\n \
                           <Btn1Up>: DrawingAreaInput() ManagerGadgetActivate()\n \
                           <Btn1Down>: DrawingAreaInput() ManagerGadgetArm()\n \
                           <Btn3Motion>: DrawingAreaInput() ManagerGadgetButtonMotion()\n \
                           <Btn3Up>: DrawingAreaInput() ManagerGadgetActivate()\n \
                           <Btn3Down>: DrawingAreaInput() ManagerGadgetArm()\n \
                           <Key>osfSelect: DrawingAreaInput() ManagerGadgetSelect()\n \
                           <Key>osfActivate: DrawingAreaInput() ManagerParentActivate()\n \
                           <Key>osfHelp: DrawingAreaInput() ManagerGadgetHelp()\n \
                           <KeyDown>: DrawingAreaInput() ManagerGadgetKeyInput()\n \
                           <KeyUp>: DrawingAreaInput()";

    XtSetArg(args[n], XmNtranslations, XtParseTranslationTable(translations)); ++n;
    draw_area = XmCreateDrawingArea(parent, "drawing_area", args, n);

    // not working for some reason
    XtAddCallback(draw_area, XmNinputCallback, ui_cb_draw_input_, NULL);
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
    g_draw_info.x_visual = DefaultVisual(display, 0);

    if (!verify_visual_(display, g_draw_info.x_visual, &g_draw_info.x_visual_info))
    {
        exit(1);
    }

#ifdef FEATURE_SHM
    buffer->x_image = NULL;
    buffer->use_shm = XShmQueryExtension(display) == True;
#else
    buffer->use_shm = 0;
#endif

    if (DEBUG_LOG)
    {
        fprintf(stderr,
                "X11 color mask. red: %08lx. green: %08lx. blue: %08lx\n",
                g_draw_info.x_visual_info.red_mask, g_draw_info.x_visual_info.green_mask, g_draw_info.x_visual_info.blue_mask
              );
    }

    Colormap screen_colormap = DefaultColormap(display, DefaultScreen(display));

    XParseColor(display, screen_colormap, "#000000", &buffer->select_dark);
    XAllocColor(display, screen_colormap, &buffer->select_dark);
    XParseColor(display, screen_colormap, "#FFFFFF", &buffer->select_bright);
    XAllocColor(display, screen_colormap, &buffer->select_bright);

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
void shm_destroy_image_(DrawInfo* ctx, Display* dpy)
{
    if (!ctx->shm_image[0]) {
        return;
    }
    ctx->x_image = NULL;

#ifdef FEATURE_SHM
    if (ctx->shminfo.shmid != -1) {
        XShmDetach(dpy, &ctx->shminfo);
        shmdt(ctx->shminfo.shmaddr);
        shmctl(ctx->shminfo.shmid, IPC_RMID, 0);
    }
#endif

    for (size_t i = 0; i < 2; ++i) {
        if (ctx->shm_image[i]) {
            XDestroyImage(ctx->shm_image[i]);
            ctx->shm_image[i] = NULL;
        }
    }
}

static
int shm_prepare_(DrawInfo* ctx, Display* dpy, int w, int h)
{
#ifndef FEATURE_SHM
    assert(0);
    return 0;
#else 
    if (DEBUG_LOG) fprintf(stderr, "resizing shm buffer\n");

    shm_destroy_image_(ctx, dpy);

    // https://www.x.org/releases/X11R7.7/doc/xextproto/shm.html
    // data: "unless you have already allocated ... you should pass in NULL for the "data" pointer."
    //
    // "there are no "offset", "bitmap_pad", or "bytes_per_line" arguments.
    // These quantities will be defined by the server ... your code needs to abide by them."
    ctx->shm_image[0] = XShmCreateImage(dpy, ctx->x_visual, 24, ZPixmap, NULL, &ctx->shminfo, w, h);

    if (!ctx->shm_image[0]) {
        return 0;
    }

    if (ctx->shm_image[0]->bytes_per_line != w * sizeof(CcPixel)) {
        fprintf(stderr, "bad alignment\n");
        XDestroyImage(ctx->shm_image[0]);
        return 0;
    }

    assert(ctx->shm_image[0]->bitmap_pad == 32);

	size_t image_size = ctx->shm_image[0]->bytes_per_line * ctx->shm_image[0]->height;
    size_t buffer_size = image_size * 2;

    ctx->shminfo.shmid = shmget(IPC_PRIVATE, buffer_size, IPC_CREAT | 0777);
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
        fprintf(stderr, "failed to create shared memory: %d. (shmget) %d\n", (int)buffer_size, errno);
#ifdef __APPLE__
        if (errno == ENOMEM)
        {
            fprintf(stderr, "detected shared memory failure due to Apple SYSV limit.\n");
        }
#endif
        goto cleanup;
    }

    ctx->shminfo.shmaddr = shmat(ctx->shminfo.shmid, 0, 0);
    // server should not modify image
    ctx->shminfo.readOnly = 1;

    if (XShmAttach(dpy, &ctx->shminfo) == 0)
    {
        fprintf(stderr, "failed to attach shared memory\n");
        goto cleanup;
    }

    ctx->shm_image[0]->data = ctx->shminfo.shmaddr;

    ctx->shm_image[1] = XShmCreateImage(dpy, ctx->x_visual, 24, ZPixmap, NULL, &ctx->shminfo, w, h);
    ctx->shm_image[1]->data = ctx->shminfo.shmaddr + image_size;

    if (!ctx->shm_image[1]) goto cleanup;
    return 1;

cleanup:
    shm_destroy_image_(ctx, dpy);
    return 0;
#endif
}

static
void framebuffer_prepare_(DrawInfo* buffer, Display* dpy, int w, int h)
{
    int needs_to_resize = !buffer->x_image
        || (buffer->x_image->width != w)
        || (buffer->x_image->height != h);


    if (buffer->use_shm)
    {
        if (!needs_to_resize || w <= 0 || h <= 0) {
            // swap buffers
            buffer->shm_index = !buffer->shm_index;
            buffer->x_image = buffer->shm_image[buffer->shm_index];
            return;
        }
        if (shm_prepare_(buffer, dpy, w, h)) {
            buffer->x_image = buffer->shm_image[0];
        } else {
            fprintf(stderr, "xshm failed. falling back to copying images.\n");
            buffer->use_shm = 0;
        }
    }

    if (!needs_to_resize || w <= 0 || h <= 0) {
        return;
    }

    if (!buffer->use_shm) {
        if (buffer->x_image) XDestroyImage(buffer->x_image);

        CcBitmap b = {
            .w = w,
            .h = h
        };
        cc_bitmap_alloc(&b);
        buffer->x_image = cc_bitmap_create_ximage(&b, dpy, buffer->x_visual);
    }

    if (!buffer->x_image) {
        fprintf(stderr, "failed to make ximage buffer");
        exit(1);
    }

    assert(buffer->x_image->width == w);
    assert(buffer->x_image->height == h);
}

void ui_refresh_drawing(int clear)
{
    int n = 0;
    Arg args[UI_ARGS_MAX];
    PaintContext* ctx = &g_paint_ctx;

    Window window = XtWindow(draw_area);
    Display* dpy = XtDisplay(draw_area);

    DrawInfo* buffer = &g_draw_info;

    if (clear)
    {
        resize_view_();
        update_scroll_();
    }

    int w = ctx->viewport.w;
    int h = ctx->viewport.h;

    framebuffer_prepare_(buffer, dpy, w, h);

    CcBitmap b = {
        .w = w,
        .h = h,
        .data = (CcPixel *)buffer->x_image->data
    };
    paint_composite(ctx, &b);
    cc_bitmap_swap_for_xvisual(&b, &buffer->x_visual_info);

    if (buffer->use_shm) {
#ifdef FEATURE_SHM
        XShmPutImage(dpy, window, buffer->x_gc, buffer->x_image, 0, 0, 0, 0, w, h, 0);
#endif
    } else {
        XPutImage(dpy, window, buffer->x_gc, buffer->x_image, 0, 0, 0, 0, w, h);
    }

    if (ctx->active_layer == LAYER_OVERLAY)
    {
        const CcLayer* l = ctx->layers + ctx->active_layer;
        if (l->bitmap.w != 0)
        {
            int x = (l->x - ctx->viewport.paint_x) * ctx->viewport.zoom;
            int y = (l->y - ctx->viewport.paint_y) * ctx->viewport.zoom;
            int w = (l->bitmap.w) * ctx->viewport.zoom - 1;
            int h = (l->bitmap.h) * ctx->viewport.zoom - 1;

            XSetLineAttributes(dpy, buffer->x_gc, 1, LineOnOffDash, CapButt, JoinMiter);

            char dash_pattern[] = { 4, 4 };
            XSetDashes(dpy, buffer->x_gc, 0, dash_pattern, 2);
            XSetForeground(dpy, buffer->x_gc, buffer->select_bright.pixel);
            XDrawRectangle(dpy, window, buffer->x_gc, x, y, w, h);

            XSetDashes(dpy, buffer->x_gc, 4, dash_pattern, 2);
            XSetForeground(dpy, buffer->x_gc, buffer->select_dark.pixel);
            XDrawRectangle(dpy, window, buffer->x_gc, x, y, w, h);
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
        DrawInfo* ctx = &g_draw_info;
        shm_destroy_image_(ctx, dpy);
        if (ctx->x_image) {
            XDestroyImage(ctx->x_image);
        }
    }
}
