#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/heightmap.h"
#include "engine/terrain.h"
#include "utest.h"

/* Maillage du terrain par chunks (M3-02). Les champs sont fabriqués à la main avec
 * altitude_min = 0 et altitude_max = 65535 : l'altitude en mètres est alors EXACTEMENT la valeur 16
 * bits, ce qui permet des attendus analytiques (plan, paraboloïde). */

static EngHeightmap make_map(uint16_t *samples, uint32_t size, float spacing)
{
    return (EngHeightmap){.size = size,
                          .spacing = spacing,
                          .altitude_min = 0.0F,
                          .altitude_max = 65535.0F,
                          .samples = samples};
}

/* Plan incliné : h = 1000 + 3x - 2z (dh/dx = 3, dh/dz = -2 par échantillon). */
static void fill_plane(uint16_t *samples, uint32_t size)
{
    for (uint32_t z = 0; z < size; z++) {
        for (uint32_t x = 0; x < size; x++) {
            samples[(z * size) + x] = (uint16_t)(1000U + (3U * x) + (2U * (size - 1U - z)));
        }
    }
}

/* Paraboloïde : h = x^2 + z^2 (la différence centrée y est exacte : 2x). */
static void fill_paraboloid(uint16_t *samples, uint32_t size)
{
    for (uint32_t z = 0; z < size; z++) {
        for (uint32_t x = 0; x < size; x++) {
            samples[(z * size) + x] = (uint16_t)((x * x) + (z * z));
        }
    }
}

typedef struct Fixture {
    EngArena arena;
    uint16_t *samples;
    EngHeightmap map;
    EngTerrain terrain;
} Fixture;

static bool open_fixture(Fixture *f, uint32_t size, float spacing,
                         void (*fill)(uint16_t *, uint32_t), const EngTerrainParams *params)
{
    *f = (Fixture){0};
    if (!eng_arena_init(&f->arena, "test-terrain", (size_t)96 << 20)) {
        return false;
    }
    f->samples = eng_arena_alloc(&f->arena, (size_t)size * size * sizeof(uint16_t), 16);
    if (f->samples == NULL) {
        return false;
    }
    fill(f->samples, size);
    f->map = make_map(f->samples, size, spacing);
    return eng_terrain_build(&f->arena, &f->map, params, &f->terrain);
}

static void close_fixture(Fixture *f)
{
    eng_arena_shutdown(&f->arena);
}

static uint32_t min3(uint32_t a, uint32_t b, uint32_t c)
{
    const uint32_t ab = a < b ? a : b;
    return ab < c ? ab : c;
}

static float len3(const float v[3])
{
    return sqrtf((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
}

UTEST(eng_terrain, parameters_are_validated_against_the_heightmap_and_the_model_limits)
{
    static uint16_t samples[129 * 129];
    EngHeightmap map = make_map(samples, 129, 1.0F);
    EngTerrainParams p = {0};
    EXPECT_TRUE(eng_terrain_params_valid(&map, &p)); /* chunk_cells 0 : 64, 128 = 2 x 64 */
    p.chunk_cells = 32;
    EXPECT_TRUE(eng_terrain_params_valid(&map, &p));
    p.chunk_cells = 128;
    EXPECT_TRUE(eng_terrain_params_valid(&map, &p)); /* un seul chunk */
    p.chunk_cells = 129;
    EXPECT_FALSE(eng_terrain_params_valid(&map, &p));
    p.chunk_cells = 130; /* plus grand que le champ : refusé même si 130 divise size + 1 */
    EXPECT_FALSE(eng_terrain_params_valid(&map, &p));
    p.chunk_cells = 100; /* ne divise pas 128 */
    EXPECT_FALSE(eng_terrain_params_valid(&map, &p));
    p.chunk_cells = 0;
    EXPECT_FALSE(eng_terrain_params_valid(NULL, &p));
    EXPECT_FALSE(eng_terrain_params_valid(&map, NULL));
    EngHeightmap bad = map;
    bad.samples = NULL;
    EXPECT_FALSE(eng_terrain_params_valid(&bad, &p));
    bad = map;
    bad.spacing = 0.0F;
    EXPECT_FALSE(eng_terrain_params_valid(&bad, &p));
    bad = map;
    bad.altitude_max = bad.altitude_min;
    EXPECT_FALSE(eng_terrain_params_valid(&bad, &p));
    p.origin_x = NAN;
    EXPECT_FALSE(eng_terrain_params_valid(&map, &p));
    p.origin_x = 0.0F;
    p.origin_z = INFINITY;
    EXPECT_FALSE(eng_terrain_params_valid(&map, &p));
    p.origin_z = 0.0F;
    /* Limites du modèle (16 777 216 index) : 1673 passe (1672² x 6 = 16 773 504), 1674 dépasse
     * (1673² x 6 = 16 793 574 ; 1673 = 7 x 239). */
    bad = map;
    bad.size = 1673;
    p.chunk_cells = 209; /* 1672 = 8 x 209 */
    EXPECT_TRUE(eng_terrain_params_valid(&bad, &p));
    bad.size = 1674;
    p.chunk_cells = 239;
    EXPECT_FALSE(eng_terrain_params_valid(&bad, &p));
    /* Trop de chunks : 1025 / 8 cellules = 128 x 128 = 16384 parties. */
    bad.size = 1025;
    p.chunk_cells = 8;
    EXPECT_FALSE(eng_terrain_params_valid(&bad, &p));
    /* Matériau : texture refusée (le terrain n'a pas d'images), valeur non finie refusée. */
    p.chunk_cells = 32;
    EngModelMaterial m = {.base_color = {1, 1, 1, 1},
                          .image = -1,
                          .metallic_roughness_image = -1,
                          .normal_image = -1,
                          .occlusion_image = -1,
                          .emissive_image = -1};
    p.material = &m;
    EXPECT_TRUE(eng_terrain_params_valid(&map, &p));
    m.image = 0;
    EXPECT_FALSE(eng_terrain_params_valid(&map, &p));
    m.image = -1;
    m.roughness = NAN;
    EXPECT_FALSE(eng_terrain_params_valid(&map, &p));
}

/* Chaque champ du matériau est contrôlé, et les bornes des limites sont exactes. */
UTEST(eng_terrain, every_material_field_and_every_limit_is_checked_at_its_boundary)
{
    static uint16_t samples[129 * 129];
    const EngHeightmap map = make_map(samples, 129, 1.0F);
    EngModelMaterial m = {.base_color = {1, 1, 1, 1},
                          .image = -1,
                          .metallic_roughness_image = -1,
                          .normal_image = -1,
                          .occlusion_image = -1,
                          .emissive_image = -1};
    EngTerrainParams p = {.chunk_cells = 32, .material = &m};
    EXPECT_TRUE(eng_terrain_params_valid(&map, &p));
    int *const images[5] = {&m.image, &m.metallic_roughness_image, &m.normal_image,
                            &m.occlusion_image, &m.emissive_image};
    for (int i = 0; i < 5; i++) {
        *images[i] = 0; /* l'image 0 existe : refusée */
        EXPECT_FALSE(eng_terrain_params_valid(&map, &p));
        *images[i] = -1;
    }
    float *const values[7] = {&m.base_color[0], &m.base_color[1], &m.base_color[2],
                              &m.base_color[3], &m.metallic,      &m.roughness,
                              &m.normal_scale};
    for (int i = 0; i < 7; i++) {
        const float saved = *values[i];
        *values[i] = NAN;
        EXPECT_FALSE(eng_terrain_params_valid(&map, &p));
        *values[i] = saved;
    }
    m.occlusion_strength = INFINITY;
    EXPECT_FALSE(eng_terrain_params_valid(&map, &p));
    m.occlusion_strength = 1.0F;
    EXPECT_TRUE(eng_terrain_params_valid(&map, &p));

    /* Bornes : le plus petit champ (2 x 2, une cellule) passe, 1 x 1 non. */
    p.material = NULL;
    EngHeightmap tiny = map;
    tiny.size = 2;
    p.chunk_cells = 1;
    EXPECT_TRUE(eng_terrain_params_valid(&tiny, &p));
    tiny.size = 1;
    EXPECT_FALSE(eng_terrain_params_valid(&tiny, &p));
    /* 8192 parties au plus : 90 x 90 = 8100 passe, 91 x 91 = 8281 non. */
    tiny.size = 91;
    EXPECT_TRUE(eng_terrain_params_valid(&tiny, &p));
    tiny.size = 92;
    EXPECT_FALSE(eng_terrain_params_valid(&tiny, &p));
    /* spacing négatif et altitudes inversées */
    tiny = map;
    p.chunk_cells = 32;
    tiny.spacing = -1.0F;
    EXPECT_FALSE(eng_terrain_params_valid(&tiny, &p));
    tiny = map;
    tiny.altitude_max = tiny.altitude_min - 1.0F;
    EXPECT_FALSE(eng_terrain_params_valid(&tiny, &p));
    tiny = map;
    tiny.altitude_min = NAN;
    EXPECT_FALSE(eng_terrain_params_valid(&tiny, &p));
    tiny = map;
    tiny.altitude_max = INFINITY;
    EXPECT_FALSE(eng_terrain_params_valid(&tiny, &p));
    tiny = map;
    tiny.spacing = INFINITY;
    EXPECT_FALSE(eng_terrain_params_valid(&tiny, &p));
}

UTEST(eng_terrain, counts_and_layout_follow_the_documented_structure)
{
    Fixture f;
    ASSERT_TRUE(open_fixture(&f, 129, 1.0F, fill_plane, &(EngTerrainParams){.chunk_cells = 32}));
    const EngModel *m = &f.terrain.model;
    EXPECT_EQ(m->vertex_count, (uint32_t)129 * 129);
    EXPECT_EQ(m->index_count, (uint32_t)128 * 128 * 6);
    EXPECT_EQ(f.terrain.chunks_per_side, (uint32_t)4);
    EXPECT_EQ(f.terrain.chunk_cells, (uint32_t)32);
    EXPECT_EQ(m->part_count, (uint32_t)16);
    EXPECT_EQ(m->material_count, (uint32_t)2);
    EXPECT_EQ(m->image_count, (uint32_t)0);
    for (uint32_t i = 0; i < m->part_count; i++) {
        EXPECT_EQ(m->parts[i].index_count, (uint32_t)32 * 32 * 6);
        EXPECT_EQ(m->parts[i].first_index, i * 32U * 32U * 6U);
        EXPECT_EQ(m->parts[i].material, (uint32_t)1);
    }
    for (uint32_t i = 0; i < m->index_count; i++) {
        ASSERT_LT(m->indices[i], m->vertex_count);
    }
    EXPECT_EQ(eng_terrain_chunk_index(&f.terrain, 0, 0), (uint32_t)0);
    EXPECT_EQ(eng_terrain_chunk_index(&f.terrain, 3, 0), (uint32_t)3);
    EXPECT_EQ(eng_terrain_chunk_index(&f.terrain, 0, 2), (uint32_t)8);
    EXPECT_EQ(eng_terrain_chunk_index(&f.terrain, 3, 3), (uint32_t)15);
    EXPECT_EQ(eng_terrain_chunk_index(&f.terrain, 4, 0), UINT32_MAX);
    EXPECT_EQ(eng_terrain_chunk_index(&f.terrain, 0, 4), UINT32_MAX);
    close_fixture(&f);
}

UTEST(eng_terrain, the_default_chunk_is_64_cells_and_materials_are_as_documented)
{
    Fixture f;
    ASSERT_TRUE(open_fixture(&f, 129, 1.0F, fill_plane, &(EngTerrainParams){0}));
    EXPECT_EQ(f.terrain.chunk_cells, (uint32_t)ENG_TERRAIN_DEFAULT_CHUNK_CELLS);
    EXPECT_EQ(f.terrain.chunks_per_side, (uint32_t)2);
    const EngModelMaterial *m = f.terrain.model.materials;
    /* materials[0] : défaut de glTF (blanc, métal 1, rugosité 1) ; materials[1] : le terrain. */
    EXPECT_EQ(m[0].base_color[0], 1.0F);
    EXPECT_EQ(m[0].metallic, 1.0F);
    EXPECT_EQ(m[0].roughness, 1.0F);
    EXPECT_EQ(m[1].metallic, 0.0F);
    EXPECT_NEAR(m[1].roughness, 0.95F, 1e-6F);
    EXPECT_EQ(m[1].image, -1);
    close_fixture(&f);

    EngModelMaterial custom = {.base_color = {0.5F, 0.4F, 0.3F, 1.0F},
                               .image = -1,
                               .metallic = 0.25F,
                               .roughness = 0.6F,
                               .metallic_roughness_image = -1,
                               .normal_scale = 1.0F,
                               .normal_image = -1,
                               .occlusion_strength = 1.0F,
                               .occlusion_image = -1,
                               .emissive_image = -1};
    ASSERT_TRUE(open_fixture(&f, 129, 1.0F, fill_plane, &(EngTerrainParams){.material = &custom}));
    EXPECT_EQ(f.terrain.model.materials[1].base_color[0], 0.5F);
    EXPECT_EQ(f.terrain.model.materials[1].metallic, 0.25F);
    EXPECT_EQ(f.terrain.model.materials[1].roughness, 0.6F);
    close_fixture(&f);
}

/* Chaque cellule du terrain est couverte par exactement deux triangles, dans le chunk qui la
 * contient et dans le sens direct vu d'en haut : aucun trou, aucun recouvrement entre chunks. */
UTEST(eng_terrain, every_cell_is_covered_by_exactly_two_upward_triangles_of_its_own_chunk)
{
    Fixture f;
    ASSERT_TRUE(
        open_fixture(&f, 97, 2.0F, fill_paraboloid, &(EngTerrainParams){.chunk_cells = 32}));
    const EngModel *m = &f.terrain.model;
    const uint32_t size = 97;
    static uint8_t coverage[96 * 96];
    memset(coverage, 0, sizeof coverage);
    for (uint32_t part = 0; part < m->part_count; part++) {
        const uint32_t cx = part % f.terrain.chunks_per_side;
        const uint32_t cz = part / f.terrain.chunks_per_side;
        const EngModelPart p = m->parts[part];
        for (uint32_t t = 0; t < p.index_count; t += 3) {
            const uint32_t i0 = m->indices[p.first_index + t];
            const uint32_t i1 = m->indices[p.first_index + t + 1U];
            const uint32_t i2 = m->indices[p.first_index + t + 2U];
            const uint32_t xs[3] = {i0 % size, i1 % size, i2 % size};
            const uint32_t zs[3] = {i0 / size, i1 / size, i2 / size};
            const uint32_t min_x = min3(xs[0], xs[1], xs[2]);
            const uint32_t min_z = min3(zs[0], zs[1], zs[2]);
            ASSERT_GE(min_x, cx * 32U);
            ASSERT_LT(min_x, (cx + 1U) * 32U);
            ASSERT_GE(min_z, cz * 32U);
            ASSERT_LT(min_z, (cz + 1U) * 32U);
            coverage[((size_t)min_z * 96U) + min_x]++;
            /* normale géométrique vers +Y */
            const float *a = m->vertices[i0].position;
            const float *b = m->vertices[i1].position;
            const float *c = m->vertices[i2].position;
            const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
            const float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
            const float ny = (e1[2] * e2[0]) - (e1[0] * e2[2]);
            ASSERT_GT(ny, 0.0F);
        }
    }
    for (size_t i = 0; i < (size_t)96 * 96; i++) {
        ASSERT_EQ(coverage[i], (uint8_t)2);
    }
    close_fixture(&f);
}

UTEST(eng_terrain, the_diagonal_alternates_like_a_checkerboard)
{
    Fixture f;
    ASSERT_TRUE(open_fixture(&f, 129, 1.0F, fill_plane, &(EngTerrainParams){.chunk_cells = 64}));
    const uint32_t *idx = f.terrain.model.indices;
    const uint32_t size = 129;
    /* Cellule (0,0) : diagonale a-d, triangles (a, c, d) puis (a, d, b). */
    EXPECT_EQ(idx[0], (uint32_t)0);
    EXPECT_EQ(idx[1], size);
    EXPECT_EQ(idx[2], size + 1U);
    EXPECT_EQ(idx[3], (uint32_t)0);
    EXPECT_EQ(idx[4], size + 1U);
    EXPECT_EQ(idx[5], (uint32_t)1);
    /* Cellule (1,0) : diagonale b-c, triangles (a, c, b) puis (b, c, d), avec a = 1. */
    EXPECT_EQ(idx[6], (uint32_t)1);
    EXPECT_EQ(idx[7], size + 1U);
    EXPECT_EQ(idx[8], (uint32_t)2);
    EXPECT_EQ(idx[9], (uint32_t)2);
    EXPECT_EQ(idx[10], size + 1U);
    EXPECT_EQ(idx[11], size + 2U);
    close_fixture(&f);
}

UTEST(eng_terrain, positions_and_uvs_follow_the_grid_the_spacing_and_the_origin)
{
    Fixture f;
    EngTerrainParams params = {.chunk_cells = 32, .origin_x = -50.0F, .origin_z = 20.0F};
    ASSERT_TRUE(open_fixture(&f, 65, 1.5F, fill_plane, &params));
    const EngModelVertex *v = f.terrain.model.vertices;
    for (uint32_t z = 0; z < 65; z += 7) {
        for (uint32_t x = 0; x < 65; x += 5) {
            const EngModelVertex *p = &v[(z * 65U) + x];
            EXPECT_NEAR(p->position[0], -50.0F + ((float)x * 1.5F), 1e-4F);
            EXPECT_NEAR(p->position[2], 20.0F + ((float)z * 1.5F), 1e-4F);
            EXPECT_NEAR(p->position[1], (float)f.samples[(z * 65U) + x], 1e-2F);
            EXPECT_NEAR(p->uv[0], (float)x / 64.0F, 1e-6F);
            EXPECT_NEAR(p->uv[1], (float)z / 64.0F, 1e-6F);
        }
    }
    EXPECT_EQ(v[0].uv[0], 0.0F);
    EXPECT_EQ(v[0].uv[1], 0.0F);
    EXPECT_NEAR(v[(65U * 65U) - 1U].uv[0], 1.0F, 1e-6F);
    EXPECT_NEAR(v[(65U * 65U) - 1U].uv[1], 1.0F, 1e-6F);
    close_fixture(&f);
}

UTEST(eng_terrain, the_altitude_range_maps_the_16_bit_values_to_metres)
{
    static uint16_t samples[9] = {0, 65535, 32768, 0, 0, 0, 0, 0, 0};
    EngHeightmap map = {.size = 3,
                        .spacing = 1.0F,
                        .altitude_min = -10.0F,
                        .altitude_max = 90.0F,
                        .samples = samples};
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test-terrain", (size_t)1 << 20));
    EngTerrain t;
    ASSERT_TRUE(eng_terrain_build(&arena, &map, &(EngTerrainParams){.chunk_cells = 2}, &t));
    EXPECT_NEAR(t.model.vertices[0].position[1], -10.0F, 1e-4F);
    EXPECT_NEAR(t.model.vertices[1].position[1], 90.0F, 1e-3F);
    EXPECT_NEAR(t.model.vertices[2].position[1], -10.0F + (32768.0F / 65535.0F * 100.0F), 1e-3F);
    EXPECT_NEAR(t.chunks[0].bounds_min[1], -10.0F, 1e-4F);
    EXPECT_NEAR(t.chunks[0].bounds_max[1], 90.0F, 1e-3F);
    eng_arena_shutdown(&arena);
}

UTEST(eng_terrain, normals_of_a_slanted_plane_are_exact_everywhere_including_borders)
{
    Fixture f;
    ASSERT_TRUE(open_fixture(&f, 129, 1.0F, fill_plane, &(EngTerrainParams){.chunk_cells = 32}));
    /* h = 1000 + 3x - 2z : dh/dx = 3, dh/dz = -2 ; normale (-3, 1, 2) / sqrt(14). */
    const float n = sqrtf(14.0F);
    for (uint32_t i = 0; i < 129U * 129U; i++) {
        const float *nv = f.terrain.model.vertices[i].normal;
        ASSERT_NEAR(nv[0], -3.0F / n, 1e-5F);
        ASSERT_NEAR(nv[1], 1.0F / n, 1e-5F);
        ASSERT_NEAR(nv[2], 2.0F / n, 1e-5F);
    }
    close_fixture(&f);
}

/* Paraboloïde h = x^2 + z^2 : la différence centrée est EXACTE (2x). Aux colonnes et lignes qui
 * sont des bords de chunk (32, 64, 96), un calcul fait chunk par chunk (différence unilatérale au
 * bord du chunk) donnerait 2x - 1 : la normale y est celle de la grille globale. */
UTEST(eng_terrain, normals_are_computed_on_the_global_grid_so_chunk_borders_are_seamless)
{
    Fixture f;
    ASSERT_TRUE(
        open_fixture(&f, 129, 1.0F, fill_paraboloid, &(EngTerrainParams){.chunk_cells = 32}));
    const uint32_t size = 129;
    for (uint32_t z = 0; z < size; z++) {
        for (uint32_t x = 0; x < size; x++) {
            /* différences centrées à l'intérieur, unilatérales (pas 1) sur le bord de la grille */
            const float dhdx =
                x == 0U ? 1.0F : (x == size - 1U ? (float)((2U * x) - 1U) : 2.0F * (float)x);
            const float dhdz =
                z == 0U ? 1.0F : (z == size - 1U ? (float)((2U * z) - 1U) : 2.0F * (float)z);
            const float inv = 1.0F / sqrtf((dhdx * dhdx) + 1.0F + (dhdz * dhdz));
            const float *nv = f.terrain.model.vertices[(z * size) + x].normal;
            ASSERT_NEAR(nv[0], -dhdx * inv, 2e-5F);
            ASSERT_NEAR(nv[1], inv, 2e-5F);
            ASSERT_NEAR(nv[2], -dhdz * inv, 2e-5F);
            ASSERT_NEAR(len3(nv), 1.0F, 1e-5F);
        }
    }
    close_fixture(&f);
}

UTEST(eng_terrain, the_spacing_scales_the_slopes_in_the_normals)
{
    Fixture f;
    ASSERT_TRUE(open_fixture(&f, 65, 4.0F, fill_plane, &(EngTerrainParams){.chunk_cells = 32}));
    /* dh/dx = 3 par échantillon = 0,75 par mètre à 4 m d'espacement ; dh/dz = -0,5. */
    const float inv = 1.0F / sqrtf((0.75F * 0.75F) + 1.0F + (0.5F * 0.5F));
    const float *nv = f.terrain.model.vertices[(10U * 65U) + 10U].normal;
    EXPECT_NEAR(nv[0], -0.75F * inv, 1e-5F);
    EXPECT_NEAR(nv[1], inv, 1e-5F);
    EXPECT_NEAR(nv[2], 0.5F * inv, 1e-5F);
    close_fixture(&f);
}

UTEST(eng_terrain, tangents_follow_the_u_direction_and_are_orthogonal_to_the_normal)
{
    Fixture f;
    ASSERT_TRUE(
        open_fixture(&f, 65, 1.0F, fill_paraboloid, &(EngTerrainParams){.chunk_cells = 32}));
    const EngModel *m = &f.terrain.model;
    for (uint32_t i = 0; i < m->vertex_count; i++) {
        const EngModelVertex *v = &m->vertices[i];
        ASSERT_NEAR(len3(v->tangent), 1.0F, 1e-5F);
        const float dot = (v->tangent[0] * v->normal[0]) + (v->tangent[1] * v->normal[1]) +
                          (v->tangent[2] * v->normal[2]);
        ASSERT_NEAR(dot, 0.0F, 2e-5F);
        ASSERT_EQ(v->tangent[3], 1.0F);
    }
    /* Sens : d(position)/du, déduit des UV de deux sommets voisins en x, est du côté de la tangente
     * ; la bitangente cross(n, t) * w va vers les v DÉCROISSANTS (haut de l'image) : donc vers -z.
     */
    const EngModelVertex *a = &m->vertices[(20U * 65U) + 20U];
    const EngModelVertex *b = &m->vertices[(20U * 65U) + 21U];
    const float dp[3] = {b->position[0] - a->position[0], b->position[1] - a->position[1],
                         b->position[2] - a->position[2]};
    EXPECT_GT((dp[0] * a->tangent[0]) + (dp[1] * a->tangent[1]) + (dp[2] * a->tangent[2]), 0.0F);
    EXPECT_GT(b->uv[0], a->uv[0]);
    const float *n = a->normal;
    const float *t = a->tangent;
    const float bitangent_z = (n[0] * t[1]) - (n[1] * t[0]);
    EXPECT_LT(bitangent_z, 0.0F);
    close_fixture(&f);
}

UTEST(eng_terrain, neighbouring_chunks_share_exactly_the_vertices_of_their_common_border)
{
    Fixture f;
    ASSERT_TRUE(
        open_fixture(&f, 129, 1.0F, fill_paraboloid, &(EngTerrainParams){.chunk_cells = 32}));
    const EngModel *m = &f.terrain.model;
    const uint32_t size = 129;
    /* Pour chaque paire de chunks voisins en x : les sommets de la colonne x = 32 * (cx + 1)
     * référencés par le chunk de gauche sont les mêmes (mêmes indices) que ceux du chunk de droite.
     */
    for (uint32_t cz = 0; cz < 4; cz++) {
        for (uint32_t cx = 0; cx < 3; cx++) {
            const uint32_t border_x = 32U * (cx + 1U);
            bool left[129] = {false};
            bool right[129] = {false};
            for (int side = 0; side < 2; side++) {
                const uint32_t part = (cz * 4U) + cx + (uint32_t)side;
                for (uint32_t t = 0; t < m->parts[part].index_count; t++) {
                    const uint32_t idx = m->indices[m->parts[part].first_index + t];
                    if (idx % size == border_x) {
                        (side == 0 ? left : right)[idx / size] = true;
                    }
                }
            }
            for (uint32_t z = cz * 32U; z <= (cz + 1U) * 32U; z++) {
                ASSERT_TRUE(left[z]);
                ASSERT_TRUE(right[z]);
            }
        }
    }
    close_fixture(&f);
}

UTEST(eng_terrain, chunk_boxes_contain_exactly_the_vertices_their_triangles_use)
{
    Fixture f;
    ASSERT_TRUE(
        open_fixture(&f, 129, 0.5F, fill_paraboloid,
                     &(EngTerrainParams){.chunk_cells = 32, .origin_x = 3.0F, .origin_z = -7.0F}));
    const EngModel *m = &f.terrain.model;
    float all_min[3] = {INFINITY, INFINITY, INFINITY};
    float all_max[3] = {-INFINITY, -INFINITY, -INFINITY};
    for (uint32_t part = 0; part < m->part_count; part++) {
        float lo[3] = {INFINITY, INFINITY, INFINITY};
        float hi[3] = {-INFINITY, -INFINITY, -INFINITY};
        for (uint32_t t = 0; t < m->parts[part].index_count; t++) {
            const float *p = m->vertices[m->indices[m->parts[part].first_index + t]].position;
            for (int c = 0; c < 3; c++) {
                lo[c] = fminf(lo[c], p[c]);
                hi[c] = fmaxf(hi[c], p[c]);
            }
        }
        for (int c = 0; c < 3; c++) {
            ASSERT_NEAR(f.terrain.chunks[part].bounds_min[c], lo[c], 1e-4F);
            ASSERT_NEAR(f.terrain.chunks[part].bounds_max[c], hi[c], 1e-4F);
            all_min[c] = fminf(all_min[c], lo[c]);
            all_max[c] = fmaxf(all_max[c], hi[c]);
        }
    }
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(m->bounds_min[c], all_min[c], 1e-4F);
        EXPECT_NEAR(m->bounds_max[c], all_max[c], 1e-4F);
    }
    EXPECT_NEAR(m->bounds_min[0], 3.0F, 1e-5F);
    EXPECT_NEAR(m->bounds_max[0], 3.0F + 64.0F, 1e-4F);
    EXPECT_NEAR(m->bounds_min[2], -7.0F, 1e-5F);
    EXPECT_NEAR(m->bounds_max[2], -7.0F + 64.0F, 1e-4F);
    close_fixture(&f);
}

UTEST(eng_terrain, a_full_arena_fails_cleanly_and_leaves_it_untouched)
{
    static uint16_t samples[65 * 65];
    fill_plane(samples, 65);
    const EngHeightmap map = make_map(samples, 65, 1.0F);
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test-terrain", (size_t)64 << 10)); /* trop petit */
    const size_t used = arena.used;
    EngTerrain t = {.chunks_per_side = 9};
    EXPECT_FALSE(eng_terrain_build(&arena, &map, &(EngTerrainParams){.chunk_cells = 32}, &t));
    EXPECT_EQ(arena.used, used);
    EXPECT_EQ(t.chunks_per_side, (uint32_t)0);
    EXPECT_EQ(t.model.vertex_count, (uint32_t)0);
    EXPECT_EQ((const void *)t.model.vertices, (const void *)NULL);
    EXPECT_FALSE(eng_terrain_build(&arena, &map, &(EngTerrainParams){.chunk_cells = 7}, &t));
    EXPECT_EQ(arena.used, used);
    eng_arena_shutdown(&arena);
}

/* Critère d'acceptation : 1 km² sans couture. 1025 x 1025 échantillons à 1 m, 16 x 16 chunks de 64
 * m. */
UTEST(eng_terrain, a_square_kilometre_is_built_from_256_chunks_with_no_gap_and_no_overlap)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test-terrain-km", (size_t)160 << 20));
    EngHeightmapParams params = eng_heightmap_default_params();
    EngHeightmap map;
    ASSERT_TRUE(eng_heightmap_generate(&arena, &params, &map));
    EngTerrain t;
    ASSERT_TRUE(eng_terrain_build(&arena, &map, &(EngTerrainParams){0}, &t));
    EXPECT_EQ(t.chunks_per_side, (uint32_t)16);
    EXPECT_EQ(t.model.part_count, (uint32_t)256);
    EXPECT_EQ(t.model.vertex_count, (uint32_t)1025 * 1025);
    EXPECT_EQ(t.model.index_count, (uint32_t)1024 * 1024 * 6);
    EXPECT_NEAR(t.model.bounds_max[0] - t.model.bounds_min[0], 1024.0F, 1e-3F);
    EXPECT_NEAR(t.model.bounds_max[2] - t.model.bounds_min[2], 1024.0F, 1e-3F);
    EXPECT_NEAR(t.model.bounds_min[1], 0.0F, 1e-3F);
    EXPECT_NEAR(t.model.bounds_max[1], 120.0F, 1e-2F);
    /* Les chunks se touchent bord à bord : l'aire de leurs boîtes en (x, z) vaut 1024 x 1024 et la
     * boîte de chaque chunk fait 64 m de côté. */
    double area = 0.0;
    for (uint32_t i = 0; i < t.model.part_count; i++) {
        const float w = t.chunks[i].bounds_max[0] - t.chunks[i].bounds_min[0];
        const float d = t.chunks[i].bounds_max[2] - t.chunks[i].bounds_min[2];
        ASSERT_NEAR(w, 64.0F, 1e-3F);
        ASSERT_NEAR(d, 64.0F, 1e-3F);
        area += (double)w * (double)d;
    }
    EXPECT_NEAR(area, 1024.0 * 1024.0, 1.0);
    static uint8_t coverage[(size_t)1024 * 1024];
    memset(coverage, 0, sizeof coverage);
    for (uint32_t i = 0; i < t.model.index_count; i += 3) {
        const uint32_t a = t.model.indices[i];
        const uint32_t b = t.model.indices[i + 1U];
        const uint32_t c = t.model.indices[i + 2U];
        const uint32_t x = min3(a % 1025U, b % 1025U, c % 1025U);
        const uint32_t z = min3(a / 1025U, b / 1025U, c / 1025U);
        coverage[((size_t)z * 1024U) + x]++;
    }
    bool every_cell_twice = true;
    for (size_t i = 0; i < (size_t)1024 * 1024; i++) {
        every_cell_twice = every_cell_twice && coverage[i] == 2U;
    }
    EXPECT_TRUE(every_cell_twice);
    eng_arena_shutdown(&arena);
}
