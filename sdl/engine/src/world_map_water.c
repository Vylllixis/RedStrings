/* Carte du monde : la surface de l'eau (lacs, rivières). Un maillage compact : les cellules de la
 * trame dont au moins un coin est sous le niveau de l'eau (le relief cache le reste : la rive est
 * la ligne où il perce le plan), à l'altitude `map->water`. Mêmes conventions que eng_water_build
 * (engine/water.h) : uv.x = profondeur au sommet, mais SIGNÉE (négative où le relief dépasse),
 * normale +Y ; dessiné avec water.frag. */
#include <math.h>
#include <string.h>

#include "engine/world_map.h"

static bool wet_cell(const EngWorldMap *m, uint32_t i, uint32_t j)
{
    const float *h = &m->heights[((size_t)j * m->size) + i];
    return h[0] < m->water || h[1] < m->water || h[m->size] < m->water ||
           h[m->size + 1U] < m->water;
}

uint32_t eng_worldmap_wet_cells(const EngWorldMap *map)
{
    if (map == NULL || map->heights == NULL || map->size < 2U) {
        return 0U;
    }
    uint32_t wet = 0;
    for (uint32_t j = 0; j + 1U < map->size; j++) {
        for (uint32_t i = 0; i + 1U < map->size; i++) {
            wet += wet_cell(map, i, j) ? 1U : 0U;
        }
    }
    return wet;
}

bool eng_worldmap_water(const EngWorldMap *map, EngArena *arena, EngModel *out)
{
    if (out == NULL) {
        return false;
    }
    *out = (EngModel){0};
    if (map == NULL || map->heights == NULL || map->size < 2U || arena == NULL) {
        return false;
    }
    const uint32_t wet = eng_worldmap_wet_cells(map);
    if (wet == 0U) {
        return true; /* carte sans eau : modèle vide */
    }
    const size_t samples = (size_t)map->size * map->size;
    const EngArenaMark mark = eng_arena_mark(arena);
    /* au plus quatre sommets par cellule mouillée ; partagés entre cellules voisines */
    const size_t max_vertices = (size_t)wet * 4U;
    EngModelVertex *vertices = ENG_ARENA_NEW_ARRAY(arena, EngModelVertex, max_vertices);
    uint32_t *indices = ENG_ARENA_NEW_ARRAY(arena, uint32_t, (size_t)wet * 6U);
    EngModelPart *parts = ENG_ARENA_NEW_ARRAY(arena, EngModelPart, 1);
    EngModelMaterial *materials = ENG_ARENA_NEW_ARRAY(arena, EngModelMaterial, 1);
    const EngArenaMark scratch = eng_arena_mark(arena);
    uint32_t *slot = ENG_ARENA_NEW_ARRAY(arena, uint32_t, samples); /* échantillon -> sommet */
    if (vertices == NULL || indices == NULL || parts == NULL || materials == NULL || slot == NULL ||
        max_vertices > ENG_MODEL_MAX_VERTICES || (size_t)wet * 6U > ENG_MODEL_MAX_INDICES) {
        eng_arena_reset_to(arena, mark);
        return false;
    }
    memset(slot, 0xFF, samples * sizeof *slot);
    const float half = 0.5F * map->side;
    uint32_t vertex_count = 0;
    uint32_t *index = indices;
    for (int a = 0; a < 3; a++) {
        out->bounds_min[a] = INFINITY;
        out->bounds_max[a] = -INFINITY;
    }
    for (uint32_t j = 0; j + 1U < map->size; j++) {
        for (uint32_t i = 0; i + 1U < map->size; i++) {
            if (!wet_cell(map, i, j)) {
                continue;
            }
            uint32_t corner[4];
            for (uint32_t k = 0; k < 4U; k++) {
                const uint32_t si = i + (k & 1U);
                const uint32_t sj = j + (k >> 1U);
                const size_t at = ((size_t)sj * map->size) + si;
                if (slot[at] == UINT32_MAX) {
                    slot[at] = vertex_count;
                    EngModelVertex *v = &vertices[vertex_count++];
                    *v = (EngModelVertex){.position = {-half + ((float)si * map->spacing),
                                                       map->water,
                                                       -half + ((float)sj * map->spacing)},
                                          .normal = {0.0F, 1.0F, 0.0F},
                                          .tangent = {1.0F, 0.0F, 0.0F, 1.0F}};
                    /* profondeur signée : négative là où le relief dépasse (water.frag écarte
                     * alors le fragment : la rive suit le relief, pas les cellules) */
                    v->uv[0] = map->water - map->heights[at];
                    for (int a = 0; a < 3; a++) {
                        out->bounds_min[a] = fminf(out->bounds_min[a], v->position[a]);
                        out->bounds_max[a] = fmaxf(out->bounds_max[a], v->position[a]);
                    }
                }
                corner[k] = slot[at];
            }
            /* anti-horaire vu du dessus (+Y), comme eng_water_build : (x, z), (x, z+1), (x+1, z) */
            const uint32_t quad[6] = {corner[0], corner[2], corner[1],
                                      corner[1], corner[2], corner[3]};
            memcpy(index, quad, sizeof quad);
            index += 6;
        }
    }
    eng_arena_reset_to(arena, scratch); /* la table de correspondance n'est plus utile */
    parts[0] = (EngModelPart){.first_index = 0, .index_count = wet * 6U, .material = 0};
    materials[0] = (EngModelMaterial){.base_color = {1.0F, 1.0F, 1.0F, 1.0F},
                                      .image = -1,
                                      .metallic = 0.0F,
                                      .roughness = 0.05F,
                                      .metallic_roughness_image = -1,
                                      .normal_scale = 1.0F,
                                      .normal_image = -1,
                                      .occlusion_strength = 1.0F,
                                      .occlusion_image = -1,
                                      .emissive_image = -1};
    out->vertices = vertices;
    out->vertex_count = vertex_count;
    out->indices = indices;
    out->index_count = wet * 6U;
    out->parts = parts;
    out->part_count = 1;
    out->materials = materials;
    out->material_count = 1;
    return true;
}

bool eng_worldmap_near_water(const EngWorldMap *map, double x, double z, double reach)
{
    if (map == NULL || map->heights == NULL || !isfinite(x) || !isfinite(z) || !(reach >= 0.0)) {
        return false;
    }
    const double half = 0.5 * (double)map->side;
    const double s = (double)map->spacing;
    const long last = (long)map->size - 1L;
    long i0 = (long)floor((x - reach + half) / s);
    long i1 = (long)ceil((x + reach + half) / s);
    long j0 = (long)floor((z - reach + half) / s);
    long j1 = (long)ceil((z + reach + half) / s);
    i0 = i0 < 0 ? 0 : i0;
    j0 = j0 < 0 ? 0 : j0;
    i1 = i1 > last ? last : i1;
    j1 = j1 > last ? last : j1;
    for (long j = j0; j <= j1; j++) {
        for (long i = i0; i <= i1; i++) {
            if (map->heights[((size_t)j * map->size) + (size_t)i] < map->water) {
                return true;
            }
        }
    }
    return false;
}
