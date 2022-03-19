#include <assert.h>
#include "color.h"

#define PREPARE(SRC, DST) color_unpack(SRC, src_comps); color_unpack(DST, dst_comps);

void color_blending_test()
{
    printf("testing blending\n");
    int src_comps[4];
    int dst_comps[4];

    {
        PREPARE(COLOR_WHITE, COLOR_WHITE);
        color_blend_invert(src_comps, dst_comps);
        assert(color_pack(dst_comps) == COLOR_BLACK);
    }
    {
        PREPARE(COLOR_WHITE, COLOR_BLACK);
        color_blend_invert(src_comps, dst_comps);
        assert(color_pack(dst_comps) == COLOR_WHITE);
    }
    {
        PREPARE(0xFF000080, 0xFFFFFFFF);
        color_blend_overlay(src_comps, dst_comps);
        assert(color_pack(dst_comps) == 0xFF7F7FFF);
    }
    {
        PREPARE(0xFF000080, 0xFFFFFFFF);
        color_blend_full(src_comps, dst_comps);
        assert(color_pack(dst_comps) == 0xFF7F7FFF);
    }
    {
        PREPARE(0xFF000080, 0xFFFFFF80);
        color_unpack(0xFF000080, dst_comps);
        color_unpack(0xFF000080, src_comps);

        color_blend_full(src_comps, dst_comps);
        assert(color_pack(dst_comps) == 0xFF0000c0);
    }

}

#undef PREPARE
