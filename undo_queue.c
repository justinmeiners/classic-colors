
#include "undo_queue.h"
#include <assert.h>


#define UNDO_CHECKPOINT_MAX 15

static UndoPatch* find_last_full_(UndoPatch* it, UndoPatch* last)
{
    UndoPatch* last_full = NULL;
    while (it != last)
    {
        if (it->full_image) last_full = it;
        it = it->next;
    }
    return last_full;
}

static Bitmap* undo_replay_(UndoPatch* first_full, UndoPatch* last)
{
    assert(first_full != last && first_full->full_image);

    // make a copy of the full image:
    Bitmap* new_canvas = bitmap_decompress(first_full->data, first_full->data_size); 
    if (DEBUG_LOG) printf("replaying at checkpint: %d %d\n", new_canvas->w, new_canvas->h);

    UndoPatch* it = first_full->next;

    // replay each change by blitting all modified rectangles on top
    while (it != last)
    {
        // fix up the last undo
        Bitmap* to_blit = bitmap_decompress(it->data, it->data_size); 
        assert(to_blit->w == it->rect.w && to_blit->h == it->rect.h);

        if (DEBUG_LOG)
        {
            printf("undo replaying: %d, %d, %d, %d\n",
                    it->rect.x, it->rect.y,
                    it->rect.w, it->rect.h
                  );
        }

        bitmap_blit_unsafe(
                to_blit,
                new_canvas,
                0, 0,
                it->rect.x, it->rect.y,
                it->rect.w, it->rect.h,
                COLOR_BLEND_REPLACE
                );
        bitmap_destroy(to_blit);
        it = it->next;
    }

    return new_canvas;
}

static void destroy_(UndoPatch* start, UndoPatch* end)
{
    while (start != end)
    {
        // kill the redo chain
        UndoPatch* next = start->next;
        undo_patch_destroy(start);
        start = next;
    }
}




// Interactive operations (strokes, lines, etc) are short and thus fast.
// Full image operations (resize, stroke, etc) can expect some delay .
UndoPatch* undo_patch_create(const Bitmap* src, BitmapRect r)
{
    // save the modified rectangle in a patch
    UndoPatch* patch = malloc(sizeof(UndoPatch));
    patch->rect = r;
    patch->next = NULL;
    patch->data_size = 0;

    if (bitmap_rect_equal(r, bitmap_rect(src)))
    {
        // full image (replay checkpoint)
        patch->full_image = 1;
        patch->data = bitmap_compress(src, &patch->data_size);
    }
    else
    {
        patch->full_image = 0;

        Bitmap* b = bitmap_create(r.w, r.h);
        bitmap_blit_unsafe(src, b, r.x, r.y, 0, 0, r.w, r.h, COLOR_BLEND_REPLACE);
        patch->data = bitmap_compress(b, &patch->data_size);
        bitmap_destroy(b);
    }
    return patch;
}

void undo_patch_destroy(UndoPatch* patch)
{
    free(patch->data);
    free(patch);
}


void undo_queue_init(UndoQueue* q)
{
    q->last_undo = NULL;
    q->first_undo = NULL;
    q->undo_count = 0;
}

void undo_queue_clear(UndoQueue* q)
{
    UndoPatch* it = q->first_undo;

    while (it)
    {
        UndoPatch* next = it->next;
        undo_patch_destroy(it);
        it = next;
    }

    q->last_undo = NULL;
    q->first_undo = NULL;
}

void undo_queue_trim(UndoQueue* q, int max_undos)
{
    assert(max_undos > 0);

    int n = q->undo_count;
    UndoPatch* end = q->first_undo;

    while (n > max_undos + 1 && end)
    {
        end = end->next;
        --n;
    }

    // empty range
    if (end == q->first_undo) return;

    if (DEBUG_LOG)
    {
        printf("trimming undo: %d\n", q->undo_count - n);
    }

    // max_undos > 0
    assert(end);

    UndoPatch* last_full = find_last_full_(q->first_undo, end);

    // first is always full
    assert(last_full);

    Bitmap* new_canvas = undo_replay_(last_full, end);
    UndoPatch* new_patch = undo_patch_create(new_canvas, bitmap_rect(new_canvas));
    assert(new_patch->full_image);
    new_patch->next = end;

    destroy_(q->first_undo, end);

    q->first_undo = new_patch;
    q->undo_count = n + 1;
}

void undo_queue_push(UndoQueue* q, UndoPatch* patch)
{
    // place on end of stack 
    if (q->last_undo)
    {
        destroy_(q->last_undo->next, NULL);
        q->last_undo->next = patch;
        q->last_undo = patch;
    }
    else
    {
        q->last_undo = patch;
        q->first_undo = patch;
        assert(q->first_undo->full_image);
    }
    ++q->undo_count;

    if (DEBUG_LOG)
    {
        printf("undo save %d: %d, %d, %d, %d\n", q->undo_count, patch->rect.x, patch->rect.y, patch->rect.w, patch->rect.h);
    }
}

int undo_queue_can_undo(UndoQueue* q)
{
    return q->first_undo && (q->first_undo != q->last_undo);
}

int undo_queue_can_redo(UndoQueue* q)
{
    return q->last_undo && q->last_undo->next;
}

void undo_queue_undo(UndoQueue* q, Layer* target)
{
    if (!undo_queue_can_undo(q))
    {
        return;
    }
    UndoPatch* last_full = find_last_full_(q->first_undo, q->last_undo);
    assert(last_full);
    assert(last_full->next);

    Bitmap* new_canvas = undo_replay_(last_full, q->last_undo);
    layer_set_bitmap(target, new_canvas);

    // one more thing.. we need to move "last_undo" back one
    UndoPatch* it = last_full;
    assert(it != q->last_undo);
    while (it->next != q->last_undo) it = it->next;
    q->last_undo = it;

    --q->undo_count;
    if (DEBUG_LOG) printf("undo_count %d\n", q->undo_count);
}

void undo_queue_redo(UndoQueue* q, Layer* target)
{
    if (!undo_queue_can_redo(q))
    {
        return;
    }

    UndoPatch* it = q->last_undo->next;
    Bitmap* to_blit = bitmap_decompress(it->data, it->data_size);
    if (it->full_image)
    {
        layer_set_bitmap(target, to_blit);
    }
    else
    {
        bitmap_blit_unsafe(
                to_blit,
                target->bitmaps,
                0, 0,
                it->rect.x, it->rect.y,
                it->rect.w, it->rect.h,
                COLOR_BLEND_REPLACE
                );
        bitmap_destroy(to_blit);
    }

    q->last_undo = it;

    ++q->undo_count;
}


