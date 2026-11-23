/* Carte du monde : herbe de la plaine (2026-11-22). Placement fixe, comme les arbres : une grille
 * du monde, chaque nœud décalé et dimensionné selon une empreinte de ses coordonnées. Les mêmes
 * touffes sont donc rendues quel que soit le centre demandé : l'herbe ne change pas quand le
 * joueur se déplace. */
#include <math.h>

#include "engine/world_map.h"

#define PATCH_M 14.0 /* taille des taches (herbe haute, herbe rase) */

static uint64_t mix64(uint64_t h)
{
    h ^= h >> 33U;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33U;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33U;
    return h;
}

static float unit_of(uint64_t h)
{
    return (float)(h >> 40U) / 16777216.0F; /* [0, 1[ */
}

static uint64_t node_key(int64_t gx, int64_t gz, uint32_t salt)
{
    return mix64(((uint64_t)gx * 0x9E3779B97F4A7C15ULL) ^ ((uint64_t)gz * 0xC2B2AE3D27D4EB4FULL) ^
                 ((uint64_t)salt * 0x165667B19E3779F9ULL) ^ 0x6A09E667F3BCC909ULL);
}

/* bruit de valeur lissé dans [0, 1], de maille PATCH_M : les taches d'herbe */
static float patch(double x, double z)
{
    const double u = x / PATCH_M;
    const double v = z / PATCH_M;
    const double fu = floor(u);
    const double fv = floor(v);
    const float tx = (float)(u - fu);
    const float tz = (float)(v - fv);
    const float sx = tx * tx * (3.0F - (2.0F * tx));
    const float sz = tz * tz * (3.0F - (2.0F * tz));
    const int64_t ix = (int64_t)fu;
    const int64_t iz = (int64_t)fv;
    const float a = unit_of(node_key(ix, iz, 0xFACEU));
    const float b = unit_of(node_key(ix + 1, iz, 0xFACEU));
    const float c = unit_of(node_key(ix, iz + 1, 0xFACEU));
    const float d = unit_of(node_key(ix + 1, iz + 1, 0xFACEU));
    const float top = a + ((b - a) * sx);
    const float bottom = c + ((d - c) * sx);
    return top + ((bottom - top) * sz);
}

float eng_worldmap_grass_cover(const EngWorldMap *map, double x, double z)
{
    if (map == NULL || map->heights == NULL || !isfinite(x) || !isfinite(z) ||
        fabs(x) > 0.5 * (double)map->side || fabs(z) > 0.5 * (double)map->side) {
        return 0.0F;
    }
    float w[ENG_BIOME_COUNT];
    float path = 0.0F;
    eng_worldmap_weights(map, x, z, w, &path);
    if (w[ENG_BIOME_PLAIN] < 0.5F || path > 0.08F || eng_worldmap_lava(map, x, z) > 0.0F) {
        return 0.0F;
    }
    const float h = eng_worldmap_height(map, x, z);
    if (h < map->water + 0.25F) {
        return 0.0F;
    }
    const float dx = eng_worldmap_height(map, x + 2.0, z) - eng_worldmap_height(map, x - 2.0, z);
    const float dz = eng_worldmap_height(map, x, z + 2.0) - eng_worldmap_height(map, x, z - 2.0);
    const float slope = hypotf(dx, dz) * 0.25F;
    if (slope > ENG_GRASS_MAX_SLOPE) {
        return 0.0F;
    }
    /* de moins en moins d'herbe vers les autres biomes et sur les pentes */
    float cover = (w[ENG_BIOME_PLAIN] - 0.5F) / 0.3F;
    cover = cover > 1.0F ? 1.0F : cover;
    const float steep = (ENG_GRASS_MAX_SLOPE - slope) / 0.15F;
    return steep < 1.0F ? cover * steep : cover;
}

uint32_t eng_worldmap_grass(const EngWorldMap *map, double cx, double cz, float radius,
                            float spacing, float height, uint32_t salt, EngMapTuft *out,
                            uint32_t max)
{
    if (map == NULL || map->heights == NULL || out == NULL || !isfinite(cx) || !isfinite(cz) ||
        !(radius > 0.0F) || !(spacing >= 0.2F) || !(height > 0.0F) || radius > 400.0F) {
        return 0U;
    }
    const double s = (double)spacing;
    uint32_t count = 0;
    for (int64_t gz = (int64_t)floor((cz - radius) / s); (double)gz * s <= cz + radius; gz++) {
        for (int64_t gx = (int64_t)floor((cx - radius) / s); (double)gx * s <= cx + radius; gx++) {
            const uint64_t h = node_key(gx, gz, salt);
            /* décalé dans sa maille : la grille ne se voit pas */
            const double x = ((double)gx + (double)unit_of(h)) * s;
            const double z = ((double)gz + (double)unit_of(mix64(h))) * s;
            if (hypot(x - cx, z - cz) > (double)radius) {
                continue;
            }
            const float cover = eng_worldmap_grass_cover(map, x, z);
            /* là où l'herbe se raréfie, de moins en moins de touffes (pas des touffes naines) */
            if (cover <= 0.0F || unit_of(mix64(h ^ 0xC0FEULL)) > cover) {
                continue;
            }
            if (count >= max) {
                return count;
            }
            const float tall = patch(x, z); /* taches d'herbe haute ou rase */
            const float vary = unit_of(mix64(h ^ 0x5A5AULL));
            const float shade = 0.8F + (0.2F * unit_of(mix64(h ^ 0x7E7EULL)));
            EngMapTuft *t = &out[count++];
            t->x = (float)x;
            t->z = (float)z;
            t->y = eng_worldmap_height(map, x, z);
            t->yaw = 6.2831853F * unit_of(mix64(h ^ 0x3C3CULL));
            t->scale = height * (0.55F + (0.55F * tall) + (0.3F * vary));
            /* herbe haute : un peu plus jaune ; chaque touffe un peu plus ou moins sombre */
            t->tint[0] = (uint8_t)lroundf(255.0F * shade * (0.78F + (0.22F * tall)));
            t->tint[1] = (uint8_t)lroundf(255.0F * shade);
            t->tint[2] = (uint8_t)lroundf(255.0F * shade * (0.9F - (0.15F * tall)));
        }
    }
    return count;
}
