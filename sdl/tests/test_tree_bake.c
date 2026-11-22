/* Conversion d'un arbre scanné en arbre de jeu (engine/tree_bake.h) : grappes, repères,
 * photographie des triangles, prolongement des bords, rangement dans l'atlas, ombre du feuillage.
 * L'allègement des maillages est dans test_tree_bake_simplify.c. */
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "engine/tree_bake.h"
#include "utest.h"

/* ── Grappes ── */

/* deux nuages de `n` points autour de (0, 0, 0) et de (10, 0, 0) */
static void two_blobs(float *points, uint32_t n)
{
    for (uint32_t i = 0; i < 2U * n; i++) {
        const float jitter = (float)(i % 7U) * 0.05F;
        points[((size_t)i * 3U) + 0U] = (i < n ? 0.0F : 10.0F) + jitter;
        points[((size_t)i * 3U) + 1U] = jitter * 0.5F;
        points[((size_t)i * 3U) + 2U] = -jitter;
    }
}

UTEST(eng_bake_cluster, two_distant_groups_become_two_clusters)
{
    enum { N = 40 };
    float points[2 * N * 3];
    uint32_t assignment[2 * N];
    float centers[2 * 3];
    two_blobs(points, N);
    ASSERT_EQ(2U, eng_bake_cluster(points, NULL, 2U * N, 2U, 7U, assignment, centers));
    for (uint32_t i = 1; i < N; i++) {
        ASSERT_EQ(assignment[0], assignment[i]);
        ASSERT_EQ(assignment[N], assignment[N + i]);
    }
    ASSERT_NE(assignment[0], assignment[N]);
    const float *left = &centers[(size_t)assignment[0] * 3U];
    const float *right = &centers[(size_t)assignment[N] * 3U];
    ASSERT_NEAR(0.15F, left[0], 0.05F); /* barycentre du nuage : 0 + moyenne des écarts */
    ASSERT_NEAR(10.15F, right[0], 0.05F);
}

UTEST(eng_bake_cluster, the_same_inputs_give_the_same_clusters)
{
    enum { N = 30 };
    float points[2 * N * 3];
    uint32_t first[2 * N];
    uint32_t second[2 * N];
    float centers_a[5 * 3];
    float centers_b[5 * 3];
    two_blobs(points, N);
    const uint32_t a = eng_bake_cluster(points, NULL, 2U * N, 5U, 3U, first, centers_a);
    const uint32_t b = eng_bake_cluster(points, NULL, 2U * N, 5U, 3U, second, centers_b);
    ASSERT_EQ(a, b);
    ASSERT_EQ(0, memcmp(first, second, sizeof first));
    ASSERT_EQ(0, memcmp(centers_a, centers_b, (size_t)a * 3U * sizeof(float)));
    ASSERT_GE(a, 2U);
    ASSERT_LE(a, 5U);
    for (uint32_t i = 0; i < 2U * N; i++) {
        ASSERT_LT(first[i], a); /* chaque point a une grappe gardée */
    }
}

UTEST(eng_bake_cluster, the_center_is_the_weighted_mean_of_its_points)
{
    const float points[6] = {0.0F, 0.0F, 0.0F, 4.0F, 8.0F, -4.0F};
    const float weights[2] = {1.0F, 3.0F};
    uint32_t assignment[2];
    float center[3];
    ASSERT_EQ(1U, eng_bake_cluster(points, weights, 2U, 1U, 1U, assignment, center));
    ASSERT_NEAR(3.0F, center[0], 1e-5F);
    ASSERT_NEAR(6.0F, center[1], 1e-5F);
    ASSERT_NEAR(-3.0F, center[2], 1e-5F);
    ASSERT_EQ(1U, eng_bake_cluster(points, NULL, 2U, 1U, 1U, assignment, center));
    ASSERT_NEAR(2.0F, center[0], 1e-5F); /* sans poids : la moyenne simple */
}

UTEST(eng_bake_cluster, empty_clusters_are_removed_and_never_more_clusters_than_points)
{
    const float same[9] = {1.0F, 2.0F, 3.0F, 1.0F, 2.0F, 3.0F, 1.0F, 2.0F, 3.0F};
    uint32_t assignment[3];
    float centers[8 * 3];
    const uint32_t kept = eng_bake_cluster(same, NULL, 3U, 3U, 5U, assignment, centers);
    ASSERT_EQ(1U, kept); /* trois points confondus : une seule grappe a des points */
    ASSERT_EQ(0U, assignment[0]);
    ASSERT_EQ(0U, assignment[2]);
    ASSERT_NEAR(2.0F, centers[1], 1e-6F);
    const float two[6] = {0.0F, 0.0F, 0.0F, 5.0F, 0.0F, 0.0F};
    ASSERT_EQ(2U, eng_bake_cluster(two, NULL, 2U, 8U, 5U, assignment, centers));
}

UTEST(eng_bake_cluster, every_point_ends_nearest_to_the_center_of_its_own_cluster)
{
    /* 60 points sur une ligne, en trois paquets de densités inégales : un partage qui s'arrêterait
     * aux centres de départ laisserait des points plus proches du centre d'une autre grappe */
    enum { N = 60 };
    float points[N * 3];
    for (uint32_t i = 0; i < N; i++) {
        const float along = i < 30U ? (float)i * 0.1F
                                    : (i < 50U ? 10.0F + ((float)(i - 30U) * 0.4F)
                                               : 30.0F + ((float)(i - 50U) * 1.5F));
        points[(size_t)i * 3U] = along;
        points[((size_t)i * 3U) + 1U] = 0.0F;
        points[((size_t)i * 3U) + 2U] = 0.0F;
    }
    uint32_t assignment[N];
    float centers[4 * 3];
    const uint32_t k = eng_bake_cluster(points, NULL, N, 4U, 11U, assignment, centers);
    ASSERT_EQ(4U, k);
    double sum[4] = {0.0, 0.0, 0.0, 0.0};
    uint32_t members[4] = {0U, 0U, 0U, 0U};
    for (uint32_t i = 0; i < N; i++) {
        const float own = fabsf(points[(size_t)i * 3U] - centers[(size_t)assignment[i] * 3U]);
        for (uint32_t c = 0; c < k; c++) {
            ASSERT_LE(own, fabsf(points[(size_t)i * 3U] - centers[(size_t)c * 3U]) + 1e-4F);
        }
        sum[assignment[i]] += (double)points[(size_t)i * 3U];
        members[assignment[i]]++;
    }
    for (uint32_t c = 0; c < k; c++) { /* et chaque centre est le barycentre de ses points */
        ASSERT_GT(members[c], 0U);
        ASSERT_NEAR(sum[c] / (double)members[c], (double)centers[(size_t)c * 3U], 1e-3);
    }
}

UTEST(eng_bake_cluster, invalid_parameters_give_no_cluster)
{
    float points[6] = {0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F};
    uint32_t assignment[2];
    float centers[6];
    ASSERT_EQ(0U, eng_bake_cluster(NULL, NULL, 2U, 2U, 1U, assignment, centers));
    ASSERT_EQ(0U, eng_bake_cluster(points, NULL, 2U, 2U, 1U, NULL, centers));
    ASSERT_EQ(0U, eng_bake_cluster(points, NULL, 2U, 2U, 1U, assignment, NULL));
    ASSERT_EQ(0U, eng_bake_cluster(points, NULL, 0U, 2U, 1U, assignment, centers));
    ASSERT_EQ(0U, eng_bake_cluster(points, NULL, 2U, 0U, 1U, assignment, centers));
    points[4] = NAN;
    ASSERT_EQ(0U, eng_bake_cluster(points, NULL, 2U, 2U, 1U, assignment, centers));
}

/* ── Repère ── */

static float dot3(const float *a, const float *b)
{
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

UTEST(eng_bake_fit, the_axes_follow_the_shape_and_the_box_holds_every_point)
{
    /* une boîte de 8 x 2 x 0,5 tournée : grand axe (0,6 ; 0,8 ; 0), puis (-0,8 ; 0,6 ; 0), puis z
     */
    const float long_axis[3] = {0.6F, 0.8F, 0.0F};
    const float mid_axis[3] = {-0.8F, 0.6F, 0.0F};
    const float center[3] = {5.0F, -2.0F, 1.0F};
    float points[5 * 5 * 3 * 3];
    uint32_t n = 0;
    for (int i = -2; i <= 2; i++) {
        for (int j = -2; j <= 2; j++) {
            for (int k = -1; k <= 1; k++) {
                for (int a = 0; a < 3; a++) {
                    points[((size_t)n * 3U) + (uint32_t)a] =
                        center[a] + ((float)i * 2.0F * long_axis[a]) +
                        ((float)j * 0.5F * mid_axis[a]) + (a == 2 ? (float)k * 0.25F : 0.0F);
                }
                n++;
            }
        }
    }
    EngBakeFrame f;
    ASSERT_TRUE(eng_bake_fit(points, 3U * sizeof(float), n, &f));
    ASSERT_NEAR(1.0F, fabsf(dot3(f.axis[0], long_axis)), 1e-4F);
    ASSERT_NEAR(1.0F, fabsf(dot3(f.axis[1], mid_axis)), 1e-4F);
    ASSERT_NEAR(1.0F, fabsf(f.axis[2][2]), 1e-4F);
    ASSERT_NEAR(4.0F, f.half[0], 1e-4F);
    ASSERT_NEAR(1.0F, f.half[1], 1e-4F);
    ASSERT_NEAR(0.25F, f.half[2], 1e-4F);
    for (int a = 0; a < 3; a++) {
        ASSERT_NEAR(center[a], f.center[a], 1e-4F);
    }
    /* base directe : axe 2 = axe 0 x axe 1 */
    const float cross[3] = {(f.axis[0][1] * f.axis[1][2]) - (f.axis[0][2] * f.axis[1][1]),
                            (f.axis[0][2] * f.axis[1][0]) - (f.axis[0][0] * f.axis[1][2]),
                            (f.axis[0][0] * f.axis[1][1]) - (f.axis[0][1] * f.axis[1][0])};
    ASSERT_NEAR(1.0F, dot3(cross, f.axis[2]), 1e-4F);
    for (uint32_t i = 0; i < n; i++) {
        const float rel[3] = {points[(size_t)i * 3U] - f.center[0],
                              points[((size_t)i * 3U) + 1U] - f.center[1],
                              points[((size_t)i * 3U) + 2U] - f.center[2]};
        for (int a = 0; a < 3; a++) {
            ASSERT_LE(fabsf(dot3(rel, f.axis[a])), f.half[a] + 1e-4F);
        }
    }
}

UTEST(eng_bake_fit, a_shape_turned_in_another_plane_still_gets_a_right_handed_frame)
{
    /* une boîte de 8 x 2 x 0,5 tournée autour de x : grand axe (0 ; 0,6 ; 0,8), puis
     * (0 ; -0,8 ; 0,6), mince le long de x */
    const float long_axis[3] = {0.0F, 0.6F, 0.8F};
    const float mid_axis[3] = {0.0F, -0.8F, 0.6F};
    float points[5 * 5 * 3 * 3];
    uint32_t n = 0;
    for (int i = -2; i <= 2; i++) {
        for (int j = -2; j <= 2; j++) {
            for (int k = -1; k <= 1; k++) {
                for (int a = 0; a < 3; a++) {
                    points[((size_t)n * 3U) + (size_t)a] = ((float)i * 2.0F * long_axis[a]) +
                                                           ((float)j * 0.5F * mid_axis[a]) +
                                                           (a == 0 ? (float)k * 0.25F : 0.0F);
                }
                n++;
            }
        }
    }
    EngBakeFrame f;
    ASSERT_TRUE(eng_bake_fit(points, 3U * sizeof(float), n, &f));
    ASSERT_NEAR(1.0F, fabsf(dot3(f.axis[0], long_axis)), 1e-4F);
    ASSERT_NEAR(1.0F, fabsf(dot3(f.axis[1], mid_axis)), 1e-4F);
    ASSERT_NEAR(1.0F, fabsf(f.axis[2][0]), 1e-4F);
    for (int a = 0; a < 3; a++) { /* trois axes unitaires, deux à deux perpendiculaires */
        ASSERT_NEAR(1.0F, dot3(f.axis[a], f.axis[a]), 1e-4F);
        ASSERT_NEAR(0.0F, dot3(f.axis[a], f.axis[(a + 1) % 3]), 1e-4F);
    }
    const float cross[3] = {(f.axis[0][1] * f.axis[1][2]) - (f.axis[0][2] * f.axis[1][1]),
                            (f.axis[0][2] * f.axis[1][0]) - (f.axis[0][0] * f.axis[1][2]),
                            (f.axis[0][0] * f.axis[1][1]) - (f.axis[0][1] * f.axis[1][0])};
    ASSERT_NEAR(1.0F, dot3(cross, f.axis[2]), 1e-4F); /* base directe */
    ASSERT_NEAR(0.25F, f.half[2], 1e-4F);
}

UTEST(eng_bake_fit, the_center_is_the_middle_of_the_extent_not_the_mean)
{
    /* trois points groupés à gauche, un seul loin à droite : la moyenne est à 2,5 ; le milieu à 5
     */
    const float points[12] = {0.0F, 0.0F, 0.0F, 0.0F,  0.0F, 0.0F,
                              0.0F, 0.0F, 0.0F, 10.0F, 0.0F, 0.0F};
    EngBakeFrame f;
    ASSERT_TRUE(eng_bake_fit(points, 3U * sizeof(float), 4U, &f));
    ASSERT_NEAR(5.0F, f.center[0], 1e-4F);
    ASSERT_NEAR(5.0F, f.half[0], 1e-4F);
    ASSERT_NEAR(1e-4F, f.half[1], 1e-6F); /* une épaisseur nulle reçoit la demi-étendue minimale */
    ASSERT_NEAR(1e-4F, f.half[2], 1e-6F);
}

UTEST(eng_bake_fit, the_stride_skips_the_other_fields_of_a_vertex)
{
    EngBakeVertex vertices[3];
    memset(vertices, 0, sizeof vertices);
    for (int i = 0; i < 3; i++) {
        vertices[i].position[1] = (float)i * 3.0F; /* 0, 3, 6 le long de y */
        vertices[i].normal[0] = 100.0F;            /* ne doit pas être lu comme une position */
        vertices[i].uv[0] = -50.0F;
    }
    EngBakeFrame f;
    ASSERT_TRUE(eng_bake_fit(vertices[0].position, sizeof(EngBakeVertex), 3U, &f));
    ASSERT_NEAR(1.0F, fabsf(f.axis[0][1]), 1e-4F);
    ASSERT_NEAR(3.0F, f.half[0], 1e-4F);
    ASSERT_NEAR(3.0F, f.center[1], 1e-4F);
    ASSERT_NEAR(0.0F, f.center[0], 1e-4F);
}

UTEST(eng_bake_fit, invalid_parameters_are_refused)
{
    float points[6] = {0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F};
    EngBakeFrame f;
    ASSERT_FALSE(eng_bake_fit(NULL, 12U, 2U, &f));
    ASSERT_FALSE(eng_bake_fit(points, 12U, 2U, NULL));
    ASSERT_FALSE(eng_bake_fit(points, 12U, 0U, &f));
    ASSERT_FALSE(eng_bake_fit(points, 8U, 2U, &f)); /* trois flottants ne tiennent pas */
    points[5] = INFINITY;
    ASSERT_FALSE(eng_bake_fit(points, 12U, 2U, &f));
    points[5] = 1.0F;
    ASSERT_TRUE(eng_bake_fit(points, 12U, 1U, &f)); /* un seul point : un repère quand même */
    ASSERT_NEAR(1e-4F, f.half[0], 1e-6F);
}

/* ── Rangement dans l'atlas ── */

static bool overlap(const EngBakeRect *a, const EngBakeRect *b, uint32_t gap)
{
    return a->x < b->x + b->w + gap && b->x < a->x + a->w + gap && a->y < b->y + b->h + gap &&
           b->y < a->y + a->h + gap;
}

UTEST(eng_bake_pack, rectangles_are_placed_inside_without_touching)
{
    EngBakeRect rects[9];
    uint32_t order[9];
    for (uint32_t i = 0; i < 9U; i++) {
        rects[i] = (EngBakeRect){
            .x = 999U, .y = 999U, .w = 10U + ((size_t)i * 3U), .h = 6U + ((i * 5U) % 11U)};
    }
    const uint32_t gap = 3U;
    ASSERT_TRUE(eng_bake_pack(rects, 9U, 96U, 64U, gap, order));
    for (uint32_t i = 0; i < 9U; i++) {
        ASSERT_GE(rects[i].x, gap); /* à `gap` du bord au moins */
        ASSERT_GE(rects[i].y, gap);
        ASSERT_LE(rects[i].x + rects[i].w + gap, 96U);
        ASSERT_LE(rects[i].y + rects[i].h + gap, 64U);
        ASSERT_EQ(10U + ((size_t)i * 3U), rects[i].w); /* les tailles ne changent pas */
        for (uint32_t j = i + 1U; j < 9U; j++) {
            ASSERT_FALSE(overlap(&rects[i], &rects[j], gap));
        }
    }
    /* du plus haut au plus bas : le plus haut ouvre la première étagère */
    uint32_t tallest = 0;
    for (uint32_t i = 1; i < 9U; i++) {
        tallest = rects[i].h > rects[tallest].h ? i : tallest;
    }
    ASSERT_EQ(tallest, order[0]);
    ASSERT_EQ(gap, rects[tallest].x);
    ASSERT_EQ(gap, rects[tallest].y);
}

UTEST(eng_bake_pack, what_does_not_fit_is_refused)
{
    uint32_t order[3];
    EngBakeRect wide[1] = {{0U, 0U, 30U, 4U}};
    ASSERT_TRUE(eng_bake_pack(wide, 1U, 32U, 32U, 1U, order)); /* 30 + 2 x 1 = 32 : tient juste */
    ASSERT_FALSE(eng_bake_pack(wide, 1U, 32U, 32U, 2U, order));
    EngBakeRect tall[3] = {{0U, 0U, 20U, 10U}, {0U, 0U, 20U, 10U}, {0U, 0U, 20U, 10U}};
    ASSERT_TRUE(eng_bake_pack(tall, 3U, 24U, 34U, 1U, order)); /* trois étagères de 10 + 4 x 1 */
    ASSERT_FALSE(eng_bake_pack(tall, 3U, 24U, 33U, 1U, order));
    EngBakeRect empty[2] = {{0U, 0U, 5U, 5U}, {0U, 0U, 0U, 5U}};
    ASSERT_FALSE(eng_bake_pack(empty, 2U, 64U, 64U, 1U, order));
    ASSERT_FALSE(eng_bake_pack(NULL, 1U, 64U, 64U, 1U, order));
    ASSERT_FALSE(eng_bake_pack(wide, 1U, 64U, 64U, 1U, NULL));
    ASSERT_FALSE(eng_bake_pack(wide, 0U, 64U, 64U, 1U, order));
}

/* ── Sommets inutilisés ── */

UTEST(eng_bake_compact, unused_vertices_are_removed_and_indices_follow)
{
    EngBakeVertex vertices[6];
    memset(vertices, 0, sizeof vertices);
    for (int i = 0; i < 6; i++) {
        vertices[i].position[0] = (float)(i * 10);
    }
    uint32_t indices[6] = {5U, 1U, 3U, 3U, 1U, 5U}; /* 0, 2 et 4 ne servent pas */
    uint32_t remap[6];
    ASSERT_EQ(3U, eng_bake_compact(vertices, 6U, indices, 2U, remap));
    ASSERT_NEAR(10.0F, vertices[0].position[0],
                1e-6F); /* l'ordre des sommets gardés est conservé */
    ASSERT_NEAR(30.0F, vertices[1].position[0], 1e-6F);
    ASSERT_NEAR(50.0F, vertices[2].position[0], 1e-6F);
    const uint32_t expected[6] = {2U, 0U, 1U, 1U, 0U, 2U};
    ASSERT_EQ(0, memcmp(expected, indices, sizeof indices));
    ASSERT_EQ(0U, eng_bake_compact(NULL, 6U, indices, 2U, remap));
    ASSERT_EQ(0U, eng_bake_compact(vertices, 6U, NULL, 2U, remap));
    ASSERT_EQ(0U, eng_bake_compact(vertices, 6U, indices, 2U, NULL));
}

/* ── Ombre du feuillage sur lui-même ── */

/* un bloc de feuillage dense : 12 x 12 x 12 morceaux de 0,25 m² tous les 0,5 m, de 0 à 5,5 m
 * (2 m² de feuille par mètre cube) */
static bool dense_block(EngBakeGrid *grid, float absorption)
{
    enum { SIDE = 12 };
    static float points[SIDE * SIDE * SIDE * 3];
    static float areas[SIDE * SIDE * SIDE];
    uint32_t n = 0;
    for (int x = 0; x < SIDE; x++) {
        for (int y = 0; y < SIDE; y++) {
            for (int z = 0; z < SIDE; z++) {
                points[(size_t)n * 3U] = (float)x * 0.5F;
                points[((size_t)n * 3U) + 1U] = (float)y * 0.5F;
                points[((size_t)n * 3U) + 2U] = (float)z * 0.5F;
                areas[n++] = 0.25F;
            }
        }
    }
    return eng_bake_grid_build(grid, points, areas, n, 12U, absorption);
}

UTEST(eng_bake_grid, the_heart_of_the_foliage_sees_less_sky_than_its_surface)
{
    EngBakeGrid grid;
    ASSERT_TRUE(dense_block(&grid, 0.5F));
    const float heart[3] = {2.75F, 2.75F, 2.75F};
    const float top[3] = {2.75F, 5.6F, 2.75F};
    const float bottom[3] = {2.75F, -0.1F, 2.75F};
    const float outside[3] = {2.75F, 40.0F, 2.75F};
    const float sky_heart = eng_bake_grid_sky(&grid, heart);
    const float sky_top = eng_bake_grid_sky(&grid, top);
    const float sky_bottom = eng_bake_grid_sky(&grid, bottom);
    ASSERT_GT(sky_heart, 0.0F);
    ASSERT_LT(sky_heart, 0.5F * sky_top); /* nettement plus sombre au cœur */
    ASSERT_GT(sky_top, sky_bottom);       /* le ciel éclaire surtout d'en haut */
    ASSERT_LE(sky_top, 1.0F);
    /* hors de la grille : la case la plus proche, celle de la marge au-dessus du bloc */
    ASSERT_GT(eng_bake_grid_sky(&grid, outside), sky_top - 1e-4F);
    ASSERT_NEAR(5.5F / 12.0F, grid.cell, 1e-5F); /* 12 cases sur le plus grand côté */
    eng_bake_grid_free(&grid);
    ASSERT_TRUE(grid.sky == NULL);
}

UTEST(eng_bake_grid, more_absorption_means_a_darker_heart_and_none_means_full_sky)
{
    EngBakeGrid light;
    EngBakeGrid dark;
    EngBakeGrid none;
    ASSERT_TRUE(dense_block(&light, 0.2F));
    ASSERT_TRUE(dense_block(&dark, 1.5F));
    ASSERT_TRUE(dense_block(&none, 0.0F));
    const float heart[3] = {2.75F, 2.75F, 2.75F};
    ASSERT_GT(eng_bake_grid_sky(&light, heart), eng_bake_grid_sky(&dark, heart) + 0.05F);
    ASSERT_NEAR(1.0F, eng_bake_grid_sky(&none, heart), 1e-4F);
    eng_bake_grid_free(&light);
    eng_bake_grid_free(&dark);
    eng_bake_grid_free(&none);
}

UTEST(eng_bake_grid, the_sky_varies_smoothly_between_cells)
{
    EngBakeGrid grid;
    ASSERT_TRUE(dense_block(&grid, 0.5F));
    float previous = eng_bake_grid_sky(&grid, (const float[3]){2.75F, 2.75F, 2.75F});
    for (int step = 1; step <= 40; step++) { /* du cœur vers le haut, par pas de 2 cm */
        const float p[3] = {2.75F, 2.75F + ((float)step * 0.02F), 2.75F};
        const float sky = eng_bake_grid_sky(&grid, p);
        ASSERT_LT(fabsf(sky - previous), 0.03F); /* pas de marche d'une case à l'autre */
        ASSERT_GE(sky, 0.0F);
        ASSERT_LE(sky, 1.0F);
        previous = sky;
    }
    eng_bake_grid_free(&grid);
}

UTEST(eng_bake_grid, invalid_parameters_leave_an_empty_grid_that_sees_the_whole_sky)
{
    const float points[6] = {0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F};
    const float areas[2] = {1.0F, 1.0F};
    const float bad[6] = {0.0F, NAN, 0.0F, 1.0F, 1.0F, 1.0F};
    const float at[3] = {0.5F, 0.5F, 0.5F};
    EngBakeGrid grid;
    ASSERT_FALSE(eng_bake_grid_build(NULL, points, areas, 2U, 8U, 0.5F));
    ASSERT_FALSE(eng_bake_grid_build(&grid, NULL, areas, 2U, 8U, 0.5F));
    ASSERT_FALSE(eng_bake_grid_build(&grid, points, NULL, 2U, 8U, 0.5F));
    ASSERT_FALSE(eng_bake_grid_build(&grid, points, areas, 0U, 8U, 0.5F));
    ASSERT_FALSE(eng_bake_grid_build(&grid, points, areas, 2U, 3U, 0.5F));
    ASSERT_FALSE(eng_bake_grid_build(&grid, points, areas, 2U, 97U, 0.5F));
    ASSERT_FALSE(eng_bake_grid_build(&grid, points, areas, 2U, 8U, -0.1F));
    ASSERT_FALSE(eng_bake_grid_build(&grid, bad, areas, 2U, 8U, 0.5F));
    ASSERT_TRUE(grid.sky == NULL);
    ASSERT_NEAR(1.0F, eng_bake_grid_sky(&grid, at), 1e-6F);
    ASSERT_NEAR(1.0F, eng_bake_grid_sky(NULL, at), 1e-6F);
    ASSERT_TRUE(eng_bake_grid_build(&grid, points, areas, 2U, 4U, 0.5F)); /* 4 : la plus petite */
    ASSERT_EQ(4U + 3U, grid.size[0]);                                     /* 4 cases et la marge */
    eng_bake_grid_free(&grid);
    eng_bake_grid_free(NULL); /* sans effet */
}
