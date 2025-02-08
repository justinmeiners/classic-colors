#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "color.h"

static
void test_fixed_point_(void)
{
    for (int16_t c = 0; c <= 255; ++c) {
        assert(fixed_mul_255(c, 255) == c);
    }
}

static
void test_invert_(void)
{
    assert(cc_color_blend_invert(COLOR_WHITE, COLOR_WHITE) == COLOR_BLACK);
    assert(cc_color_blend_invert(COLOR_WHITE, COLOR_BLACK) == COLOR_WHITE);

    uint8_t comps[4] = { 0, 0, 0, 0xFF };

    // confirm it does an invert like we expect
    for (int c = 0; c <= 255; ++c) {
        comps[1] = c;
        comps[3] = c;

        CcPixel dst = cc_color_pack(comps);
        CcPixel result = cc_color_blend_invert(COLOR_WHITE, dst);

        uint8_t comps2[4];
        cc_color_unpack(result, comps2);

        for (int j = 0; j < 3; ++j) {
            assert(comps2[j] == 255 - comps[j]);
        }
        assert(comps2[3] == c);
    }
}

// clear on top of anything is itself
static
void test_clear_(void)
{
    uint8_t comps[4] = { 0, 0, 0, 0xFF };

    // one channel should be sufficient to test
    for (int c = 0; c <= 255; ++c) {
        comps[1] = c;

        CcPixel dst = cc_color_pack(comps);
        assert(dst == cc_color_blend_overlay(COLOR_CLEAR, dst));
        assert(dst == cc_color_blend_full(COLOR_CLEAR, dst));
    }
}

// opaque on top of anything is itself
static
void test_opaque_(void)
{
    uint8_t comps[4] = { 0, 0, 0, 0xFF };

    // one channel should be sufficient to test
    for (int c = 0; c <= 255; ++c) {
        comps[1] = c;
        comps[3] = c;

        CcPixel dst = cc_color_pack(comps);

        assert(COLOR_RED == cc_color_blend_overlay(COLOR_RED, dst));
        assert(COLOR_RED == cc_color_blend_full(COLOR_RED, dst));
    }
}


void color_blending_test(void)
{
    printf("testing blending\n");

    {
        uint8_t comps[] = { 0xFF, 0, 0, 0xFF };
        assert(cc_color_pack(comps) == COLOR_RED);
    }

    {
        uint8_t comps[] = { 0, 0xFF, 0, 0xFF };
        assert(cc_color_pack(comps) == COLOR_GREEN);
    }

    test_fixed_point_();
    test_invert_();
    test_clear_();
    test_opaque_();

    assert(cc_color_blend_overlay(0xFF000080, 0xFFFFFFFF) == 0xFF7F7FFF);
    assert(cc_color_blend_full(0xFF000080, 0xFFFFFFFF) == 0xFF7F7FFF);
    //assert(cc_color_blend_full(0xFF000080, 0xFFFFFF80) == 0xFFc0c0c0);

    assert(cc_color_blend_overlay(COLOR_WHITE, COLOR_WHITE) == COLOR_WHITE);
    assert(cc_color_blend_overlay(COLOR_CLEAR, COLOR_WHITE) == COLOR_WHITE);
    assert(cc_color_blend_full(COLOR_CLEAR, COLOR_WHITE) == COLOR_WHITE);
}

#undef PREPARE
