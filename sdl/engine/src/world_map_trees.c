/* Carte du monde : arbres (forêts, arbres épars) et places des zones de combat. Placement fixe :
 * une grille du monde par forme, chaque nœud décalé et accepté selon une empreinte de ses
 * coordonnées (aucun aléa, même résultat quel que soit le découpage en cellules). */
#include <math.h>

#include "engine/world_map.h"

#define KEEP_OFF_SPAWN_M 70.0F /* le château de départ */
#define KEEP_OFF_ZONE_M 32.0F  /* l'aire d'une zone de combat */
#define MAX_SLOPE 0.7F         /* pas d'arbre sur une pente plus raide (35 degrés) */

/* hauteur typique (m) et variation par essence */
static const float TREE_HEIGHT[ENG_TREE_KINDS] = {9.0F, 11.0F, 6.5F};

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

static uint64_t node_key(int64_t gx, int64_t gz, uint32_t feature)
{
    return mix64(((uint64_t)gx * 0x9E3779B97F4A7C15ULL) ^ ((uint64_t)gz * 0xC2B2AE3D27D4EB4FULL) ^
                 ((uint64_t)feature * 0x165667B19E3779F9ULL));
}

/* un arbre peut-il pousser en (x, z) ? */
static bool ground_ok(const EngWorldMap *m, float x, float z, float *y)
{
    float w[ENG_BIOME_COUNT];
    float path = 0.0F;
    eng_worldmap_weights(m, x, z, w, &path);
    if (path > 0.05F || w[ENG_BIOME_VOLCANO] > 0.25F || eng_worldmap_lava(m, x, z) > 0.0F) {
        return false;
    }
    const float h = eng_worldmap_height(m, x, z);
    if (h < m->water + 0.8F) {
        return false;
    }
    const float dx = eng_worldmap_height(m, x + 2.0, z) - eng_worldmap_height(m, x - 2.0, z);
    const float dz = eng_worldmap_height(m, x, z + 2.0) - eng_worldmap_height(m, x, z - 2.0);
    if (hypotf(dx, dz) * 0.25F > MAX_SLOPE) {
        return false;
    }
    if (hypotf(x - m->spawn[0], z - m->spawn[1]) < KEEP_OFF_SPAWN_M) {
        return false;
    }
    for (uint32_t k = 0; k < ENG_MAPZONE_COUNT; k++) {
        if (m->zones[k].used && hypotf(x - m->zones[k].x, z - m->zones[k].z) < KEEP_OFF_ZONE_M) {
            return false;
        }
    }
    *y = h;
    return true;
}

/* nœuds de la grille de la forme `f` (indice `index`) dans le carré, ajoutés à `out` */
static uint32_t place(const EngWorldMap *m, const EngMapFeature *f, uint32_t index, double x0,
                      double z0, double size, EngMapTree *out, uint32_t count, uint32_t max)
{
    const double s = (double)f->a;
    double lo_x = x0;
    double lo_z = z0;
    double hi_x = x0 + size;
    double hi_z = z0 + size;
    if (f->kind == (uint8_t)ENG_MAPF_FOREST) { /* seulement la boîte du disque */
        lo_x = fmax(lo_x, (double)(f->x - f->radius));
        lo_z = fmax(lo_z, (double)(f->z - f->radius));
        hi_x = fmin(hi_x, (double)(f->x + f->radius));
        hi_z = fmin(hi_z, (double)(f->z + f->radius));
    }
    const double half_map = 0.5 * (double)m->side;
    for (int64_t gz = (int64_t)floor(lo_z / s); (double)gz * s < hi_z + s; gz++) {
        for (int64_t gx = (int64_t)floor(lo_x / s); (double)gx * s < hi_x + s; gx++) {
            if (count >= max) {
                return count;
            }
            const uint64_t h = node_key(gx, gz, index);
            /* décalage dans la maille : jusqu'à 40 % de l'espacement */
            const double x = ((double)gx + 0.5 + (0.8 * ((double)unit_of(h) - 0.5))) * s;
            const double z = ((double)gz + 0.5 + (0.8 * ((double)unit_of(mix64(h)) - 0.5))) * s;
            if (x < x0 || x >= x0 + size || z < z0 || z >= z0 + size || fabs(x) > half_map ||
                fabs(z) > half_map) {
                continue;
            }
            const float keep = unit_of(mix64(h ^ 0xA5A5ULL));
            if (f->kind == (uint8_t)ENG_MAPF_FOREST) {
                /* lisière : la densité baisse sur le dernier tiers du rayon */
                const float r = hypotf((float)x - f->x, (float)z - f->z) / f->radius;
                if (r >= 1.0F || keep > (r < 0.66F ? 0.92F : 0.92F * (1.0F - r) / 0.34F)) {
                    continue;
                }
            } else {
                float w[ENG_BIOME_COUNT];
                eng_worldmap_weights(m, x, z, w, NULL);
                if (w[f->biome] < 0.7F || keep > 0.75F) {
                    continue;
                }
            }
            float y = 0.0F;
            if (!ground_ok(m, (float)x, (float)z, &y)) {
                continue;
            }
            const float vary = unit_of(mix64(h ^ 0x5A5AULL));
            out[count++] = (EngMapTree){.x = (float)x,
                                        .z = (float)z,
                                        .y = y,
                                        .yaw = 6.2831853F * unit_of(mix64(h ^ 0x3C3CULL)),
                                        .scale = TREE_HEIGHT[f->tree] * (0.8F + (0.45F * vary)),
                                        .kind = f->tree};
        }
    }
    return count;
}

uint32_t eng_worldmap_trees(const EngWorldMap *map, double x0, double z0, double size,
                            EngMapTree *out, uint32_t max)
{
    if (map == NULL || map->heights == NULL || out == NULL || !(size > 0.0) || !isfinite(x0) ||
        !isfinite(z0)) {
        return 0U;
    }
    uint32_t count = 0;
    for (uint32_t k = 0; k < map->feature_count && count < max; k++) {
        const EngMapFeature *f = &map->features[k];
        if (f->kind == (uint8_t)ENG_MAPF_FOREST || f->kind == (uint8_t)ENG_MAPF_SCATTER) {
            count = place(map, f, k, x0, z0, size, out, count, max);
        }
    }
    return count;
}

bool eng_worldmap_zones(const EngWorldMap *map, EngMapZone out[ENG_MAPZONE_COUNT])
{
    if (map == NULL || out == NULL || !eng_mapzones_build(NULL, out)) {
        return false;
    }
    for (uint32_t k = 0; k < ENG_MAPZONE_COUNT; k++) {
        if (map->zones[k].used) {
            out[k].x = (double)map->zones[k].x;
            out[k].z = (double)map->zones[k].z;
        }
    }
    return true;
}

const char *eng_tree_name(EngTreeKind kind)
{
    static const char *const names[ENG_TREE_KINDS] = {"chêne", "sapin enneigé", "acacia"};
    return (unsigned)kind < (unsigned)ENG_TREE_KINDS ? names[kind] : "?";
}
