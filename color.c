#include <assert.h>
#include <stdio.h>
#include "color.h"

#define PREPARE(SRC, DST) cc_color_unpack(SRC, src_comps); cc_color_unpack(DST, dst_comps);

void color_blending_test()
{
    printf("testing blending\n");
    int src_comps[4];
    int dst_comps[4];

    {
        PREPARE(COLOR_WHITE, COLOR_WHITE);
        cc_color_blend_invert(src_comps, dst_comps);
        assert(cc_color_pack(dst_comps) == COLOR_BLACK);
    }
    {
        PREPARE(COLOR_WHITE, COLOR_BLACK);
        cc_color_blend_invert(src_comps, dst_comps);
        assert(cc_color_pack(dst_comps) == COLOR_WHITE);
    }
    {
        PREPARE(0xFF000080, 0xFFFFFFFF);
        cc_color_blend_overlay(src_comps, dst_comps);
        assert(cc_color_pack(dst_comps) == 0xFF7F7FFF);
    }
    {
        PREPARE(0xFF000080, 0xFFFFFFFF);
        cc_color_blend_full(src_comps, dst_comps);
        assert(cc_color_pack(dst_comps) == 0xFF7F7FFF);
    }
    {
        PREPARE(0xFF000080, 0xFFFFFF80);
        cc_color_unpack(0xFF000080, dst_comps);
        cc_color_unpack(0xFF000080, src_comps);

        cc_color_blend_full(src_comps, dst_comps);
        assert(cc_color_pack(dst_comps) == 0xFF0000c0);
    }

}

#undef PREPARE
