#include "engine/biome.h"

#include <math.h>
#include <stddef.h>

static const float ALTITUDE_SCALE[ENG_BIOME_COUNT] = {0.45F, 1.8F, 0.35F, 1.2F};

EngBiomeMap eng_biome_map_default(void)
{
    return (EngBiomeMap){.enabled = true,
                         .start_x = 0.0F,
                         .band_m = 800.0F,
                         .blend_m = 160.0F,
                         .wobble_m = 70.0F,
                         .wobble_wavelength_m = 900.0F};
}

bool eng_biome_map_valid(const EngBiomeMap *map)
{
    return map != NULL && isfinite(map->start_x) && isfinite(map->band_m) &&
           isfinite(map->blend_m) && isfinite(map->wobble_m) &&
           isfinite(map->wobble_wavelength_m) && map->blend_m > 0.0F &&
           map->band_m > map->blend_m && map->wobble_m >= 0.0F && map->wobble_wavelength_m > 0.0F;
}

static float smooth(float edge0, float edge1, float x)
{
    const float t = (x - edge0) / (edge1 - edge0);
    const float c = t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
    return c * c * (3.0F - (2.0F * c));
}

void eng_biome_weights(const EngBiomeMap *map, double x, double z, float out[ENG_BIOME_COUNT])
{
    out[ENG_BIOME_PLAIN] = 1.0F;
    out[ENG_BIOME_SNOW] = 0.0F;
    out[ENG_BIOME_DESERT] = 0.0F;
    out[ENG_BIOME_VOLCANO] = 0.0F;
    if (map == NULL || !map->enabled || !eng_biome_map_valid(map)) {
        return;
    }
    /* la frontière ondule : on déplace le point le long de X selon z */
    const float u =
        (float)(x - (double)map->start_x) +
        (map->wobble_m * sinf((float)(z * 6.283185307179586 / (double)map->wobble_wavelength_m)));
    const float half = 0.5F * map->blend_m;
    float t[ENG_BIOME_COUNT]; /* t[k] : part des biomes d'indice >= k */
    t[0] = 1.0F;
    for (int k = 1; k < ENG_BIOME_COUNT; k++) {
        const float edge = (float)k * map->band_m;
        t[k] = smooth(edge - half, edge + half, u);
    }
    for (int k = 0; k < ENG_BIOME_COUNT; k++) {
        const float next = k + 1 < ENG_BIOME_COUNT ? t[k + 1] : 0.0F;
        out[k] = t[k] - next;
    }
}

EngBiome eng_biome_at(const EngBiomeMap *map, double x, double z)
{
    float w[ENG_BIOME_COUNT];
    eng_biome_weights(map, x, z, w);
    int best = 0;
    for (int k = 1; k < ENG_BIOME_COUNT; k++) {
        if (w[k] > w[best]) {
            best = k;
        }
    }
    return (EngBiome)best;
}

const char *eng_biome_name(EngBiome biome)
{
    switch (biome) {
    case ENG_BIOME_PLAIN:
        return "plaine";
    case ENG_BIOME_SNOW:
        return "montagne enneigée";
    case ENG_BIOME_DESERT:
        return "désert";
    case ENG_BIOME_VOLCANO:
        return "volcan";
    default:
        return "?";
    }
}

float eng_biome_altitude_scale(const float weights[ENG_BIOME_COUNT])
{
    if (weights == NULL) {
        return 1.0F;
    }
    float scale = 0.0F;
    for (int k = 0; k < ENG_BIOME_COUNT; k++) {
        scale += weights[k] * ALTITUDE_SCALE[k];
    }
    return scale;
}

void eng_biome_splat(const float weights[ENG_BIOME_COUNT], const float base[ENG_SPLAT_LAYERS],
                     float out[ENG_SPLAT_LAYERS])
{
    const float grass = base[ENG_SPLAT_GRASS];
    const float rock = base[ENG_SPLAT_ROCK];
    const float dirt = base[ENG_SPLAT_DIRT];
    const float snow = base[ENG_SPLAT_SNOW];
    const float total = grass + rock + dirt + snow;
    /* une règle par biome : [herbe, roche, terre, neige], chacune de somme `total` */
    const float rules[ENG_BIOME_COUNT][ENG_SPLAT_LAYERS] = {
        {grass + snow, rock, dirt, 0.0F},        /* plaine : jamais de neige */
        {0.0F, rock, 0.0F, grass + dirt + snow}, /* montagne : neige */
        {0.0F, rock, grass + dirt + snow, 0.0F}, /* désert : sable (terre) */
        {0.0F, rock + (0.8F * (total - rock)), 0.2F * (total - rock), 0.0F}, /* volcan */
    };
    for (uint32_t l = 0; l < ENG_SPLAT_LAYERS; l++) {
        out[l] = 0.0F;
        for (int k = 0; k < ENG_BIOME_COUNT; k++) {
            out[l] += weights[k] * rules[k][l];
        }
    }
}
