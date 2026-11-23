#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/rock.h"
#include "utest.h"

static bool build(EngArena *arena, const EngRockParams *p, EngModel *m)
{
    return eng_arena_init(arena, "rock-test", (size_t)8 << 20) && eng_rock_build(arena, p, m);
}

UTEST(eng_rock, counts_follow_the_icosphere_formulas)
{
    for (uint32_t n = 0; n <= 4; n++) {
        EngRockParams p = eng_rock_default_params();
        p.subdivisions = n;
        EngArena arena;
        EngModel m;
        ASSERT_TRUE(build(&arena, &p, &m));
        EXPECT_EQ(m.vertex_count, (10U * (1U << (2U * n))) + 2U);
        EXPECT_EQ(m.vertex_count, eng_rock_vertex_count(n));
        EXPECT_EQ(m.index_count, 3U * 20U * (1U << (2U * n)));
        EXPECT_EQ(m.index_count / 3U, eng_rock_triangle_count(n));
        EXPECT_EQ(m.part_count, 1U);
        EXPECT_EQ(m.parts[0].index_count, m.index_count);
        for (uint32_t i = 0; i < m.index_count; i++) {
            ASSERT_LT(m.indices[i], m.vertex_count);
        }
        eng_arena_shutdown(&arena);
    }
}

/* Variété fermée : chaque arête orientée a son opposée une fois, aucune n'est répétée (donc chaque
 * arête non orientée sert exactement deux triangles, orientés de façon cohérente) ; Euler : V - E +
 * F = 2. */
UTEST(eng_rock, the_mesh_is_a_closed_consistently_oriented_surface)
{
    EngRockParams p = eng_rock_default_params();
    p.subdivisions = 3;
    EngArena arena;
    EngModel m;
    ASSERT_TRUE(build(&arena, &p, &m));
    const uint32_t triangles = m.index_count / 3U;
    uint32_t edges = 0;
    for (uint32_t f = 0; f < triangles; f++) {
        for (uint32_t k = 0; k < 3; k++) {
            const uint32_t a = m.indices[(f * 3U) + k];
            const uint32_t b = m.indices[(f * 3U) + ((k + 1U) % 3U)];
            uint32_t same = 0;
            uint32_t opposite = 0;
            for (uint32_t g = 0; g < triangles; g++) {
                for (uint32_t j = 0; j < 3; j++) {
                    const uint32_t c = m.indices[(g * 3U) + j];
                    const uint32_t d = m.indices[(g * 3U) + ((j + 1U) % 3U)];
                    same += (a == c && b == d) ? 1U : 0U;
                    opposite += (a == d && b == c) ? 1U : 0U;
                }
            }
            EXPECT_EQ(same, 1U);
            EXPECT_EQ(opposite, 1U);
            edges += (a < b) ? 1U : 0U;
        }
    }
    EXPECT_EQ((int)m.vertex_count - (int)edges + (int)triangles, 2);
    eng_arena_shutdown(&arena);
}

UTEST(eng_rock, triangles_face_outwards_and_normals_are_unit_and_outward)
{
    EngRockParams p = eng_rock_default_params();
    p.subdivisions = 3;
    p.roughness = 0.4F;
    EngArena arena;
    EngModel m;
    ASSERT_TRUE(build(&arena, &p, &m));
    for (uint32_t f = 0; f < m.index_count / 3U; f++) {
        const float *a = m.vertices[m.indices[(size_t)f * 3U]].position;
        const float *b = m.vertices[m.indices[(f * 3U) + 1U]].position;
        const float *c = m.vertices[m.indices[(f * 3U) + 2U]].position;
        const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float n[3] = {(e1[1] * e2[2]) - (e1[2] * e2[1]), (e1[2] * e2[0]) - (e1[0] * e2[2]),
                            (e1[0] * e2[1]) - (e1[1] * e2[0])};
        const float centroid[3] = {a[0] + b[0] + c[0], a[1] + b[1] + c[1], a[2] + b[2] + c[2]};
        EXPECT_GT((n[0] * centroid[0]) + (n[1] * centroid[1]) + (n[2] * centroid[2]), 0.0F);
    }
    for (uint32_t i = 0; i < m.vertex_count; i++) {
        const EngModelVertex *v = &m.vertices[i];
        const float nl = sqrtf((v->normal[0] * v->normal[0]) + (v->normal[1] * v->normal[1]) +
                               (v->normal[2] * v->normal[2]));
        EXPECT_NEAR(nl, 1.0F, 1e-4F);
        EXPECT_GT((v->normal[0] * v->position[0]) + (v->normal[1] * v->position[1]) +
                      (v->normal[2] * v->position[2]),
                  0.0F);
        const float dot_nt = (v->normal[0] * v->tangent[0]) + (v->normal[1] * v->tangent[1]) +
                             (v->normal[2] * v->tangent[2]);
        EXPECT_NEAR(dot_nt, 0.0F, 1e-4F);
        EXPECT_EQ(v->tangent[3], 1.0F);
    }
    eng_arena_shutdown(&arena);
}

UTEST(eng_rock, roughness_zero_and_no_flattening_give_a_sphere)
{
    EngRockParams p = eng_rock_default_params();
    p.roughness = 0.0F;
    p.flatness = 1.0F;
    p.radius = 2.5F;
    p.subdivisions = 3;
    EngArena arena;
    EngModel m;
    ASSERT_TRUE(build(&arena, &p, &m));
    for (uint32_t i = 0; i < m.vertex_count; i++) {
        const float *q = m.vertices[i].position;
        EXPECT_NEAR(sqrtf((q[0] * q[0]) + (q[1] * q[1]) + (q[2] * q[2])), 2.5F, 1e-4F);
    }
    /* volume (théorème de la divergence) proche de celui de la sphère circonscrite */
    double volume = 0.0;
    for (uint32_t f = 0; f < m.index_count / 3U; f++) {
        const float *a = m.vertices[m.indices[(size_t)f * 3U]].position;
        const float *b = m.vertices[m.indices[(f * 3U) + 1U]].position;
        const float *c = m.vertices[m.indices[(f * 3U) + 2U]].position;
        volume += ((double)a[0] * (((double)b[1] * (double)c[2]) - ((double)b[2] * (double)c[1])) +
                   (double)a[1] * (((double)b[2] * (double)c[0]) - ((double)b[0] * (double)c[2])) +
                   (double)a[2] * (((double)b[0] * (double)c[1]) - ((double)b[1] * (double)c[0]))) /
                  6.0;
    }
    const double sphere = 4.0 / 3.0 * 3.14159265358979 * 2.5 * 2.5 * 2.5;
    EXPECT_GT(volume, 0.95 * sphere);
    EXPECT_LT(volume, sphere);
    eng_arena_shutdown(&arena);
}

UTEST(eng_rock, bounds_follow_the_radius_roughness_and_flattening)
{
    EngRockParams p = eng_rock_default_params();
    p.subdivisions = 3;
    p.radius = 2.0F;
    p.roughness = 0.3F;
    p.flatness = 0.5F;
    EngArena arena;
    EngModel m;
    ASSERT_TRUE(build(&arena, &p, &m));
    const float r_max = 2.0F * 1.3F;
    const float r_min = 2.0F * 0.7F;
    for (int c = 0; c < 3; c++) {
        const float limit = c == 1 ? r_max * 0.5F : r_max;
        EXPECT_LE(m.bounds_max[c], limit + 1e-4F);
        EXPECT_GE(m.bounds_min[c], -limit - 1e-4F);
    }
    /* les boîtes déclarées sont celles des sommets */
    float lo[3] = {1e9F, 1e9F, 1e9F};
    float hi[3] = {-1e9F, -1e9F, -1e9F};
    for (uint32_t i = 0; i < m.vertex_count; i++) {
        for (int c = 0; c < 3; c++) {
            lo[c] = fminf(lo[c], m.vertices[i].position[c]);
            hi[c] = fmaxf(hi[c], m.vertices[i].position[c]);
        }
    }
    for (int c = 0; c < 3; c++) {
        EXPECT_EQ(m.bounds_min[c], lo[c]);
        EXPECT_EQ(m.bounds_max[c], hi[c]);
    }
    /* plus plat que large : l'étendue verticale est nettement plus petite que l'horizontale */
    EXPECT_LT(m.bounds_max[1] - m.bounds_min[1], 0.8F * (m.bounds_max[0] - m.bounds_min[0]));
    /* chaque sommet respecte le rayon [r_min, r_max] avant aplatissement vertical */
    for (uint32_t i = 0; i < m.vertex_count; i++) {
        const float *q = m.vertices[i].position;
        const float r = sqrtf((q[0] * q[0]) + ((q[1] / 0.5F) * (q[1] / 0.5F)) + (q[2] * q[2]));
        EXPECT_GE(r, r_min - 1e-3F);
        EXPECT_LE(r, r_max + 1e-3F);
    }
    eng_arena_shutdown(&arena);
}

UTEST(eng_rock, same_seed_same_bytes_and_other_seeds_differ_and_roughness_matters)
{
    EngRockParams p = eng_rock_default_params();
    EngArena a1;
    EngArena a2;
    EngArena a3;
    EngModel m1;
    EngModel m2;
    EngModel m3;
    ASSERT_TRUE(build(&a1, &p, &m1));
    ASSERT_TRUE(build(&a2, &p, &m2));
    ASSERT_EQ(m1.vertex_count, m2.vertex_count);
    EXPECT_EQ(memcmp(m1.vertices, m2.vertices, (size_t)m1.vertex_count * sizeof(EngModelVertex)),
              0);
    EXPECT_EQ(memcmp(m1.indices, m2.indices, (size_t)m1.index_count * sizeof(uint32_t)), 0);
    p.seed = 2;
    ASSERT_TRUE(build(&a3, &p, &m3));
    EXPECT_NE(memcmp(m1.vertices, m3.vertices, (size_t)m1.vertex_count * sizeof(EngModelVertex)),
              0);
    /* de nombreux sommets sont déplacés différemment */
    uint32_t different = 0;
    for (uint32_t i = 0; i < m1.vertex_count; i++) {
        different +=
            fabsf(m1.vertices[i].position[0] - m3.vertices[i].position[0]) > 1e-3F ? 1U : 0U;
    }
    EXPECT_GT(different, m1.vertex_count / 2U);
    eng_arena_shutdown(&a1);
    eng_arena_shutdown(&a2);
    eng_arena_shutdown(&a3);
}

UTEST(eng_rock, the_material_is_used_and_defaults_are_rough_and_non_metallic)
{
    EngRockParams p = eng_rock_default_params();
    EngArena arena;
    EngModel m;
    ASSERT_TRUE(build(&arena, &p, &m));
    EXPECT_EQ(m.material_count, 2U);
    EXPECT_EQ(m.parts[0].material, 1U);
    EXPECT_EQ(m.materials[1].metallic, 0.0F);
    EXPECT_GE(m.materials[1].roughness, 0.8F);
    EXPECT_EQ(m.materials[1].image, -1);
    eng_arena_shutdown(&arena);
    EngModelMaterial custom = eng_rock_default_material();
    custom.base_color[0] = 0.9F;
    p.material = &custom;
    ASSERT_TRUE(build(&arena, &p, &m));
    EXPECT_EQ(m.materials[1].base_color[0], 0.9F);
    eng_arena_shutdown(&arena);
}

UTEST(eng_rock, invalid_parameters_are_rejected_and_a_small_arena_is_restored)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "rock-bad", (size_t)1 << 20));
    EngModel m;
    EngRockParams p = eng_rock_default_params();
    EXPECT_TRUE(eng_rock_params_valid(&p));
    EXPECT_FALSE(eng_rock_params_valid(NULL));
    const size_t before = eng_arena_remaining(&arena);
    p.subdivisions = ENG_ROCK_MAX_SUBDIVISIONS + 1U;
    EXPECT_FALSE(eng_rock_build(&arena, &p, &m));
    EXPECT_EQ(m.vertex_count, 0U);
    p = eng_rock_default_params();
    p.radius = 0.0F;
    EXPECT_FALSE(eng_rock_params_valid(&p));
    p = eng_rock_default_params();
    p.radius = NAN;
    EXPECT_FALSE(eng_rock_params_valid(&p));
    p = eng_rock_default_params();
    p.roughness = 1.0F;
    EXPECT_FALSE(eng_rock_params_valid(&p));
    p.roughness = -0.1F;
    EXPECT_FALSE(eng_rock_params_valid(&p));
    p = eng_rock_default_params();
    p.flatness = 0.0F;
    EXPECT_FALSE(eng_rock_params_valid(&p));
    p.flatness = 1.5F;
    EXPECT_FALSE(eng_rock_params_valid(&p));
    p = eng_rock_default_params();
    p.roughness = 0.0F; /* bornes incluses */
    p.flatness = 1.0F;
    p.subdivisions = ENG_ROCK_MAX_SUBDIVISIONS;
    EXPECT_TRUE(eng_rock_params_valid(&p));
    /* arène de 100 Ko : la sous-division 5 (10 242 sommets) n'y tient pas ; elle est restituée */
    EngArena small;
    ASSERT_TRUE(eng_arena_init(&small, "rock-small", (size_t)100 << 10));
    const size_t small_before = eng_arena_remaining(&small);
    EXPECT_FALSE(eng_rock_build(&small, &p, &m));
    EXPECT_EQ(m.vertex_count, 0U);
    EXPECT_EQ(eng_arena_remaining(&small), small_before);
    eng_arena_shutdown(&small);
    EXPECT_EQ(eng_arena_remaining(&arena), before);
    eng_arena_shutdown(&arena);
}

/* Valeurs de référence (graine 1, paramètres par défaut) : le bruit et l'icosphère ne changent pas
 * sans qu'on le voie (les rochers d'une sauvegarde ou d'un niveau seraient différents). */
UTEST(eng_rock, the_default_rock_has_pinned_vertex_positions)
{
    EngRockParams p = eng_rock_default_params();
    EngArena arena;
    EngModel m;
    ASSERT_TRUE(build(&arena, &p, &m));
    const struct {
        uint32_t id;
        float x;
        float y;
        float z;
    } pins[] = {{0, -0.5020765F, 0.5686638F, 0.0F},
                {5, 0.0F, 0.3660744F, 0.8461727F},
                {17, 0.2820353F, 0.5168647F, -0.4563428F},
                {100, 0.0F, 0.1815000F, -0.9127240F},
                {161, 0.8901346F, 0.3850934F, 0.0F}};
    for (int i = 0; i < 5; i++) {
        const float *q = m.vertices[pins[i].id].position;
        EXPECT_NEAR(q[0], pins[i].x, 2e-6F);
        EXPECT_NEAR(q[1], pins[i].y, 2e-6F);
        EXPECT_NEAR(q[2], pins[i].z, 2e-6F);
    }
    eng_arena_shutdown(&arena);
}

/* Le bruit est lisse (deux sommets voisins ont des rayons proches) et remplit la bande de rugosité
 * (il ne reste pas collé à une valeur). */
UTEST(eng_rock, the_displacement_is_smooth_and_uses_the_roughness_band)
{
    EngRockParams p = eng_rock_default_params();
    p.subdivisions = 4;
    p.roughness = 0.3F;
    p.flatness = 1.0F;
    EngArena arena;
    EngModel m;
    ASSERT_TRUE(build(&arena, &p, &m));
    float r_min = 1e9F;
    float r_max = -1e9F;
    float step_max = 0.0F;
    for (uint32_t i = 0; i < m.vertex_count; i++) {
        const float *q = m.vertices[i].position;
        const float r = sqrtf((q[0] * q[0]) + (q[1] * q[1]) + (q[2] * q[2]));
        r_min = fminf(r_min, r);
        r_max = fmaxf(r_max, r);
    }
    for (uint32_t f = 0; f < m.index_count / 3U; f++) {
        for (uint32_t k = 0; k < 3; k++) {
            const float *a = m.vertices[m.indices[(f * 3U) + k]].position;
            const float *b = m.vertices[m.indices[(f * 3U) + ((k + 1U) % 3U)]].position;
            const float ra = sqrtf((a[0] * a[0]) + (a[1] * a[1]) + (a[2] * a[2]));
            const float rb = sqrtf((b[0] * b[0]) + (b[1] * b[1]) + (b[2] * b[2]));
            step_max = fmaxf(step_max, fabsf(ra - rb));
        }
    }
    EXPECT_LT(r_min, 1.0F - (0.3F * 0.4F)); /* atteint le bas de la bande... */
    EXPECT_GT(r_max, 1.0F + (0.3F * 0.4F)); /* ...et le haut */
    EXPECT_LT(step_max, 0.18F); /* mais sans saut entre voisins (pente réelle : 0,12 au plus) */
    eng_arena_shutdown(&arena);
}

/* Sphère : UV sphériques et tangente unitaire. */
UTEST(eng_rock, uv_follow_the_spherical_mapping_and_tangents_are_unit)
{
    EngRockParams p = eng_rock_default_params();
    p.roughness = 0.0F;
    p.flatness = 1.0F;
    p.subdivisions = 2;
    EngArena arena;
    EngModel m;
    ASSERT_TRUE(build(&arena, &p, &m));
    for (uint32_t i = 0; i < m.vertex_count; i++) {
        const EngModelVertex *v = &m.vertices[i];
        EXPECT_NEAR(v->uv[0], (atan2f(v->position[2], v->position[0]) / 6.2831853F) + 0.5F, 1e-5F);
        EXPECT_NEAR(v->uv[1], acosf(fmaxf(-1.0F, fminf(1.0F, v->position[1]))) / 3.14159265F,
                    1e-5F);
        EXPECT_GE(v->uv[0], 0.0F);
        EXPECT_LE(v->uv[0], 1.0F);
        EXPECT_GE(v->uv[1], 0.0F);
        EXPECT_LE(v->uv[1], 1.0F);
        const float tl = sqrtf((v->tangent[0] * v->tangent[0]) + (v->tangent[1] * v->tangent[1]) +
                               (v->tangent[2] * v->tangent[2]));
        EXPECT_NEAR(tl, 1.0F, 1e-4F);
        /* à rugosité 0 la normale est la position (sphère unité) */
        for (int c = 0; c < 3; c++) {
            EXPECT_NEAR(v->normal[c], v->position[c], 0.03F);
        }
    }
    eng_arena_shutdown(&arena);
}

static int compare_edges(const void *a, const void *b)
{
    const uint64_t x = *(const uint64_t *)a;
    const uint64_t y = *(const uint64_t *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

/* Sous-division maximale : la table des arêtes grandit sans collision fausse ; chaque arête non
 * orientée sert exactement deux triangles et V - E + F = 2. */
UTEST(eng_rock, the_finest_subdivision_is_still_a_closed_surface)
{
    EngRockParams p = eng_rock_default_params();
    p.subdivisions = ENG_ROCK_MAX_SUBDIVISIONS;
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "rock-fine", (size_t)16 << 20));
    EngModel m;
    ASSERT_TRUE(eng_rock_build(&arena, &p, &m));
    EXPECT_EQ(m.vertex_count, 10242U);
    EXPECT_EQ(m.index_count, 3U * 20480U);
    const size_t count = m.index_count;
    uint64_t *edges = malloc(count * sizeof(uint64_t));
    ASSERT_TRUE(edges != NULL);
    for (uint32_t f = 0; f < m.index_count / 3U; f++) {
        for (uint32_t k = 0; k < 3; k++) {
            const uint64_t a = m.indices[(f * 3U) + k];
            const uint64_t b = m.indices[(f * 3U) + ((k + 1U) % 3U)];
            edges[((size_t)f * 3U) + k] = a < b ? (a << 32U) | b : (b << 32U) | a;
        }
    }
    qsort(edges, count, sizeof(uint64_t), compare_edges);
    size_t unique = 0;
    bool every_edge_twice = true;
    for (size_t i = 0; i < count;) {
        size_t j = i;
        while (j < count && edges[j] == edges[i]) {
            j++;
        }
        every_edge_twice = every_edge_twice && (j - i) == 2U;
        unique++;
        i = j;
    }
    free(edges);
    EXPECT_TRUE(every_edge_twice);
    EXPECT_EQ((int)m.vertex_count - (int)unique + (int)(m.index_count / 3U), 2);
    eng_arena_shutdown(&arena);
}
