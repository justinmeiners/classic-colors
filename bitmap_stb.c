#include "bitmap.h"

// definitions for whole program
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

typedef struct
{
    size_t size;
    size_t capacity;
    unsigned char* data;
} CompressContext;

static
void write_(void* context, void* chunk_data, int chunk_size)
{
    CompressContext* ctx = context;
    size_t required = ctx->size + chunk_size;
    if (required > ctx->capacity)
    {
        ctx->capacity = MAX(required * 2, 256);
        ctx->data = realloc(ctx->data, ctx->capacity);
    }
    memcpy(ctx->data + ctx->size, chunk_data, chunk_size);
    ctx->size += chunk_size;
}

CcBitmap cc_bitmap_decompress(unsigned char* compressed_data, size_t compressed_size)
{
    int w, h, comps;
    unsigned char* data = stbi_load_from_memory(compressed_data, compressed_size, &w, &h, &comps, 0);
    assert(comps == 4);

    CcBitmap b = {
        .w = w,
        .h = h,
        .data = (CcPixel *)data
    };
    return b;
}

unsigned char* cc_bitmap_compress(const CcBitmap* b, size_t* out_size)
{
    assert(b);

    CompressContext ctx = { 0 };

    size_t stride = b->w * sizeof(CcPixel);
    stbi_write_png_compression_level = 5;
    if (stbi_write_png_to_func(write_, &ctx, b->w, b->h, 4, b->data, stride))
    {
        if (ctx.capacity > ctx.size)
        {
            ctx.data = realloc(ctx.data, ctx.size);
        }

        if (DEBUG_LOG)
        {
            printf("compress. before: %d. after: %lu\n", b->w * b->h * 4, ctx.size);
        }

        *out_size = ctx.size;
        return ctx.data;
    }

    return NULL;
}
