#include "engine/placement.h"

#include <math.h>
#include <string.h>

#include "engine/log.h"

#define PI_F 3.14159265358979323846F
#define RAD_TO_DEG (180.0F / PI_F)

EngPlacementRule eng_placement_vegetation_rule(void)
{
    return (EngPlacementRule){.seed = 1,
                              .cell_meters = 2.0F,
                              .slope_max_deg = 28.0F,
                              .slope_falloff_deg = 8.0F,
                              .altitude_min = 0.0F,
                              .altitude_max = 1.0F,
                              .density_layer = (int32_t)ENG_SPLAT_GRASS,
                              .layer_weight_min = 0.2F,
                              .rock_weight_max = 0.15F,
                              .scale_min = 0.6F,
                              .scale_max = 1.2F,
                              .sink = 0.05F,
                              .tint_min = 190,
                              .margin_meters = 2.0F};
}

EngPlacementRule eng_placement_rock_rule(void)
{
    return (EngPlacementRule){.seed = 2,
                              .cell_meters = 6.0F,
                              .slope_max_deg = 90.0F,
                              .slope_falloff_deg = 0.0F,
                              .altitude_min = 0.0F,
                              .altitude_max = 1.0F,
                              .density_layer = (int32_t)ENG_SPLAT_ROCK,
                              .layer_weight_min = 0.35F,
                              .rock_weight_max = 1.0F,
                              .scale_min = 0.5F,
                              .scale_max = 2.0F,
                              .sink = 0.15F,
                              .tint_min = 200,
                              .margin_meters = 2.0F};
}

EngPlacementRule eng_placement_grass_rule(void)
{
    return (EngPlacementRule){.seed = 3,
                              .cell_meters = 1.0F,
                              .slope_max_deg = 30.0F,
                              .slope_falloff_deg = 10.0F,
                              .altitude_min = 0.0F,
                              .altitude_max = 1.0F,
                              .density_layer = (int32_t)ENG_SPLAT_GRASS,
                              .layer_weight_min = 0.3F,
                              .rock_weight_max = 0.1F,
                              .scale_min = 0.7F,
                              .scale_max = 1.4F,
                              .sink = 0.0F,
                              .tint_min = 150,
                              .margin_meters = 2.0F};
}

bool eng_placement_rule_valid(const EngPlacementRule *r)
{
    return r != NULL && isfinite(r->cell_meters) && r->cell_meters > 0.0F &&
           isfinite(r->slope_max_deg) && r->slope_max_deg > 0.0F && r->slope_max_deg <= 90.0F &&
           isfinite(r->slope_falloff_deg) && r->slope_falloff_deg >= 0.0F &&
           r->slope_falloff_deg <= r->slope_max_deg && isfinite(r->altitude_min) &&
           isfinite(r->altitude_max) && r->altitude_min >= 0.0F &&
           r->altitude_max >= r->altitude_min && r->altitude_max <= 1.0F &&
           r->density_layer >= -1 && r->density_layer < (int32_t)ENG_SPLAT_LAYERS &&
           isfinite(r->layer_weight_min) && r->layer_weight_min >= 0.0F &&
           r->layer_weight_min <= 1.0F && isfinite(r->rock_weight_max) &&
           r->rock_weight_max >= 0.0F && r->rock_weight_max <= 1.0F && isfinite(r->scale_min) &&
           r->scale_min > 0.0F && isfinite(r->scale_max) && r->scale_max >= r->scale_min &&
           isfinite(r->sink) && r->sink >= 0.0F && r->sink < 1.0F && isfinite(r->margin_meters) &&
           r->margin_meters >= 0.0F;
}

static float smoothstep01(float t)
{
    const float c = t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
    return c * c * (3.0F - (2.0F * c));
}

float eng_splatmap_weight_at(const EngHeightmap *map, const EngSplatmap *splat, uint32_t layer,
                             float x_m, float z_m)
{
    if (splat == NULL || splat->pixels == NULL || splat->size != map->size ||
        layer >= ENG_SPLAT_LAYERS) {
        return 0.0F;
    }
    const float last = (float)(map->size - 1U);
    float u = x_m / map->spacing;
    float v = z_m / map->spacing;
    u = u < 0.0F ? 0.0F : (u > last ? last : u);
    v = v < 0.0F ? 0.0F : (v > last ? last : v);
    uint32_t x0 = (uint32_t)u;
    uint32_t z0 = (uint32_t)v;
    if (x0 > map->size - 2U) {
        x0 = map->size - 2U;
    }
    if (z0 > map->size - 2U) {
        z0 = map->size - 2U;
    }
    const float fx = u - (float)x0;
    const float fz = v - (float)z0;
    const size_t stride = map->size;
    const float w00 = (float)splat->pixels[((((size_t)z0 * stride) + x0) * 4U) + layer];
    const float w10 = (float)splat->pixels[((((size_t)z0 * stride) + x0 + 1U) * 4U) + layer];
    const float w01 = (float)splat->pixels[((((size_t)(z0 + 1U) * stride) + x0) * 4U) + layer];
    const float w11 = (float)splat->pixels[((((size_t)(z0 + 1U) * stride) + x0 + 1U) * 4U) + layer];
    const float top = w00 + ((w10 - w00) * fx);
    const float bottom = w01 + ((w11 - w01) * fx);
    return (top + ((bottom - top) * fz)) * (1.0F / 255.0F);
}

/* pente (degrés) en un point quelconque : différences centrées sur un échantillon de part et
 * d'autre */
static float slope_at(const EngHeightmap *map, float x_m, float z_m)
{
    const float d = map->spacing;
    const float dx =
        (eng_heightmap_sample(map, x_m + d, z_m) - eng_heightmap_sample(map, x_m - d, z_m)) /
        (2.0F * d);
    const float dz =
        (eng_heightmap_sample(map, x_m, z_m + d) - eng_heightmap_sample(map, x_m, z_m - d)) /
        (2.0F * d);
    return atanf(sqrtf((dx * dx) + (dz * dz))) * RAD_TO_DEG;
}

float eng_placement_suitability(const EngHeightmap *map, const EngSplatmap *splat,
                                const EngPlacementRule *rule, float x_m, float z_m)
{
    const float extent = eng_heightmap_extent(map);
    if (!(x_m >= rule->margin_meters) || !(z_m >= rule->margin_meters) ||
        !(x_m <= extent - rule->margin_meters) || !(z_m <= extent - rule->margin_meters)) {
        return 0.0F;
    }
    const float altitude = (eng_heightmap_sample(map, x_m, z_m) - map->altitude_min) /
                           (map->altitude_max - map->altitude_min);
    if (altitude < rule->altitude_min || altitude > rule->altitude_max) {
        return 0.0F;
    }
    const float slope = slope_at(map, x_m, z_m);
    if (slope >= rule->slope_max_deg) {
        return 0.0F;
    }
    float suitability = 1.0F;
    const float start = rule->slope_max_deg - rule->slope_falloff_deg;
    if (slope > start) {
        suitability = 1.0F - smoothstep01((slope - start) / rule->slope_falloff_deg);
    }
    if (rule->rock_weight_max < 1.0F &&
        eng_splatmap_weight_at(map, splat, ENG_SPLAT_ROCK, x_m, z_m) > rule->rock_weight_max) {
        return 0.0F;
    }
    if (rule->density_layer >= 0) {
        const float w = eng_splatmap_weight_at(map, splat, (uint32_t)rule->density_layer, x_m, z_m);
        if (w < rule->layer_weight_min || w <= 0.0F) {
            return 0.0F;
        }
        suitability *= w;
    }
    return suitability;
}

/* ── hachage sans état ── */

static uint32_t mix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x7FEB352DU;
    h ^= h >> 15;
    h *= 0x846CA68BU;
    h ^= h >> 16;
    return h;
}

/* flottant dans [0, 1[ pour (graine, cellule, canal) */
static float cell_random(uint32_t seed, int32_t i, int32_t j, uint32_t channel)
{
    uint32_t h = mix32(seed ^ (channel * 0x9E3779B9U));
    h = mix32(h ^ ((uint32_t)i * 0x85EBCA6BU));
    h = mix32(h ^ ((uint32_t)j * 0xC2B2AE35U));
    return (float)(h >> 8) * (1.0F / 16777216.0F);
}

/* Parcourt les cellules ; pour chacune accepte (ou non) le candidat. `out` NULL : compte seulement.
 */
static uint32_t scan(const EngHeightmap *map, const EngSplatmap *splat,
                     const EngPlacementRule *rule, float origin_x, float origin_z, EngInstance *out)
{
    const float extent = eng_heightmap_extent(map);
    const uint32_t cells = (uint32_t)ceilf(extent / rule->cell_meters);
    uint32_t count = 0;
    for (uint32_t j = 0; j < cells; j++) {
        for (uint32_t i = 0; i < cells; i++) {
            const float x =
                ((float)i + cell_random(rule->seed, (int32_t)i, (int32_t)j, 0)) * rule->cell_meters;
            const float z =
                ((float)j + cell_random(rule->seed, (int32_t)i, (int32_t)j, 1)) * rule->cell_meters;
            const float suitability = eng_placement_suitability(map, splat, rule, x, z);
            if (suitability <= 0.0F ||
                !(cell_random(rule->seed, (int32_t)i, (int32_t)j, 2) < suitability)) {
                continue;
            }
            if (out != NULL) {
                const float scale =
                    rule->scale_min + ((rule->scale_max - rule->scale_min) *
                                       cell_random(rule->seed, (int32_t)i, (int32_t)j, 3));
                const float yaw = 2.0F * PI_F * cell_random(rule->seed, (int32_t)i, (int32_t)j, 4);
                const float shade_f =
                    (float)rule->tint_min + ((255.0F - (float)rule->tint_min) *
                                             cell_random(rule->seed, (int32_t)i, (int32_t)j, 5));
                const uint8_t shade = (uint8_t)shade_f;
                const uint8_t tint[4] = {shade, shade, shade, 255};
                const float position[3] = {origin_x + x,
                                           eng_heightmap_sample(map, x, z) - (rule->sink * scale),
                                           origin_z + z};
                eng_instance_make_yaw(&out[count], position, scale, yaw, tint);
            }
            count++;
        }
    }
    return count;
}

static bool inputs_valid(const EngHeightmap *map, const EngSplatmap *splat,
                         const EngPlacementRule *rule)
{
    if (map == NULL || map->samples == NULL || map->size < ENG_HEIGHTMAP_MIN_SIZE ||
        !(map->spacing > 0.0F) || !(map->altitude_max > map->altitude_min)) {
        ENG_LOG_ERROR("placement : champ de hauteurs invalide");
        return false;
    }
    if (!eng_placement_rule_valid(rule)) {
        ENG_LOG_ERROR("placement : règle invalide");
        return false;
    }
    const bool needs_splat = rule->density_layer >= 0 || rule->rock_weight_max < 1.0F;
    if (needs_splat && (splat == NULL || splat->pixels == NULL || splat->size != map->size)) {
        ENG_LOG_ERROR("placement : cette règle demande une splatmap de la taille du champ");
        return false;
    }
    return true;
}

uint32_t eng_placement_count(const EngHeightmap *map, const EngSplatmap *splat,
                             const EngPlacementRule *rule)
{
    if (!inputs_valid(map, splat, rule)) {
        return 0;
    }
    return scan(map, splat, rule, 0.0F, 0.0F, NULL);
}

bool eng_placement_generate(EngArena *arena, const EngHeightmap *map, const EngSplatmap *splat,
                            const EngPlacementRule *rule, float origin_x, float origin_z,
                            EngInstance **instances, uint32_t *count)
{
    *instances = NULL;
    *count = 0;
    if (!inputs_valid(map, splat, rule) || !isfinite(origin_x) || !isfinite(origin_z)) {
        return false;
    }
    const uint32_t total = scan(map, splat, rule, origin_x, origin_z, NULL);
    if (total == 0) {
        return true;
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    EngInstance *out = ENG_ARENA_NEW_ARRAY(arena, EngInstance, total);
    if (out == NULL) {
        eng_arena_reset_to(arena, mark);
        ENG_LOG_ERROR("placement : arène trop petite pour %u instances", total);
        return false;
    }
    const uint32_t written = scan(map, splat, rule, origin_x, origin_z, out);
    *instances = out;
    *count = written;
    return true;
}
