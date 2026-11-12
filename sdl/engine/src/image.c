#include "engine/image.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stb_image.h>
#include <string.h>

#include "engine/files.h"
#include "engine/log.h"

bool eng_image_decode(const void *data, size_t size, EngImage *out)
{
    *out = (EngImage){0};
    if (data == NULL || size == 0 || size > (size_t)INT32_MAX) {
        ENG_LOG_ERROR("image : données vides ou trop grandes");
        return false;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    uint8_t *pixels =
        stbi_load_from_memory((const stbi_uc *)data, (int)size, &width, &height, &channels, 4);
    if (pixels == NULL) {
        ENG_LOG_ERROR("image : décodage impossible : %s", stbi_failure_reason());
        return false;
    }
    if (width <= 0 || height <= 0 || (uint32_t)width > ENG_IMAGE_MAX_SIZE ||
        (uint32_t)height > ENG_IMAGE_MAX_SIZE) {
        ENG_LOG_ERROR("image : dimensions hors limites (%d x %d, maximum %u)", width, height,
                      ENG_IMAGE_MAX_SIZE);
        stbi_image_free(pixels);
        return false;
    }
    out->width = (uint32_t)width;
    out->height = (uint32_t)height;
    out->pixels = pixels;
    return true;
}

bool eng_image_load(EngArena *scratch, const char *path, EngImage *out)
{
    *out = (EngImage){0};
    char resolved[512];
    if (!eng_path_resolve(resolved, sizeof resolved, path)) {
        ENG_LOG_ERROR("image : chemin « %s » trop long", path);
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(scratch);
    EngFileData file;
    bool ok = false;
    if (eng_file_read_all(scratch, resolved, &file)) {
        ok = eng_image_decode(file.data, file.size, out);
    } else {
        ENG_LOG_ERROR("image : lecture impossible : %s", resolved);
    }
    eng_arena_reset_to(scratch, mark);
    return ok;
}

void eng_image_free(EngImage *image)
{
    if (image->pixels != NULL) {
        stbi_image_free(image->pixels);
    }
    *image = (EngImage){0};
}

/* ── Mipmaps ── */

uint32_t eng_image_mip_count(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        return 0;
    }
    uint32_t largest = width > height ? width : height;
    uint32_t levels = 1;
    while (largest > 1) {
        largest >>= 1;
        levels++;
    }
    return levels;
}

void eng_image_mip_size(uint32_t width, uint32_t height, uint32_t level, uint32_t *out_width,
                        uint32_t *out_height)
{
    uint32_t w = level >= 32 ? 1 : width >> level;
    uint32_t h = level >= 32 ? 1 : height >> level;
    *out_width = w != 0 ? w : 1;
    *out_height = h != 0 ? h : 1;
}

size_t eng_image_mip_chain_size(uint32_t width, uint32_t height, uint32_t levels)
{
    size_t total = 0;
    for (uint32_t level = 0; level < levels; level++) {
        uint32_t w;
        uint32_t h;
        eng_image_mip_size(width, height, level, &w, &h);
        total += (size_t)w * h * 4U;
    }
    return total;
}

/* sRGB 8 bits -> lumière linéaire [0, 1] (formule officielle). */
static float srgb_to_linear(uint32_t value)
{
    const float c = (float)value / 255.0F;
    return c <= 0.04045F ? c / 12.92F : powf((c + 0.055F) / 1.055F, 2.4F);
}

/* Lumière linéaire [0, 1] -> sRGB 8 bits, arrondi au plus proche. */
static uint8_t linear_to_srgb(float linear)
{
    const float c =
        linear <= 0.0031308F ? linear * 12.92F : (1.055F * powf(linear, 1.0F / 2.4F)) - 0.055F;
    const float scaled = (c * 255.0F) + 0.5F;
    return (uint8_t)(scaled < 0.0F ? 0.0F : (scaled > 255.0F ? 255.0F : scaled));
}

/* Moyenne la zone [x0, x1) x [y0, y1) de `src` (largeur sw) en un pixel RGBA. */
static void average_box(const uint8_t *src, uint32_t sw, uint32_t x0, uint32_t x1, uint32_t y0,
                        uint32_t y1, const float *lut, bool srgb, uint8_t out[4])
{
    float sum[4] = {0.0F, 0.0F, 0.0F, 0.0F};
    for (uint32_t y = y0; y < y1; y++) {
        for (uint32_t x = x0; x < x1; x++) {
            const uint8_t *p = src + (((size_t)y * sw) + x) * 4U;
            for (int c = 0; c < 3; c++) {
                sum[c] += srgb ? lut[p[c]] : (float)p[c];
            }
            sum[3] += (float)p[3];
        }
    }
    const float count = (float)((x1 - x0) * (y1 - y0));
    for (int c = 0; c < 3; c++) {
        const float mean = sum[c] / count;
        out[c] = srgb ? linear_to_srgb(mean) : (uint8_t)lroundf(mean);
    }
    out[3] = (uint8_t)lroundf(sum[3] / count);
}

bool eng_image_generate_mips(const uint8_t *base, uint32_t width, uint32_t height, uint32_t levels,
                             bool srgb, uint8_t *out)
{
    if (levels == 0 || levels > eng_image_mip_count(width, height) || base == NULL) {
        return false;
    }
    if (levels == 1) {
        return true;
    }
    if (out == NULL) {
        return false;
    }
    float lut[256];
    for (uint32_t i = 0; i < 256; i++) {
        lut[i] = srgb_to_linear(i);
    }
    const uint8_t *src = base;
    uint32_t sw = width;
    uint32_t sh = height;
    uint8_t *dst = out;
    for (uint32_t level = 1; level < levels; level++) {
        uint32_t dw;
        uint32_t dh;
        eng_image_mip_size(width, height, level, &dw, &dh);
        for (uint32_t y = 0; y < dh; y++) {
            const uint32_t y0 = (uint32_t)(((uint64_t)y * sh) / dh);
            uint32_t y1 = (uint32_t)(((uint64_t)(y + 1) * sh) / dh);
            y1 = y1 > y0 ? y1 : y0 + 1;
            for (uint32_t x = 0; x < dw; x++) {
                const uint32_t x0 = (uint32_t)(((uint64_t)x * sw) / dw);
                uint32_t x1 = (uint32_t)(((uint64_t)(x + 1) * sw) / dw);
                x1 = x1 > x0 ? x1 : x0 + 1;
                average_box(src, sw, x0, x1, y0, y1, lut, srgb, dst + (((size_t)y * dw) + x) * 4U);
            }
        }
        src = dst;
        sw = dw;
        sh = dh;
        dst += (size_t)dw * dh * 4U;
    }
    return true;
}
