/* Terrain à six matières (carte du monde, 2026-11-21) : les couches de sable et de lave, recolorées
 * depuis les couches générées de terre et de roche (même grain) : sable = terre en ocre clair ;
 * lave = roche dont les parties sombres deviennent la croûte et les claires la lave vive. Et la
 * seconde carte de poids par défaut (1 x 1, nulle : ni sable ni lave). */
#include "engine/terrain_gpu.h"
#include "engine/terrain_texture.h"

static void recolor(const uint8_t *src, uint8_t *dst, size_t count, bool lava)
{
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += (0.3 * src[i * 4U]) + (0.59 * src[(i * 4U) + 1U]) + (0.11 * src[(i * 4U) + 2U]);
    }
    const float mean = (float)(sum / (double)count) + 1.0F;
    for (size_t i = 0; i < count; i++) {
        const float y = (0.3F * (float)src[i * 4U]) + (0.59F * (float)src[(i * 4U) + 1U]) +
                        (0.11F * (float)src[(i * 4U) + 2U]);
        float k = y / mean; /* grain : 1 en moyenne */
        float rgb[3];
        if (!lava) {
            k = k < 0.75F ? 0.75F : (k > 1.2F ? 1.2F : k);
            rgb[0] = 214.0F * k;
            rgb[1] = 178.0F * k;
            rgb[2] = 112.0F * k;
        } else { /* croûte sombre sous 0,9, lave vive au-dessus */
            const float t = k < 0.9F ? 0.0F : (k > 1.1F ? 1.0F : (k - 0.9F) * 5.0F);
            rgb[0] = 70.0F + (185.0F * t);
            rgb[1] = 18.0F + (122.0F * t);
            rgb[2] = 8.0F + (16.0F * t);
        }
        for (int c = 0; c < 3; c++) {
            dst[(i * 4U) + (size_t)c] = (uint8_t)(rgb[c] > 255.0F ? 255.0F : rgb[c]);
        }
        dst[(i * 4U) + 3U] = 255U;
    }
}

bool eng_terrain_gpu_create_extra(EngTerrainGpu *gpu, EngRenderer *renderer, EngArena *scratch,
                                  uint32_t layer_size)
{
    const size_t count = (size_t)layer_size * layer_size;
    const EngArenaMark mark = eng_arena_mark(scratch);
    uint8_t *src = eng_arena_alloc_array(scratch, 4U, count, 4U);
    uint8_t *dst = eng_arena_alloc_array(scratch, 4U, count, 4U);
    bool ok = src != NULL && dst != NULL;
    for (int pass = 0; ok && pass < 2; pass++) {
        eng_terrain_layer_pixels(pass == 0 ? ENG_SPLAT_DIRT : ENG_SPLAT_ROCK, layer_size, src);
        recolor(src, dst, count, pass == 1);
        const EngTextureDesc desc = {.name = pass == 0 ? "terrain-sable" : "terrain-lave",
                                     .width = layer_size,
                                     .height = layer_size,
                                     .format = ENG_TEXTURE_RGBA8_SRGB,
                                     .pixels = dst,
                                     .mip_levels = 0};
        gpu->extra[pass] = eng_renderer_create_texture(renderer, &desc);
        ok = eng_renderer_texture_is_valid(renderer, gpu->extra[pass]);
    }
    eng_arena_reset_to(scratch, mark);
    static const uint8_t none[4] = {0, 0, 0, 0};
    const EngTextureDesc zero = {.name = "terrain-poids2-nuls",
                                 .width = 1,
                                 .height = 1,
                                 .format = ENG_TEXTURE_RGBA8,
                                 .pixels = none,
                                 .mip_levels = 1};
    if (ok) {
        gpu->splat2 = eng_renderer_create_texture(renderer, &zero);
        ok = eng_renderer_texture_is_valid(renderer, gpu->splat2);
    }
    return ok;
}

void eng_terrain_gpu_tail(const EngTerrainGpu *gpu, EngTextureHandle splat2,
                          EngTextureHandle out[ENG_MAX_TAIL_TEXTURES])
{
    out[0] = splat2.handle.generation != 0 ? splat2 : gpu->splat2;
    out[1] = gpu->extra[0];
    out[2] = gpu->extra[1];
}
