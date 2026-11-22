#include "engine/splatmap.h"

#include <math.h>
#include <string.h>

#include "engine/log.h"
#include "engine/noise.h"

#define RAD_TO_DEG 57.29577951308232F

EngSplatParams eng_splat_default_params(void)
{
    return (EngSplatParams){.rock_slope_start_deg = 30.0F,
                            .rock_slope_full_deg = 50.0F,
                            .snow_start = 0.70F,
                            .snow_full = 0.85F,
                            .dirt_amount = 0.30F,
                            .dirt_wavelength = 24.0F,
                            .seed = 7};
}

bool eng_splat_params_valid(const EngSplatParams *p)
{
    const char *why = NULL;
    if (p == NULL) {
        why = "paramètres absents";
    } else if (!isfinite(p->rock_slope_start_deg) || !isfinite(p->rock_slope_full_deg) ||
               !isfinite(p->snow_start) || !isfinite(p->snow_full) || !isfinite(p->dirt_amount) ||
               !isfinite(p->dirt_wavelength)) {
        why = "valeur non finie";
    } else if (!(p->rock_slope_start_deg >= 0.0F &&
                 p->rock_slope_start_deg < p->rock_slope_full_deg &&
                 p->rock_slope_full_deg <= 90.0F)) {
        why = "pentes de la roche : 0 <= début < plein <= 90";
    } else if (!(p->snow_start >= 0.0F && p->snow_start <= p->snow_full && p->snow_full <= 1.0F)) {
        why = "altitudes de la neige : 0 <= début <= plein <= 1";
    } else if (!(p->dirt_amount >= 0.0F && p->dirt_amount <= 1.0F)) {
        why = "part de terre hors de [0, 1]";
    } else if (!(p->dirt_wavelength > 0.0F)) {
        why = "longueur d'onde de la terre <= 0";
    }
    if (why != NULL) {
        ENG_LOG_ERROR("splatmap : paramètres invalides : %s", why);
        return false;
    }
    return true;
}

static float clamp01(float v)
{
    return v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v);
}

/* smoothstep(a, b, x) ; a == b : marche en a (1 si x >= a). */
static float smooth(float a, float b, float x)
{
    if (!(b > a)) {
        return x >= a ? 1.0F : 0.0F;
    }
    const float t = clamp01((x - a) / (b - a));
    return t * t * (3.0F - (2.0F * t));
}

void eng_splat_weights(const EngSplatParams *p, float slope_deg, float altitude_relative,
                       float dirt_noise, float out[ENG_SPLAT_LAYERS])
{
    const float rock = smooth(p->rock_slope_start_deg, p->rock_slope_full_deg, slope_deg);
    const float snow =
        smooth(p->snow_start, p->snow_full, clamp01(altitude_relative)) * (1.0F - rock);
    float dirt = 0.0F;
    if (p->dirt_amount > 0.0F) {
        const float threshold = 1.0F - p->dirt_amount;
        dirt = smooth(threshold - 0.1F, threshold + 0.1F, clamp01(dirt_noise));
    }
    dirt *= 1.0F - rock - snow;
    out[ENG_SPLAT_GRASS] = 1.0F - rock - snow - dirt;
    out[ENG_SPLAT_ROCK] = rock;
    out[ENG_SPLAT_DIRT] = dirt;
    out[ENG_SPLAT_SNOW] = snow;
}

void eng_splat_quantize(const float weights[ENG_SPLAT_LAYERS], uint8_t out[ENG_SPLAT_LAYERS])
{
    int total = 0;
    uint32_t largest = 0;
    for (uint32_t i = 0; i < ENG_SPLAT_LAYERS; i++) {
        out[i] = (uint8_t)(clamp01(weights[i]) * 255.0F + 0.5F);
        total += out[i];
        if (weights[i] > weights[largest]) {
            largest = i;
        }
    }
    /* le reste de l'arrondi (au plus deux unités) va au plus grand poids, qui vaut au moins 1/4 */
    out[largest] = (uint8_t)((int)out[largest] + (255 - total));
}

float eng_heightmap_slope_degrees(const EngHeightmap *map, uint32_t x, uint32_t z)
{
    const uint32_t last = map->size - 1U;
    const uint32_t x0 = x > 0U ? x - 1U : 0U;
    const uint32_t x1 = x < last ? x + 1U : last;
    const uint32_t z0 = z > 0U ? z - 1U : 0U;
    const uint32_t z1 = z < last ? z + 1U : last;
    const float dx = (eng_heightmap_altitude(map, x1, z) - eng_heightmap_altitude(map, x0, z)) /
                     ((float)(x1 - x0) * map->spacing);
    const float dz = (eng_heightmap_altitude(map, x, z1) - eng_heightmap_altitude(map, x, z0)) /
                     ((float)(z1 - z0) * map->spacing);
    return atanf(sqrtf((dx * dx) + (dz * dz))) * RAD_TO_DEG;
}

bool eng_splatmap_generate(EngArena *arena, const EngHeightmap *map, const EngSplatParams *params,
                           EngSplatmap *out)
{
    *out = (EngSplatmap){0};
    if (map == NULL || map->samples == NULL || map->size < ENG_HEIGHTMAP_MIN_SIZE ||
        !(map->altitude_max > map->altitude_min) || !(map->spacing > 0.0F)) {
        ENG_LOG_ERROR("splatmap : champ de hauteurs invalide");
        return false;
    }
    if (!eng_splat_params_valid(params)) {
        return false;
    }
    const uint32_t size = map->size;
    const EngArenaMark start = eng_arena_mark(arena);
    uint8_t *pixels = eng_arena_alloc_array(arena, 4U, (size_t)size * size, 4U);
    const EngArenaMark after_pixels = eng_arena_mark(arena);
    float *noise_row = pixels != NULL ? ENG_ARENA_NEW_ARRAY(arena, float, size) : NULL;
    if (noise_row == NULL) {
        eng_arena_reset_to(arena, start);
        ENG_LOG_ERROR("splatmap : arène trop petite pour %u x %u texels", size, size);
        return false;
    }
    const EngNoiseParams noise = {.seed = params->seed,
                                  .frequency = 1.0F / params->dirt_wavelength,
                                  .octaves = 3,
                                  .lacunarity = 2.0F,
                                  .gain = 0.5F,
                                  .fractal = ENG_NOISE_FBM};
    const float range = map->altitude_max - map->altitude_min;
    for (uint32_t z = 0; z < size; z++) {
        eng_noise_row_2d(&noise, 0.0F, map->spacing, (float)z * map->spacing, size, noise_row);
        for (uint32_t x = 0; x < size; x++) {
            float weights[ENG_SPLAT_LAYERS];
            const float altitude = (eng_heightmap_altitude(map, x, z) - map->altitude_min) / range;
            eng_splat_weights(params, eng_heightmap_slope_degrees(map, x, z), altitude,
                              (noise_row[x] * 0.5F) + 0.5F, weights);
            eng_splat_quantize(weights, &pixels[((size_t)z * size + x) * 4U]);
        }
    }
    eng_arena_reset_to(arena, after_pixels); /* la ligne de travail est restituée */
    out->size = size;
    out->pixels = pixels;
    return true;
}
