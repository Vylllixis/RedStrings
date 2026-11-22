/* Carte du monde : arbres (forêts, arbres épars) et places des zones de combat. Placement fixe :
 * une grille du monde par forme, chaque nœud décalé et accepté selon une empreinte de ses
 * coordonnées (aucun aléa, même résultat quel que soit le découpage en cellules). */
#include <math.h>

#include "engine/world_map.h"

#define GROUP_RADIUS_M 7.0F /* un groupe tient dans ce rayon autour de son emplacement */

/* par essence : taille typique (m), distance au départ et aux zones de combat (m), pente maximale
 */
static const float TREE_HEIGHT[ENG_TREE_KINDS] = {9.0F, 11.0F, 6.5F, 1.3F, 1.2F, 1.0F, 1.5F};
static const float KEEP_OFF_SPAWN[ENG_TREE_KINDS] = {70.0F, 70.0F, 70.0F, 34.0F,
                                                     34.0F, 34.0F, 40.0F};
static const float KEEP_OFF_ZONE[ENG_TREE_KINDS] = {32.0F, 32.0F, 32.0F, 26.0F,
                                                    26.0F, 26.0F, 32.0F};
static const float MAX_SLOPE[ENG_TREE_KINDS] = {0.7F, 0.7F, 0.7F, 0.7F, 0.7F, 0.7F, 1.1F};

bool eng_tree_is_solid(EngTreeKind kind)
{
    return kind == ENG_TREE_OAK || kind == ENG_TREE_PINE || kind == ENG_TREE_ACACIA ||
           kind == ENG_TREE_ROCK;
}

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

/* un pied de l'essence `kind` peut-il être en (x, z) ? `w` reçoit les poids des biomes */
static bool ground_ok(const EngWorldMap *m, uint8_t kind, float x, float z, float *y,
                      float w[ENG_BIOME_COUNT])
{
    float path = 0.0F;
    eng_worldmap_weights(m, x, z, w, &path);
    const bool living = kind != (uint8_t)ENG_TREE_ROCK; /* rien ne pousse dans le volcan */
    if (path > 0.05F || (living && w[ENG_BIOME_VOLCANO] > 0.25F) ||
        eng_worldmap_lava(m, x, z) > 0.0F) {
        return false;
    }
    const float h = eng_worldmap_height(m, x, z);
    if (h < m->water + 0.8F) {
        return false;
    }
    const float dx = eng_worldmap_height(m, x + 2.0, z) - eng_worldmap_height(m, x - 2.0, z);
    const float dz = eng_worldmap_height(m, x, z + 2.0) - eng_worldmap_height(m, x, z - 2.0);
    if (hypotf(dx, dz) * 0.25F > MAX_SLOPE[kind]) {
        return false;
    }
    if (hypotf(x - m->spawn[0], z - m->spawn[1]) < KEEP_OFF_SPAWN[kind]) {
        return false;
    }
    for (uint32_t k = 0; k < ENG_MAPZONE_COUNT; k++) {
        if (m->zones[k].used &&
            hypotf(x - m->zones[k].x, z - m->zones[k].z) < KEEP_OFF_ZONE[kind]) {
            return false;
        }
    }
    *y = h;
    return true;
}

/* teinte d'un pied : les rochers prennent la roche de leur biome ; le reste varie un peu */
static void tint_of(uint8_t kind, const float w[ENG_BIOME_COUNT], float vary, uint8_t out[3])
{
    if (kind == (uint8_t)ENG_TREE_ROCK) {
        static const float rock[ENG_BIOME_COUNT][3] = {{255.0F, 255.0F, 255.0F},
                                                       {236.0F, 242.0F, 252.0F},
                                                       {236.0F, 198.0F, 150.0F},
                                                       {104.0F, 94.0F, 94.0F}};
        for (int c = 0; c < 3; c++) {
            float v = 0.0F;
            for (int b = 0; b < ENG_BIOME_COUNT; b++) {
                v += w[b] * rock[b][c];
            }
            out[c] = (uint8_t)lroundf(v * (0.88F + (0.12F * vary)));
        }
        return;
    }
    const uint8_t shade = (uint8_t)lroundf(205.0F + (50.0F * vary));
    out[0] = shade;
    out[1] = shade;
    out[2] = shade;
}

/* pieds de la forme `f` (indice `index`) dans le carré, ajoutés à `out`. Un emplacement de la
 * grille porte un pied, ou un groupe de `f->b` pieds au plus pour les objets épars : le premier à
 * l'emplacement même, les autres à moins de GROUP_RADIUS_M. Chaque pied appartient au carré qui
 * contient SA position : on visite donc aussi les emplacements voisins du carré. */
static uint32_t place(const EngWorldMap *m, const EngMapFeature *f, uint32_t index, double x0,
                      double z0, double size, EngMapTree *out, uint32_t count, uint32_t max)
{
    const double s = (double)f->a;
    const bool forest = f->kind == (uint8_t)ENG_MAPF_FOREST;
    const uint32_t group = !forest && f->b >= 2.0F ? (uint32_t)f->b : 1U;
    const double reach = group > 1U ? (double)GROUP_RADIUS_M : 0.0;
    double lo_x = x0 - reach;
    double lo_z = z0 - reach;
    double hi_x = x0 + size + reach;
    double hi_z = z0 + size + reach;
    if (forest) { /* seulement la boîte du disque */
        lo_x = fmax(lo_x, (double)(f->x - f->radius));
        lo_z = fmax(lo_z, (double)(f->z - f->radius));
        hi_x = fmin(hi_x, (double)(f->x + f->radius));
        hi_z = fmin(hi_z, (double)(f->z + f->radius));
    }
    const double half_map = 0.5 * (double)m->side;
    for (int64_t gz = (int64_t)floor(lo_z / s); (double)gz * s < hi_z + s; gz++) {
        for (int64_t gx = (int64_t)floor(lo_x / s); (double)gx * s < hi_x + s; gx++) {
            const uint64_t h = node_key(gx, gz, index);
            /* décalage dans la maille : jusqu'à 40 % de l'espacement */
            const double nx = ((double)gx + 0.5 + (0.8 * ((double)unit_of(h) - 0.5))) * s;
            const double nz = ((double)gz + 0.5 + (0.8 * ((double)unit_of(mix64(h)) - 0.5))) * s;
            if (fabs(nx) > half_map || fabs(nz) > half_map) {
                continue;
            }
            const float keep = unit_of(mix64(h ^ 0xA5A5ULL));
            float w[ENG_BIOME_COUNT];
            if (forest) {
                /* lisière : la densité baisse sur le dernier tiers du rayon */
                const float r = hypotf((float)nx - f->x, (float)nz - f->z) / f->radius;
                if (r >= 1.0F || keep > (r < 0.66F ? 0.92F : 0.92F * (1.0F - r) / 0.34F)) {
                    continue;
                }
            } else {
                eng_worldmap_weights(m, nx, nz, w, NULL);
                if (w[f->biome] < 0.7F || keep > 0.75F) {
                    continue;
                }
            }
            const uint32_t members =
                group > 1U ? 1U + (uint32_t)(mix64(h ^ 0x1111ULL) % group) : 1U;
            for (uint32_t j = 0; j < members; j++) {
                const uint64_t hj = j == 0U ? h : mix64(h ^ (0x9000ULL + j));
                double x = nx;
                double z = nz;
                if (j > 0U) { /* les autres pieds du groupe, autour du premier */
                    const double angle = 6.283185307179586 * (double)unit_of(hj);
                    const double dist =
                        2.5 + (((double)GROUP_RADIUS_M - 2.5) * (double)unit_of(mix64(hj)));
                    x += dist * cos(angle);
                    z += dist * sin(angle);
                }
                if (x < x0 || x >= x0 + size || z < z0 || z >= z0 + size) {
                    continue;
                }
                float y = 0.0F;
                if (!ground_ok(m, f->tree, (float)x, (float)z, &y, w)) {
                    continue;
                }
                if (count >= max) {
                    return count;
                }
                const float vary = unit_of(mix64(hj ^ 0x5A5AULL));
                /* rochers : de 0,55 à 1,75 fois la taille type ; le reste : de 0,8 à 1,25 */
                const float size_k = f->tree == (uint8_t)ENG_TREE_ROCK ? 0.55F + (1.2F * vary)
                                                                       : 0.8F + (0.45F * vary);
                EngMapTree *t = &out[count++];
                *t = (EngMapTree){.x = (float)x,
                                  .z = (float)z,
                                  .y = y,
                                  .yaw = 6.2831853F * unit_of(mix64(hj ^ 0x3C3CULL)),
                                  .scale = TREE_HEIGHT[f->tree] * size_k,
                                  .kind = f->tree};
                tint_of(f->tree, w, unit_of(mix64(hj ^ 0x7E7EULL)), t->tint);
            }
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
    static const char *const names[ENG_TREE_KINDS] = {
        "chêne", "sapin enneigé", "acacia", "buisson", "buisson enneigé", "buisson sec", "rocher"};
    return (unsigned)kind < (unsigned)ENG_TREE_KINDS ? names[kind] : "?";
}
