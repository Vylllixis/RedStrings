#include "engine/terrain.h"

#include <math.h>
#include <string.h>

#include "engine/log.h"

#define VERTEX_BYTES sizeof(EngModelVertex)

static const EngModelMaterial DEFAULT_TERRAIN_MATERIAL = {
    .base_color = {0.30F, 0.38F, 0.22F, 1.0F},
    .image = -1,
    .metallic = 0.0F,
    .roughness = 0.95F,
    .metallic_roughness_image = -1,
    .normal_scale = 1.0F,
    .normal_image = -1,
    .occlusion_strength = 1.0F,
    .occlusion_image = -1,
    .emissive = {0.0F, 0.0F, 0.0F},
    .emissive_image = -1,
};

EngModelMaterial eng_terrain_splat_material(void)
{
    EngModelMaterial m = DEFAULT_TERRAIN_MATERIAL;
    for (int c = 0; c < 3; c++) {
        m.base_color[c] = 1.0F;
    }
    return m;
}

static bool material_valid(const EngModelMaterial *m)
{
    for (int c = 0; c < 4; c++) {
        if (!isfinite(m->base_color[c])) {
            return false;
        }
    }
    return isfinite(m->metallic) && isfinite(m->roughness) && isfinite(m->normal_scale) &&
           isfinite(m->occlusion_strength) && m->image < 0 && m->metallic_roughness_image < 0 &&
           m->normal_image < 0 && m->occlusion_image < 0 && m->emissive_image < 0;
}

bool eng_terrain_params_valid(const EngHeightmap *map, const EngTerrainParams *params)
{
    const char *why = NULL;
    if (map == NULL || params == NULL || map->samples == NULL) {
        why = "champ ou paramètres absents";
    } else if (map->size < ENG_HEIGHTMAP_MIN_SIZE || map->size > ENG_HEIGHTMAP_MAX_SIZE ||
               !isfinite(map->spacing) || map->spacing <= 0.0F ||
               !(map->altitude_max > map->altitude_min) || !isfinite(map->altitude_min) ||
               !isfinite(map->altitude_max)) {
        why = "champ de hauteurs invalide";
    } else if (!isfinite(params->origin_x) || !isfinite(params->origin_z)) {
        why = "origine non finie";
    } else {
        const uint32_t cells =
            params->chunk_cells != 0 ? params->chunk_cells : ENG_TERRAIN_DEFAULT_CHUNK_CELLS;
        const uint64_t vertices = (uint64_t)map->size * map->size;
        const uint64_t indices = (uint64_t)(map->size - 1U) * (map->size - 1U) * 6U;
        const uint64_t parts = (uint64_t)((map->size - 1U) / cells) * ((map->size - 1U) / cells);
        if (cells > map->size - 1U || (map->size - 1U) % cells != 0U) {
            why = "(size - 1) doit être un multiple de chunk_cells";
        } else if (vertices > ENG_MODEL_MAX_VERTICES || indices > ENG_MODEL_MAX_INDICES) {
            why = "terrain trop grand pour un modèle (size <= 1673)";
        } else if (parts > ENG_MODEL_MAX_PARTS) {
            why = "trop de chunks (8192 au plus) : augmenter chunk_cells";
        } else if (params->material != NULL && !material_valid(params->material)) {
            why = "matériau invalide (valeurs finies, sans texture)";
        }
    }
    if (why != NULL) {
        ENG_LOG_ERROR("terrain : paramètres invalides : %s", why);
        return false;
    }
    return true;
}

uint32_t eng_terrain_chunk_index(const EngTerrain *terrain, uint32_t cx, uint32_t cz)
{
    if (cx >= terrain->chunks_per_side || cz >= terrain->chunks_per_side) {
        return UINT32_MAX;
    }
    return (cz * terrain->chunks_per_side) + cx;
}

/* Altitude (mètres) de l'échantillon (x, z) du champ. */
static float height_at(const EngHeightmap *map, uint32_t x, uint32_t z)
{
    const float v = (float)map->samples[((size_t)z * map->size) + x];
    return map->altitude_min + ((v / 65535.0F) * (map->altitude_max - map->altitude_min));
}

/* Pente dh/dx ou dh/dz par différence centrée (unilatérale au bord de la grille). */
static float slope(const EngHeightmap *map, uint32_t x, uint32_t z, bool along_x)
{
    const uint32_t last = map->size - 1U;
    const uint32_t i = along_x ? x : z;
    const uint32_t lo = i > 0U ? i - 1U : 0U;
    const uint32_t hi = i < last ? i + 1U : last;
    const float h_lo = along_x ? height_at(map, lo, z) : height_at(map, x, lo);
    const float h_hi = along_x ? height_at(map, hi, z) : height_at(map, x, hi);
    return (h_hi - h_lo) / ((float)(hi - lo) * map->spacing);
}

static void fill_vertices(EngModelVertex *vertices, const EngHeightmap *map,
                          const EngTerrainParams *params)
{
    const uint32_t size = map->size;
    const float inv_last = 1.0F / (float)(size - 1U);
    for (uint32_t z = 0; z < size; z++) {
        for (uint32_t x = 0; x < size; x++) {
            EngModelVertex *v = &vertices[((size_t)z * size) + x];
            const float dhdx = slope(map, x, z, true);
            const float dhdz = slope(map, x, z, false);
            const float inv_len = 1.0F / sqrtf((dhdx * dhdx) + 1.0F + (dhdz * dhdz));
            v->position[0] = params->origin_x + ((float)x * map->spacing);
            v->position[1] = height_at(map, x, z);
            v->position[2] = params->origin_z + ((float)z * map->spacing);
            v->normal[0] = -dhdx * inv_len;
            v->normal[1] = inv_len;
            v->normal[2] = -dhdz * inv_len;
            v->uv[0] = (float)x * inv_last;
            v->uv[1] = (float)z * inv_last;
            /* Tangente : le sens des u croissants (+x) le long de la surface, (1, dh/dx, 0) ;
             * orthogonale à la normale (-dh/dx, 1, -dh/dz) : -dh/dx + dh/dx = 0. */
            const float t_inv = 1.0F / sqrtf(1.0F + (dhdx * dhdx));
            v->tangent[0] = t_inv;
            v->tangent[1] = dhdx * t_inv;
            v->tangent[2] = 0.0F;
            v->tangent[3] = 1.0F;
        }
    }
}

/* Index d'un chunk : deux triangles par cellule, diagonale en damier, anti-horaires vus d'en haut.
 */
static uint32_t *fill_chunk_indices(uint32_t *out, uint32_t size, uint32_t first_x,
                                    uint32_t first_z, uint32_t cells)
{
    for (uint32_t z = first_z; z < first_z + cells; z++) {
        for (uint32_t x = first_x; x < first_x + cells; x++) {
            const uint32_t a = (z * size) + x;
            const uint32_t b = a + 1U;
            const uint32_t c = a + size;
            const uint32_t d = c + 1U;
            if (((x + z) & 1U) == 0U) { /* diagonale a-d */
                *out++ = a;
                *out++ = c;
                *out++ = d;
                *out++ = a;
                *out++ = d;
                *out++ = b;
            } else { /* diagonale b-c */
                *out++ = a;
                *out++ = c;
                *out++ = b;
                *out++ = b;
                *out++ = c;
                *out++ = d;
            }
        }
    }
    return out;
}

static void chunk_bounds(const EngHeightmap *map, const EngTerrainParams *params, uint32_t first_x,
                         uint32_t first_z, uint32_t cells, EngTerrainChunk *chunk)
{
    float low = height_at(map, first_x, first_z);
    float high = low;
    for (uint32_t z = first_z; z <= first_z + cells; z++) {
        for (uint32_t x = first_x; x <= first_x + cells; x++) {
            const float h = height_at(map, x, z);
            low = fminf(low, h);
            high = fmaxf(high, h);
        }
    }
    chunk->bounds_min[0] = params->origin_x + ((float)first_x * map->spacing);
    chunk->bounds_min[1] = low;
    chunk->bounds_min[2] = params->origin_z + ((float)first_z * map->spacing);
    chunk->bounds_max[0] = params->origin_x + ((float)(first_x + cells) * map->spacing);
    chunk->bounds_max[1] = high;
    chunk->bounds_max[2] = params->origin_z + ((float)(first_z + cells) * map->spacing);
}

bool eng_terrain_build(EngArena *arena, const EngHeightmap *map, const EngTerrainParams *params,
                       EngTerrain *out)
{
    *out = (EngTerrain){0};
    if (!eng_terrain_params_valid(map, params)) {
        return false;
    }
    const uint32_t size = map->size;
    const uint32_t cells =
        params->chunk_cells != 0 ? params->chunk_cells : ENG_TERRAIN_DEFAULT_CHUNK_CELLS;
    const uint32_t per_side = (size - 1U) / cells;
    const uint32_t chunk_count = per_side * per_side;
    const uint32_t vertex_count = size * size;
    const uint32_t index_count = (size - 1U) * (size - 1U) * 6U;

    const EngArenaMark mark = eng_arena_mark(arena);
    EngModelVertex *vertices = eng_arena_alloc_array(arena, VERTEX_BYTES, vertex_count, 16);
    uint32_t *indices = eng_arena_alloc_array(arena, sizeof(uint32_t), index_count, 16);
    EngModelPart *parts = ENG_ARENA_NEW_ARRAY(arena, EngModelPart, chunk_count);
    EngTerrainChunk *chunks = ENG_ARENA_NEW_ARRAY(arena, EngTerrainChunk, chunk_count);
    EngModelMaterial *materials = ENG_ARENA_NEW_ARRAY(arena, EngModelMaterial, 2);
    if (vertices == NULL || indices == NULL || parts == NULL || chunks == NULL ||
        materials == NULL) {
        ENG_LOG_ERROR("terrain : arène trop petite (%u sommets, %u index)", (unsigned)vertex_count,
                      (unsigned)index_count);
        eng_arena_reset_to(arena, mark);
        return false;
    }

    fill_vertices(vertices, map, params);

    uint32_t *cursor = indices;
    for (uint32_t cz = 0; cz < per_side; cz++) {
        for (uint32_t cx = 0; cx < per_side; cx++) {
            const uint32_t i = (cz * per_side) + cx;
            parts[i] = (EngModelPart){.first_index = (uint32_t)(cursor - indices),
                                      .index_count = cells * cells * 6U,
                                      .material = 1U};
            cursor = fill_chunk_indices(cursor, size, cx * cells, cz * cells, cells);
            chunk_bounds(map, params, cx * cells, cz * cells, cells, &chunks[i]);
        }
    }

    materials[0] = (EngModelMaterial){.base_color = {1.0F, 1.0F, 1.0F, 1.0F},
                                      .image = -1,
                                      .metallic = 1.0F,
                                      .roughness = 1.0F,
                                      .metallic_roughness_image = -1,
                                      .normal_scale = 1.0F,
                                      .normal_image = -1,
                                      .occlusion_strength = 1.0F,
                                      .occlusion_image = -1,
                                      .emissive_image = -1};
    materials[1] = params->material != NULL ? *params->material : DEFAULT_TERRAIN_MATERIAL;

    EngModel *model = &out->model;
    model->vertices = vertices;
    model->vertex_count = vertex_count;
    model->indices = indices;
    model->index_count = index_count;
    model->parts = parts;
    model->part_count = chunk_count;
    model->materials = materials;
    model->material_count = 2;
    for (int c = 0; c < 3; c++) {
        model->bounds_min[c] = chunks[0].bounds_min[c];
        model->bounds_max[c] = chunks[0].bounds_max[c];
    }
    for (uint32_t i = 1; i < chunk_count; i++) {
        for (int c = 0; c < 3; c++) {
            model->bounds_min[c] = fminf(model->bounds_min[c], chunks[i].bounds_min[c]);
            model->bounds_max[c] = fmaxf(model->bounds_max[c], chunks[i].bounds_max[c]);
        }
    }
    out->chunks = chunks;
    out->chunks_per_side = per_side;
    out->chunk_cells = cells;
    return true;
}

/* ── M7-05 : niveaux de détail d'une cellule carrée ── */

/* Indice, dans la jupe, du sommet de bord (x, z) : le tour de la cellule, sens trigonométrique vu
 * d'en haut à partir du coin (0, 0) ; 4 x n sommets (n = côté - 1). */
static uint32_t perimeter_index(uint32_t n, uint32_t x, uint32_t z)
{
    if (z == 0U) {
        return x; /* bord z = 0 (coin (n, 0) compris) */
    }
    if (x == n) {
        return n + z; /* bord x = n */
    }
    if (z == n) {
        return (2U * n) + (n - x); /* bord z = n */
    }
    return (3U * n) + (n - z); /* bord x = 0 */
}

static uint32_t *push_tri(uint32_t *o, uint32_t a, uint32_t b, uint32_t c)
{
    o[0] = a;
    o[1] = b;
    o[2] = c;
    return o + 3;
}

bool eng_terrain_build_lod(EngArena *arena, const EngModel *grid, uint32_t size, uint32_t levels,
                           float skirt_depth, EngModel *out)
{
    if (arena == NULL || grid == NULL || out == NULL || size < 3U || levels == 0U ||
        levels > ENG_TERRAIN_LOD_MAX_LEVELS || !isfinite(skirt_depth) || skirt_depth < 0.0F ||
        grid->vertex_count != size * size || grid->part_count == 0U ||
        ((size - 1U) % (1U << (levels - 1U))) != 0U) {
        return false;
    }
    const uint32_t res = size;
    const uint32_t n = res - 1U;
    const uint32_t ring = 4U * n;
    const uint32_t material = grid->parts[0].material;
    uint32_t index_total = 0;
    for (uint32_t l = 0; l < levels; l++) {
        const uint32_t q = n >> l;                    /* quads par côté */
        index_total += (q * q * 6U) + (4U * q * 12U); /* surface + jupe double face */
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    EngModelVertex *v =
        ENG_ARENA_NEW_ARRAY(arena, EngModelVertex, (size_t)grid->vertex_count + ring);
    uint32_t *idx = ENG_ARENA_NEW_ARRAY(arena, uint32_t, index_total);
    EngModelPart *parts = ENG_ARENA_NEW_ARRAY(arena, EngModelPart, levels);
    if (v == NULL || idx == NULL || parts == NULL) {
        eng_arena_reset_to(arena, mark);
        return false;
    }
    memcpy(v, grid->vertices, sizeof(EngModelVertex) * grid->vertex_count);
    for (uint32_t z = 0; z < res; z++) { /* sommets abaissés de la jupe */
        for (uint32_t x = 0; x < res; x++) {
            if (x != 0U && z != 0U && x != n && z != n) {
                continue;
            }
            EngModelVertex *sv = &v[grid->vertex_count + perimeter_index(n, x, z)];
            *sv = grid->vertices[(z * res) + x];
            sv->position[1] -= skirt_depth;
        }
    }
    uint32_t *o = idx;
    for (uint32_t l = 0; l < levels; l++) {
        const uint32_t step = 1U << l;
        const uint32_t q = n >> l;
        parts[l].first_index = (uint32_t)(o - idx);
        parts[l].material = material;
        for (uint32_t z = 0; z < q; z++) {
            for (uint32_t x = 0; x < q; x++) {
                const uint32_t a = (z * step * res) + (x * step);
                const uint32_t b = a + step;
                const uint32_t c = a + (step * res);
                const uint32_t d = c + step;
                if (((x + z) & 1U) == 0U) {
                    o = push_tri(o, a, c, d);
                    o = push_tri(o, a, d, b);
                } else {
                    o = push_tri(o, a, c, b);
                    o = push_tri(o, b, c, d);
                }
            }
        }
        /* jupe : chaque arête du bord (pas `step`) et sa copie abaissée, deux faces */
        for (uint32_t k = 0; k < 4U * q; k++) {
            const uint32_t p0 = k * step;
            const uint32_t p1 = ((k + 1U) * step) % ring;
            uint32_t g[2] = {0, 0};
            for (int e = 0; e < 2; e++) { /* périmètre -> sommet de la grille */
                const uint32_t p = e == 0 ? p0 : p1;
                uint32_t x = 0;
                uint32_t z = 0;
                if (p < n) {
                    x = p;
                } else if (p < 2U * n) {
                    x = n;
                    z = p - n;
                } else if (p < 3U * n) {
                    x = n - (p - (2U * n));
                    z = n;
                } else {
                    z = n - (p - (3U * n));
                }
                g[e] = (z * res) + x;
            }
            const uint32_t g0 = g[0];
            const uint32_t g1 = g[1];
            const uint32_t s0 = grid->vertex_count + p0;
            const uint32_t s1 = grid->vertex_count + p1;
            o = push_tri(o, g0, s0, g1);
            o = push_tri(o, g1, s0, s1);
            o = push_tri(o, g0, g1, s0);
            o = push_tri(o, g1, s1, s0);
        }
        parts[l].index_count = (uint32_t)(o - idx) - parts[l].first_index;
    }
    *out = *grid;
    out->vertices = v;
    out->vertex_count = grid->vertex_count + ring;
    out->indices = idx;
    out->index_count = (uint32_t)(o - idx);
    out->parts = parts;
    out->part_count = levels;
    out->bounds_min[1] -= skirt_depth;
    return out->index_count == index_total;
}
