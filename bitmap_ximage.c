#include "bitmap.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static inline
CcPixel swap1_pixel_(CcPixel x)
{
  return x >> 8;
  //return cc_color_swap(x) >> 8;
  /*
    return ((x & 0xFF000000) >> 24) |
        ((x & 0x00FF0000) >> 8)  |
        ((x & 0x0000FF00) << 8)  |
        ((x & 0x000000FF) << 24);
        */
}

static inline
void swap1_(
        char *dst,
        size_t stride,
        int w,
        int h) {

    while (h)
    {

        CcPixel *row = (CcPixel *)dst;
        for (int col = 0; col < w; ++col) {
            row[col] = swap1_pixel_(row[col]);
        }
        dst += stride;
        --h;
    }
}

void cc_bitmap_swap_for_xvisual(CcBitmap *b, const XVisualInfo *info)
{
    // https://groups.google.com/g/comp.windows.x/c/c4tjX7UiuVU
    if (info->red_mask == 0x00FF0000
         && info->green_mask == 0x0000FF00
         && info->blue_mask == 0x000000FF) {
        swap1_((char *)b->data, sizeof(CcPixel) * b->w, b->w, b->h);
    } else {
        assert(0); // need to change formats
    }
}

XImage *cc_bitmap_create_ximage(CcBitmap *b, Display *display, Visual *visual)
{
    // What is bitmap_pad?
    // the documentation isn't clear, but I found:
    // "This is a very roundabout way of describing the pixel size in bits."
    // https://handmade.network/wiki/2834-tutorial_a_tour_through_xlib_and_related_technologies
    return XCreateImage(display, visual, 24, ZPixmap, 0, (char *)b->data, b->w, b->h, 32, b->w * sizeof(CcPixel));
}

