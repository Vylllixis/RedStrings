#include "engine/rock.h"

#include <math.h>
#include <string.h>

#include "engine/log.h"

#define PI_F 3.14159265358979323846F

EngRockParams eng_rock_default_params(void)
{
    return (EngRockParams){.seed = 1,
                           .subdivisions = 2,
                           .radius = 1.0F,
                           .roughness = 0.25F,
                           .flatness = 0.7F,
                           .material = NULL};
}

EngModelMaterial eng_rock_default_material(void)
{
    return (EngModelMaterial){.base_color = {0.42F, 0.40F, 0.38F, 1.0F},
                              .image = -1,
                              .metallic = 0.0F,
                              .roughness = 0.9F,
                              .metallic_roughness_image = -1,
                              .normal_scale = 1.0F,
                              .normal_image = -1,
                              .occlusion_strength = 1.0F,
                              .occlusion_image = -1,
                              .emissive_image = -1};
}

bool eng_rock_params_valid(const EngRockParams *p)
{
    return p != NULL && p->subdivisions <= ENG_ROCK_MAX_SUBDIVISIONS && isfinite(p->radius) &&
           p->radius > 0.0F && isfinite(p->roughness) && p->roughness >= 0.0F &&
           p->roughness < 1.0F && isfinite(p->flatness) && p->flatness > 0.0F &&
           p->flatness <= 1.0F;
}

uint32_t eng_rock_vertex_count(uint32_t n)
{
    return (10U << (2U * n)) + 2U;
}

uint32_t eng_rock_triangle_count(uint32_t n)
{
    return 20U << (2U * n);
}

/* ── bruit de valeur 3D déterministe ── */

static uint32_t hash3(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
    uint32_t h = seed ^ 0x9E3779B9U;
    h ^= (uint32_t)x * 0x85EBCA6BU;
    h = (h << 13) | (h >> 19);
    h ^= (uint32_t)y * 0xC2B2AE35U;
    h = (h << 11) | (h >> 21);
    h ^= (uint32_t)z * 0x27D4EB2FU;
    h ^= h >> 15;
    h *= 0x2C1B3C6DU;
    h ^= h >> 12;
    h *= 0x297A2D39U;
    h ^= h >> 15;
    return h;
}

static float lattice(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
    return (float)(hash3(x, y, z, seed) >> 8) * (1.0F / 16777216.0F); /* [0, 1[ */
}

static float smooth(float t)
{
    return t * t * (3.0F - (2.0F * t));
}

static float mix(float a, float b, float t)
{
    return a + ((b - a) * t);
}

/* bruit de valeur lissé dans [0, 1] */
static float value_noise(float x, float y, float z, uint32_t seed)
{
    const float fx = floorf(x);
    const float fy = floorf(y);
    const float fz = floorf(z);
    const int32_t ix = (int32_t)fx;
    const int32_t iy = (int32_t)fy;
    const int32_t iz = (int32_t)fz;
    const float tx = smooth(x - fx);
    const float ty = smooth(y - fy);
    const float tz = smooth(z - fz);
    float row[4];
    for (int dz = 0; dz < 2; dz++) {
        for (int dy = 0; dy < 2; dy++) {
            row[(dz * 2) + dy] = mix(lattice(ix, iy + dy, iz + dz, seed),
                                     lattice(ix + 1, iy + dy, iz + dz, seed), tx);
        }
    }
    return mix(mix(row[0], row[1], ty), mix(row[2], row[3], ty), tz);
}

/* bruit à deux octaves dans [0, 1] */
static float rock_noise(const float d[3], uint32_t seed)
{
    const float low = value_noise(d[0] * 1.7F, d[1] * 1.7F, d[2] * 1.7F, seed);
    const float high =
        value_noise(d[0] * 3.9F + 11.0F, d[1] * 3.9F + 11.0F, d[2] * 3.9F + 11.0F, seed + 7U);
    return ((low * 2.0F) + high) * (1.0F / 3.0F);
}

/* ── icosphère ── */

typedef struct EdgeSlot {
    uint32_t a; /* UINT32_MAX : vide */
    uint32_t b;
    uint32_t mid;
} EdgeSlot;

static uint32_t edge_midpoint(EdgeSlot *table, uint32_t mask, EngModelVertex *vertices,
                              uint32_t *vertex_count, uint32_t a, uint32_t b)
{
    const uint32_t lo = a < b ? a : b;
    const uint32_t hi = a < b ? b : a;
    uint32_t slot = ((lo * 73856093U) ^ (hi * 19349663U)) & mask;
    while (table[slot].a != UINT32_MAX) {
        if (table[slot].a == lo && table[slot].b == hi) {
            return table[slot].mid;
        }
        slot = (slot + 1U) & mask;
    }
    const uint32_t id = (*vertex_count)++;
    float p[3];
    float length2 = 0.0F;
    for (int c = 0; c < 3; c++) {
        p[c] = 0.5F * (vertices[lo].position[c] + vertices[hi].position[c]);
        length2 += p[c] * p[c];
    }
    const float inv = 1.0F / sqrtf(length2);
    for (int c = 0; c < 3; c++) {
        vertices[id].position[c] = p[c] * inv;
    }
    table[slot] = (EdgeSlot){.a = lo, .b = hi, .mid = id};
    return id;
}

static const uint32_t ICO_FACES[20][3] = {
    {0, 11, 5},  {0, 5, 1},  {0, 1, 7},  {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4},
    {11, 10, 2}, {10, 7, 6}, {7, 1, 8},  {3, 9, 4},  {3, 4, 2},   {3, 2, 6}, {3, 6, 8},
    {3, 8, 9},   {4, 9, 5},  {2, 4, 11}, {6, 2, 10}, {8, 6, 7},   {9, 8, 1}};

bool eng_rock_build(EngArena *arena, const EngRockParams *params, EngModel *model)
{
    *model = (EngModel){0};
    if (!eng_rock_params_valid(params)) {
        ENG_LOG_ERROR("rocher : paramètres invalides (subdivisions <= %u, rayon > 0, rugosité dans "
                      "[0, 1[, aplatissement dans ]0, 1])",
                      ENG_ROCK_MAX_SUBDIVISIONS);
        return false;
    }
    const uint32_t n = params->subdivisions;
    const uint32_t final_vertices = eng_rock_vertex_count(n);
    const uint32_t final_triangles = eng_rock_triangle_count(n);
    const EngArenaMark mark = eng_arena_mark(arena);
    EngModelVertex *vertices = ENG_ARENA_NEW_ARRAY(arena, EngModelVertex, final_vertices);
    uint32_t *indices = ENG_ARENA_NEW_ARRAY(arena, uint32_t, (size_t)final_triangles * 3U);
    EngModelPart *parts = ENG_ARENA_NEW_ARRAY(arena, EngModelPart, 1);
    EngModelMaterial *materials = ENG_ARENA_NEW_ARRAY(arena, EngModelMaterial, 2);
    /* temporaires : second tableau d'index, table des milieux d'arêtes (restitués ensuite) */
    const EngArenaMark temp_mark = eng_arena_mark(arena);
    uint32_t *next = ENG_ARENA_NEW_ARRAY(arena, uint32_t, (size_t)final_triangles * 3U);
    uint32_t table_size = 16U;
    while (table_size < 4U * final_vertices) {
        table_size <<= 1U;
    }
    EdgeSlot *table = ENG_ARENA_NEW_ARRAY(arena, EdgeSlot, table_size);
    float *normals_accum = ENG_ARENA_NEW_ARRAY(arena, float, (size_t)final_vertices * 3U);
    if (vertices == NULL || indices == NULL || parts == NULL || materials == NULL || next == NULL ||
        table == NULL || normals_accum == NULL) {
        eng_arena_reset_to(arena, mark);
        ENG_LOG_ERROR("rocher : arène trop petite");
        return false;
    }
    /* icosaèdre normalisé */
    const float t = (1.0F + sqrtf(5.0F)) * 0.5F;
    const float base[12][3] = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0},
                               {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
                               {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    uint32_t vertex_count = 12;
    for (uint32_t i = 0; i < 12; i++) {
        const float inv = 1.0F / sqrtf((base[i][0] * base[i][0]) + (base[i][1] * base[i][1]) +
                                       (base[i][2] * base[i][2]));
        for (int c = 0; c < 3; c++) {
            vertices[i].position[c] = base[i][c] * inv;
        }
    }
    uint32_t triangles = 20;
    for (uint32_t f = 0; f < 20; f++) {
        for (uint32_t k = 0; k < 3; k++) {
            indices[(f * 3U) + k] = ICO_FACES[f][k];
        }
    }
    uint32_t *current = indices;
    uint32_t *other = next;
    for (uint32_t level = 0; level < n; level++) {
        for (uint32_t i = 0; i < table_size; i++) {
            table[i] = (EdgeSlot){.a = UINT32_MAX};
        }
        const uint32_t mask = table_size - 1U;
        for (uint32_t f = 0; f < triangles; f++) {
            const uint32_t a = current[(size_t)f * 3U];
            const uint32_t b = current[(f * 3U) + 1U];
            const uint32_t c = current[(f * 3U) + 2U];
            const uint32_t ab = edge_midpoint(table, mask, vertices, &vertex_count, a, b);
            const uint32_t bc = edge_midpoint(table, mask, vertices, &vertex_count, b, c);
            const uint32_t ca = edge_midpoint(table, mask, vertices, &vertex_count, c, a);
            const uint32_t quad[12] = {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca};
            memcpy(&other[(size_t)f * 12U], quad, sizeof quad);
        }
        triangles *= 4U;
        uint32_t *swap = current;
        current = other;
        other = swap;
    }
    if (current != indices) {
        memcpy(indices, current, (size_t)triangles * 3U * sizeof(uint32_t));
    }
    /* déplacement radial, aplatissement, UV sphériques */
    float lo[3] = {INFINITY, INFINITY, INFINITY};
    float hi[3] = {-INFINITY, -INFINITY, -INFINITY};
    for (uint32_t i = 0; i < vertex_count; i++) {
        EngModelVertex *v = &vertices[i];
        const float d[3] = {v->position[0], v->position[1], v->position[2]};
        const float r =
            params->radius *
            (1.0F + (params->roughness * ((rock_noise(d, params->seed) * 2.0F) - 1.0F)));
        v->uv[0] = (atan2f(d[2], d[0]) / (2.0F * PI_F)) + 0.5F;
        v->uv[1] = (acosf(fmaxf(-1.0F, fminf(1.0F, d[1]))) / PI_F);
        v->position[0] = d[0] * r;
        v->position[1] = d[1] * r * params->flatness;
        v->position[2] = d[2] * r;
        for (int c = 0; c < 3; c++) {
            lo[c] = fminf(lo[c], v->position[c]);
            hi[c] = fmaxf(hi[c], v->position[c]);
        }
    }
    /* normales lissées : somme des normales de faces pondérées par l'aire (le produit vectoriel) */
    memset(normals_accum, 0, (size_t)vertex_count * 3U * sizeof(float));
    for (uint32_t f = 0; f < triangles; f++) {
        const uint32_t ia = indices[(size_t)f * 3U];
        const uint32_t ib = indices[(f * 3U) + 1U];
        const uint32_t ic = indices[(f * 3U) + 2U];
        const float *pa = vertices[ia].position;
        const float *pb = vertices[ib].position;
        const float *pc = vertices[ic].position;
        const float e1[3] = {pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]};
        const float e2[3] = {pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2]};
        const float cross[3] = {(e1[1] * e2[2]) - (e1[2] * e2[1]),
                                (e1[2] * e2[0]) - (e1[0] * e2[2]),
                                (e1[0] * e2[1]) - (e1[1] * e2[0])};
        const uint32_t ids[3] = {ia, ib, ic};
        for (int k = 0; k < 3; k++) {
            for (int c = 0; c < 3; c++) {
                normals_accum[((size_t)ids[k] * 3U) + (size_t)c] += cross[c];
            }
        }
    }
    for (uint32_t i = 0; i < vertex_count; i++) {
        EngModelVertex *v = &vertices[i];
        float length2 = 0.0F;
        for (int c = 0; c < 3; c++) {
            length2 += normals_accum[((size_t)i * 3U) + (size_t)c] *
                       normals_accum[((size_t)i * 3U) + (size_t)c];
        }
        const float inv = length2 > 0.0F ? 1.0F / sqrtf(length2) : 0.0F;
        for (int c = 0; c < 3; c++) {
            v->normal[c] = normals_accum[((size_t)i * 3U) + (size_t)c] * inv;
        }
        /* tangente : orthogonale à la normale (pas de texture sur les rochers par défaut) */
        const float up[3] = {fabsf(v->normal[1]) < 0.99F ? 0.0F : 1.0F,
                             fabsf(v->normal[1]) < 0.99F ? 1.0F : 0.0F, 0.0F};
        float tangent[3] = {(up[1] * v->normal[2]) - (up[2] * v->normal[1]),
                            (up[2] * v->normal[0]) - (up[0] * v->normal[2]),
                            (up[0] * v->normal[1]) - (up[1] * v->normal[0])};
        const float tl = sqrtf((tangent[0] * tangent[0]) + (tangent[1] * tangent[1]) +
                               (tangent[2] * tangent[2]));
        for (int c = 0; c < 3; c++) {
            v->tangent[c] = tl > 0.0F ? tangent[c] / tl : 0.0F;
        }
        v->tangent[3] = 1.0F;
    }
    eng_arena_reset_to(arena, temp_mark); /* second tableau d'index, table, normales : libérés */
    parts[0] = (EngModelPart){.first_index = 0, .index_count = triangles * 3U, .material = 1};
    materials[0] = eng_rock_default_material();
    materials[1] = params->material != NULL ? *params->material : eng_rock_default_material();
    model->vertices = vertices;
    model->vertex_count = vertex_count;
    model->indices = indices;
    model->index_count = triangles * 3U;
    model->parts = parts;
    model->part_count = 1;
    model->materials = materials;
    model->material_count = 2;
    for (int c = 0; c < 3; c++) {
        model->bounds_min[c] = lo[c];
        model->bounds_max[c] = hi[c];
    }
    return true;
}
