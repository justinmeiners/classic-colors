
#include "undo_queue.h"
#include <assert.h>

// Undo works by recording every change to the image.
// There are two kinds of changes,
// "Full" that replace every pixel and possibly resize.
// And patches, which are changes to individual regions.
// To perform an undo, you go back to the most recent full image,
// and replay all the patches.
// We optimize for recording the undo state,
// and then pay a higher cost to perform an undo.

static inline
size_t mask_(size_t i)
{
    assert(IS_POW_2(UNDO_QUEUE_MAX));
    return i & (UNDO_QUEUE_MAX - 1);
}

static
void check_invariants_(CcUndo *q)
{
    assert(q->front <= q->back);
    assert(q->front <= q->undo);
    assert(q->undo <= q->back);

    assert(q->back - q->front <= UNDO_QUEUE_MAX);

    if (q->back != q->front) {
        // always starts with a full image
        assert(q->patches[mask_(q->front)].full_image);
    }
}

static
void clear_(UndoPatch *patch)
{
    free(patch->data);
    *patch = (UndoPatch) { 0 };
}

static
void clear_range_(UndoPatch *patches, size_t front, size_t back)
{
    while (front != back) {
        clear_(patches + mask_(front));
        ++front;
    }
}

static
size_t trim_front_(UndoPatch *patches, size_t front, size_t back)
{
    if (front == back) {
        return front;
    }

    assert(patches[mask_(front)].full_image);

    // full
    clear_(patches + mask_(front));
    ++front;

    while (front != back) {
        if (patches[mask_(front)].full_image) {
            return front;
        }
        clear_(patches + mask_(front));
        ++front;
    }
    assert(0);
}

static
size_t find_last_full_(const UndoPatch *patches, size_t front, size_t back)
{
    size_t i = back;
    while (i != front) {
        --i;
        if (patches[mask_(i)].full_image) {
            return i;
        }
    }
    return back;
}

static
CcBitmap replay_(const UndoPatch *patches, size_t front, size_t back)
{
    assert(front != back);
    UndoPatch first_full = patches[mask_(front)];
    assert(first_full.full_image);

    // make a copy of the full image:
    CcBitmap new_canvas = cc_bitmap_decompress(first_full.data, first_full.data_size);
    if (DEBUG_LOG) printf("replaying at checkpint: %d %d\n", new_canvas.w, new_canvas.h);
    ++front;

    // replay each change by blitting all modified rectangles on top
    while (front != back)
    {
        UndoPatch p = patches[mask_(front)];
        assert(!p.full_image);

        // fix up the last undo
        CcBitmap to_blit = cc_bitmap_decompress(p.data, p.data_size);
        assert(to_blit.w == p.rect.w && to_blit.h == p.rect.h);

        if (DEBUG_LOG)
        {
            printf("undo replaying: %d, %d, %d, %d\n",
                    p.rect.x, p.rect.y,
                    p.rect.w, p.rect.h
                  );
        }

        cc_bitmap_blit_unsafe(
                &to_blit,
                &new_canvas,
                0, 0,
                p.rect.x, p.rect.y,
                p.rect.w, p.rect.h,
                COLOR_BLEND_REPLACE
                );

        cc_bitmap_free(&to_blit);
        ++front;
    }

    return new_canvas;
}

void cc_undo_clear(CcUndo* q)
{
    clear_range_(q->patches, q->front, q->back);
    *q = (CcUndo) { 0 };
    check_invariants_(q);
}

// This function is called often and should do minimal work.
static
void push_(CcUndo* q, UndoPatch* patch)
{
    // adding a new state invalidates all redos.
    // cut off everything after undo
    clear_range_(q->patches, q->undo, q->back);
    q->back = q->undo;
    check_invariants_(q);

    // full
    if (q->back - q->front >= UNDO_QUEUE_MAX)
    {
        printf("undo is full. clearing\n");
        q->front = trim_front_(q->patches, q->front, q->back);
    }
    check_invariants_(q);

    // now append the new one
    q->patches[mask_(q->back++)] = *patch;
    q->undo = q->back;

    if (DEBUG_LOG)
    {
        printf("undo count: %d\n", q->back - q->front);
    }
    check_invariants_(q);
}

// Interactive operations (strokes, lines, etc) are short and thus fast.
// Full image operations (resize, stroke, etc) can expect some delay .
void cc_undo_record_change(CcUndo* q, const CcLayer *layer, CcRect changed_region)
{
    CcRect r;
    if (!cc_rect_intersect(cc_layer_rect(layer), changed_region, &r)) return;

    assert(UNDO_QUEUE_MAX > UNDO_QUEUE_MIN);
    if (q->since_last_checkpoint >= UNDO_QUEUE_MAX - UNDO_QUEUE_MIN) {
        // force this to be full
        if (DEBUG_LOG)
        {
            printf("undo force checkpoint\n");
        }

        r = cc_layer_rect(layer);
    }

    // save the modified rectangle in a patch
    UndoPatch patch;
    patch.rect = r;
    patch.data_size = 0;

    if (cc_rect_equal(r, cc_layer_rect(layer)))
    {
        // full image (replay checkpoint)
        patch.full_image = 1;
        patch.data = cc_bitmap_compress(&layer->bitmap, &patch.data_size);

        q->since_last_checkpoint = 0;
    }
    else
    {
        // partial region
        patch.full_image = 0;

        CcBitmap b = {
            .w = r.w,
            .h = r.h
        };
        cc_bitmap_alloc(&b);
        cc_bitmap_blit_unsafe(&layer->bitmap, &b, r.x, r.y, 0, 0, r.w, r.h, COLOR_BLEND_REPLACE);
        patch.data = cc_bitmap_compress(&b, &patch.data_size);
        cc_bitmap_free(&b);

        ++q->since_last_checkpoint;
    }

    if (DEBUG_LOG)
    {
        printf("undo save: %d, %d, %d, %d\n", r.x, r.y, r.w,r.h);
    }

    push_(q, &patch);
}

int cc_undo_can_back(CcUndo* q)
{
    return q->undo != q->front && q->undo != (q->front + 1);
}

int cc_undo_can_forward(CcUndo* q)
{
    return q->undo != q->back;
}

void cc_undo_maybe_back(CcUndo* q, CcLayer* target)
{
    if (!cc_undo_can_back(q))
    {
        return;
    }
    // get to step N-1 by replaying from the most recent full image.
    --q->undo;

    size_t replay = find_last_full_(q->patches, q->front, q->undo);
    assert(replay <= q->undo);

    CcBitmap new_canvas = replay_(q->patches, replay, q->undo);
    cc_layer_set_bitmap(target, &new_canvas);
}

void cc_undo_maybe_forward(CcUndo* q, CcLayer* target)
{
    if (!cc_undo_can_forward(q))
    {
        return;
    }

    UndoPatch p = q->patches[mask_(q->undo)];
    CcBitmap to_blit = cc_bitmap_decompress(p.data, p.data_size);

    if (p.full_image)
    {
        // replace image
        cc_layer_set_bitmap(target, &to_blit);
    }
    else
    {
        // just blit on top
        cc_bitmap_blit_unsafe(
                &to_blit,
                &target->bitmap,
                0, 0,
                p.rect.x, p.rect.y,
                p.rect.w, p.rect.h,
                COLOR_BLEND_REPLACE
                );
        cc_bitmap_free(&to_blit);
    }
    ++q->undo;
}
