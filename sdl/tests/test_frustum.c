#include <math.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/frustum.h"
#include "engine/terrain.h"
#include "utest.h"

/* Frustum et culling CPU (M3-04). Caméra à l'origine regardant -Z, champ vertical 90°, aspect 1,
 * plans à 1 et 100 m : la pyramide est |x| <= -z, |y| <= -z, 1 <= -z <= 100. */

static void camera_matrix(const float eye[3], const float target[3], float fov_deg, float aspect,
                          float near_z, float far_z, mat4 out)
{
    mat4 view;
    mat4 proj;
    glm_lookat((float *)eye, (float *)target, (vec3){0.0F, 1.0F, 0.0F}, view);
    glm_perspective(glm_rad(fov_deg), aspect, near_z, far_z, proj);
    glm_mat4_mul(proj, view, out);
}

static EngFrustum standard(void)
{
    mat4 vp;
    camera_matrix((float[3]){0, 0, 0}, (float[3]){0, 0, -1}, 90.0F, 1.0F, 1.0F, 100.0F, vp);
    EngFrustum f;
    const bool ok = eng_frustum_from_view_proj(&f, vp);
    if (!ok) {
        memset(&f, 0, sizeof f); /* les tests qui suivent échouent alors sur les valeurs */
    }
    return f;
}

UTEST(eng_frustum, the_six_planes_are_unit_normals_pointing_inwards)
{
    const EngFrustum f = standard();
    for (int p = 0; p < 6; p++) {
        const float *pl = f.planes[p];
        EXPECT_NEAR(sqrtf((pl[0] * pl[0]) + (pl[1] * pl[1]) + (pl[2] * pl[2])), 1.0F, 1e-5F);
    }
    /* le point (0, 0, -10) est à l'intérieur de chaque plan : distances connues */
    const float inside[3] = {0.0F, 0.0F, -10.0F};
    const float s = sqrtf(0.5F);
    const float expected[6] = {10.0F * s, 10.0F * s, 10.0F * s, 10.0F * s, 9.0F, 90.0F};
    for (int p = 0; p < 6; p++) {
        const float *pl = f.planes[p];
        const float d = (pl[0] * inside[0]) + (pl[1] * inside[1]) + (pl[2] * inside[2]) + pl[3];
        EXPECT_NEAR(d, expected[p],
                    2e-3F); /* float : la matrice de projection porte 1e-5 relatif */
    }
}

UTEST(eng_frustum, points_are_inside_on_the_border_and_outside_for_every_plane)
{
    const EngFrustum f = standard();
    EXPECT_TRUE(eng_frustum_contains_point(&f, (float[3]){0, 0, -10}));
    EXPECT_TRUE(eng_frustum_contains_point(&f, (float[3]){9.9F, 9.9F, -10}));
    EXPECT_FALSE(eng_frustum_contains_point(&f, (float[3]){10.5F, 0, -10}));  /* droite */
    EXPECT_FALSE(eng_frustum_contains_point(&f, (float[3]){-10.5F, 0, -10})); /* gauche */
    EXPECT_FALSE(eng_frustum_contains_point(&f, (float[3]){0, 10.5F, -10}));  /* haut */
    EXPECT_FALSE(eng_frustum_contains_point(&f, (float[3]){0, -10.5F, -10})); /* bas */
    EXPECT_FALSE(
        eng_frustum_contains_point(&f, (float[3]){0, 0, -0.5F}));         /* avant le plan proche */
    EXPECT_FALSE(eng_frustum_contains_point(&f, (float[3]){0, 0, -101})); /* au-delà du lointain */
    EXPECT_FALSE(eng_frustum_contains_point(&f, (float[3]){0, 0, 10}));   /* derrière */
    EXPECT_TRUE(eng_frustum_contains_point(&f, (float[3]){0, 0, -1.001F}));
    EXPECT_TRUE(eng_frustum_contains_point(&f, (float[3]){0, 0, -99.9F}));
}

UTEST(eng_frustum, boxes_are_classified_outside_intersecting_or_inside)
{
    const EngFrustum f = standard();
    float lo[3] = {-1, -1, -21};
    float hi[3] = {1, 1, -19};
    EXPECT_EQ(eng_frustum_classify_aabb(&f, lo, hi), ENG_FRUSTUM_INSIDE);
    /* à cheval sur chacun des six plans */
    const float straddle[6][2][3] = {
        {{-30, -1, -21}, {-5, 1, -19}},        /* gauche */
        {{5, -1, -21}, {30, 1, -19}},          /* droite */
        {{-1, -30, -21}, {1, -5, -19}},        /* bas */
        {{-1, 5, -21}, {1, 30, -19}},          /* haut */
        {{-0.1F, -0.1F, -3}, {0.1F, 0.1F, 3}}, /* proche */
        {{-1, -1, -150}, {1, 1, -50}},         /* lointain */
    };
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(eng_frustum_classify_aabb(&f, straddle[i][0], straddle[i][1]),
                  ENG_FRUSTUM_INTERSECT);
        EXPECT_TRUE(eng_frustum_intersects_aabb(&f, straddle[i][0], straddle[i][1]));
    }
    /* entièrement dehors pour chacun des six plans, et derrière la caméra */
    const float outside[7][2][3] = {
        {{-50, -1, -21}, {-30, 1, -19}},  {{30, -1, -21}, {50, 1, -19}},
        {{-1, -50, -21}, {1, -30, -19}},  {{-1, 30, -21}, {1, 50, -19}},
        {{-1, -1, -0.8F}, {1, 1, -0.2F}}, {{-1, -1, -300}, {1, 1, -200}},
        {{-1, -1, 19}, {1, 1, 21}},
    };
    for (int i = 0; i < 7; i++) {
        EXPECT_EQ(eng_frustum_classify_aabb(&f, outside[i][0], outside[i][1]), ENG_FRUSTUM_OUTSIDE);
        EXPECT_FALSE(eng_frustum_intersects_aabb(&f, outside[i][0], outside[i][1]));
    }
    /* un point (boîte nulle) dedans / dehors */
    float p[3] = {0, 0, -10};
    EXPECT_EQ(eng_frustum_classify_aabb(&f, p, p), ENG_FRUSTUM_INSIDE);
    float q[3] = {50, 0, -10};
    EXPECT_EQ(eng_frustum_classify_aabb(&f, q, q), ENG_FRUSTUM_OUTSIDE);
    /* une boîte qui contient toute la pyramide coupe chaque plan : à cheval */
    float big_lo[3] = {-1000, -1000, -1000};
    float big_hi[3] = {1000, 1000, 1000};
    EXPECT_EQ(eng_frustum_classify_aabb(&f, big_lo, big_hi), ENG_FRUSTUM_INTERSECT);
    /* sur un plan : la boîte touche la limite droite (x = -z) par un coin : acceptée */
    float tl[3] = {10, -1, -11};
    float th[3] = {12, 1, -9};
    EXPECT_TRUE(eng_frustum_intersects_aabb(&f, tl, th));
}

UTEST(eng_frustum, invalid_boxes_and_matrices_are_rejected)
{
    const EngFrustum f = standard();
    EXPECT_EQ(eng_frustum_classify_aabb(&f, (float[3]){1, 1, -10}, (float[3]){0, 2, -9}),
              ENG_FRUSTUM_OUTSIDE); /* min > max */
    EXPECT_EQ(eng_frustum_classify_aabb(&f, (float[3]){NAN, 0, -10}, (float[3]){1, 1, -9}),
              ENG_FRUSTUM_OUTSIDE);
    EXPECT_EQ(eng_frustum_classify_aabb(&f, (float[3]){0, 0, -10}, (float[3]){1, INFINITY, -9}),
              ENG_FRUSTUM_OUTSIDE);
    mat4 bad;
    glm_mat4_zero(bad);
    EngFrustum g = f;
    EXPECT_FALSE(eng_frustum_from_view_proj(&g, bad));
    EXPECT_FALSE(eng_frustum_intersects_aabb(&g, (float[3]){-1, -1, -10}, (float[3]){1, 1, -9}));
    glm_mat4_identity(bad);
    bad[1][2] = NAN;
    EXPECT_FALSE(eng_frustum_from_view_proj(&g, bad));
    bad[1][2] = INFINITY;
    EXPECT_FALSE(eng_frustum_from_view_proj(&g, bad));
}

/* Référence indépendante : une boîte est visible si l'un de ses huit coins, projeté par la matrice
 * (coordonnées de découpage), est dans le cube de découpage, OU si elle est à cheval ; ici on
 * compare le TEST PAR COINS (huit coins en espace de découpage, les six demi-espaces) à notre test
 * par plans : un algorithme différent (sans normalisation) doit donner le même verdict. */
static bool corners_clip_test(mat4 vp, const float lo[3], const float hi[3])
{
    for (int side = 0; side < 6; side++) {
        bool all_outside = true;
        for (int c = 0; c < 8; c++) {
            const float p[4] = {(c & 1) ? hi[0] : lo[0], (c & 2) ? hi[1] : lo[1],
                                (c & 4) ? hi[2] : lo[2], 1.0F};
            float clip[4];
            for (int r = 0; r < 4; r++) {
                clip[r] = (vp[0][r] * p[0]) + (vp[1][r] * p[1]) + (vp[2][r] * p[2]) + vp[3][r];
            }
            bool out = false;
            switch (side) {
            case 0:
                out = clip[0] < -clip[3];
                break;
            case 1:
                out = clip[0] > clip[3];
                break;
            case 2:
                out = clip[1] < -clip[3];
                break;
            case 3:
                out = clip[1] > clip[3];
                break;
            case 4:
                out = clip[2] < 0.0F;
                break;
            default:
                out = clip[2] > clip[3];
                break;
            }
            all_outside = all_outside && out;
        }
        if (all_outside) {
            return false;
        }
    }
    return true;
}

static float frand(uint32_t *state, float lo, float hi)
{
    *state = (*state * 1664525U) + 1013904223U;
    return lo + (hi - lo) * ((float)(*state >> 8) / 16777216.0F);
}

UTEST(eng_frustum,
      the_plane_test_agrees_with_an_independent_corner_clip_test_and_never_culls_a_visible_point)
{
    uint32_t state = 4242U;
    uint32_t visible_boxes = 0;
    uint32_t hidden_boxes = 0;
    for (int cam = 0; cam < 6; cam++) {
        mat4 vp;
        const float eye[3] = {frand(&state, -50, 50), frand(&state, 0, 60), frand(&state, -50, 50)};
        const float target[3] = {frand(&state, -50, 50), frand(&state, -10, 30),
                                 frand(&state, -50, 50)};
        camera_matrix(eye, target, 30.0F + (float)cam * 15.0F, 0.5F + (float)cam * 0.3F, 0.5F,
                      300.0F, vp);
        EngFrustum f;
        ASSERT_TRUE(eng_frustum_from_view_proj(&f, vp));
        for (int i = 0; i < 4000; i++) {
            const float c[3] = {frand(&state, -200, 200), frand(&state, -50, 100),
                                frand(&state, -200, 200)};
            const float h[3] = {frand(&state, 0.1F, 40), frand(&state, 0.1F, 40),
                                frand(&state, 0.1F, 40)};
            const float lo[3] = {c[0] - h[0], c[1] - h[1], c[2] - h[2]};
            const float hi[3] = {c[0] + h[0], c[1] + h[1], c[2] + h[2]};
            const bool plane_test = eng_frustum_intersects_aabb(&f, lo, hi);
            ASSERT_EQ(plane_test, corners_clip_test(vp, lo, hi));
            if (plane_test) {
                visible_boxes++;
            } else {
                hidden_boxes++;
                /* jamais de faux rejet : aucun point de la boîte rejetée n'est dans la pyramide */
                for (int k = 0; k < 20; k++) {
                    const float pt[3] = {frand(&state, lo[0], hi[0]), frand(&state, lo[1], hi[1]),
                                         frand(&state, lo[2], hi[2])};
                    ASSERT_FALSE(eng_frustum_contains_point(&f, pt));
                }
            }
        }
    }
    /* le test n'est pas trivial : beaucoup de boîtes visibles et beaucoup de cachées */
    EXPECT_GT(visible_boxes, (uint32_t)1000);
    EXPECT_GT(hidden_boxes, (uint32_t)1000);
}

UTEST(eng_frustum, a_box_with_a_visible_point_is_never_rejected)
{
    uint32_t state = 777U;
    mat4 vp;
    camera_matrix((float[3]){5, 20, 5}, (float[3]){60, 0, 60}, 60.0F, 1.5F, 0.3F, 400.0F, vp);
    EngFrustum f;
    ASSERT_TRUE(eng_frustum_from_view_proj(&f, vp));
    uint32_t checked = 0;
    for (int i = 0; i < 20000; i++) {
        const float p[3] = {frand(&state, -100, 200), frand(&state, -20, 80),
                            frand(&state, -100, 200)};
        if (!eng_frustum_contains_point(&f, p)) {
            continue;
        }
        const float h = frand(&state, 0.0F, 30.0F);
        const float lo[3] = {p[0] - h, p[1] - (h * 0.5F), p[2] - (h * 1.5F)};
        const float hi[3] = {p[0] + (h * 2.0F), p[1] + h, p[2] + h};
        ASSERT_TRUE(eng_frustum_intersects_aabb(&f, lo, hi));
        checked++;
    }
    EXPECT_GT(checked, (uint32_t)500);
}

/* ── Chunks du terrain ── */

typedef struct Terr {
    EngArena arena;
    EngTerrain terrain;
} Terr;

static void fill_flat(uint16_t *samples, uint32_t size)
{
    for (size_t i = 0; i < (size_t)size * size; i++) {
        samples[i] = 1000;
    }
}

static bool open_terr(Terr *t, uint32_t size, uint32_t chunk_cells, float origin)
{
    *t = (Terr){0};
    if (!eng_arena_init(&t->arena, "test-frustum", (size_t)32 << 20)) {
        return false;
    }
    uint16_t *samples = eng_arena_alloc(&t->arena, (size_t)size * size * 2U, 16);
    fill_flat(samples, size);
    const EngHeightmap map = {.size = size,
                              .spacing = 1.0F,
                              .altitude_min = 0.0F,
                              .altitude_max = 65535.0F,
                              .samples = samples};
    const EngTerrainParams params = {
        .chunk_cells = chunk_cells, .origin_x = origin, .origin_z = origin};
    return eng_terrain_build(&t->arena, &map, &params, &t->terrain);
}

UTEST(eng_frustum, a_camera_over_the_terrain_sees_some_chunks_and_the_sky_none)
{
    Terr t;
    ASSERT_TRUE(open_terr(&t, 257, 32, -128.0F)); /* 8 x 8 = 64 chunks de 32 m, centrés */
    uint32_t visible[64];
    mat4 vp;
    EngFrustum f;
    /* 300 m au-dessus du centre, vers le bas : fov 60° -> demi-largeur au sol 173 m > 128 : tout */
    camera_matrix((float[3]){0, 1300, 0}, (float[3]){0, 1000, -0.01F}, 60.0F, 1.0F, 1.0F, 2000.0F,
                  vp);
    ASSERT_TRUE(eng_frustum_from_view_proj(&f, vp));
    EXPECT_EQ(eng_terrain_visible_chunks(&t.terrain, &f, visible, 64), (uint32_t)64);
    /* au ras du sol vers l'horizon -Z, fov 60° : le quart avant seulement */
    camera_matrix((float[3]){0, 1002.0F, 0}, (float[3]){0, 1002.0F, -100}, 60.0F, 1.0F, 1.0F,
                  2000.0F, vp);
    /* l'altitude du terrain est 1000 + 0 : la caméra est 2 m au-dessus */
    ASSERT_TRUE(eng_frustum_from_view_proj(&f, vp));
    const uint32_t ahead = eng_terrain_visible_chunks(&t.terrain, &f, visible, 64);
    EXPECT_GT(ahead, (uint32_t)0);
    EXPECT_LT(ahead, (uint32_t)64);
    /* aucun chunk derrière la caméra : les indices z >= 4 (z > 0 m) sont absents, sauf à cheval
     * (le plan proche est à 1 m : un chunk dont le bord z = 0 est devant) */
    for (uint32_t i = 0; i < ahead; i++) {
        EXPECT_LT(visible[i] / 8U, (uint32_t)5);
        if (i > 0) {
            EXPECT_GT(visible[i], visible[i - 1]); /* croissants */
        }
    }
    /* vers le ciel : rien */
    camera_matrix((float[3]){0, 1002.0F, 0}, (float[3]){0, 1100.0F, -10}, 60.0F, 1.0F, 1.0F,
                  2000.0F, vp);
    ASSERT_TRUE(eng_frustum_from_view_proj(&f, vp));
    EXPECT_EQ(eng_terrain_visible_chunks(&t.terrain, &f, visible, 64), (uint32_t)0);
    /* tampon trop petit : tronqué */
    camera_matrix((float[3]){0, 1300, 0}, (float[3]){0, 1000, -0.01F}, 60.0F, 1.0F, 1.0F, 2000.0F,
                  vp);
    ASSERT_TRUE(eng_frustum_from_view_proj(&f, vp));
    EXPECT_EQ(eng_terrain_visible_chunks(&t.terrain, &f, visible, 10), (uint32_t)10);
    EXPECT_EQ(visible[9], (uint32_t)9);
    EXPECT_EQ(eng_terrain_visible_chunks(&t.terrain, &f, visible, 0), (uint32_t)0);
    eng_arena_shutdown(&t.arena);
}

UTEST(eng_frustum, the_visible_chunk_list_is_exactly_the_chunks_whose_box_meets_the_frustum)
{
    Terr t;
    ASSERT_TRUE(open_terr(&t, 129, 16, -64.0F)); /* 8 x 8 chunks de 16 m */
    uint32_t visible[64];
    uint32_t state = 99U;
    for (int round = 0; round < 40; round++) {
        mat4 vp;
        const float eye[3] = {frand(&state, -90, 90), frand(&state, 1005, 1060),
                              frand(&state, -90, 90)};
        const float target[3] = {frand(&state, -60, 60), frand(&state, 990, 1010),
                                 frand(&state, -60, 60)};
        camera_matrix(eye, target, 50.0F, 1.6F, 0.5F, 500.0F, vp);
        EngFrustum f;
        ASSERT_TRUE(eng_frustum_from_view_proj(&f, vp));
        const uint32_t count = eng_terrain_visible_chunks(&t.terrain, &f, visible, 64);
        uint32_t expected = 0;
        for (uint32_t i = 0; i < 64; i++) {
            const bool meets = corners_clip_test(vp, t.terrain.chunks[i].bounds_min,
                                                 t.terrain.chunks[i].bounds_max);
            if (meets) {
                ASSERT_LT(expected, count);
                ASSERT_EQ(visible[expected], i);
                expected++;
            }
        }
        ASSERT_EQ(count, expected);
    }
    eng_arena_shutdown(&t.arena);
}

/* Matrice identité : la pyramide est le cube de découpage -1 <= x, y <= 1, 0 <= z <= 1. Les plans
 * passent exactement par des valeurs entières : les cas « exactement sur un plan » sont exacts. */
UTEST(eng_frustum, a_point_or_box_exactly_on_a_plane_counts_as_visible)
{
    mat4 vp;
    glm_mat4_identity(vp);
    EngFrustum f;
    ASSERT_TRUE(eng_frustum_from_view_proj(&f, vp));
    /* sur chacune des six faces et sur un coin */
    const float on_plane[7][3] = {{-1, 0, 0.5F}, {1, 0, 0.5F}, {0, -1, 0.5F}, {0, 1, 0.5F},
                                  {0, 0, 0},     {0, 0, 1},    {1, 1, 1}};
    for (int i = 0; i < 7; i++) {
        EXPECT_TRUE(eng_frustum_contains_point(&f, on_plane[i]));
    }
    EXPECT_FALSE(eng_frustum_contains_point(&f, (float[3]){1.001F, 0, 0.5F}));
    EXPECT_FALSE(eng_frustum_contains_point(&f, (float[3]){0, 0, -0.001F}));
    EXPECT_FALSE(eng_frustum_contains_point(&f, (float[3]){0, 0, 1.001F}));
    /* une boîte qui ne fait que TOUCHER une face de l'extérieur n'est pas rejetée (conservatif) */
    EXPECT_EQ(eng_frustum_classify_aabb(&f, (float[3]){1, -0.5F, 0.2F}, (float[3]){2, 0.5F, 0.8F}),
              ENG_FRUSTUM_INTERSECT);
    EXPECT_EQ(
        eng_frustum_classify_aabb(&f, (float[3]){1.001F, -0.5F, 0.2F}, (float[3]){2, 0.5F, 0.8F}),
        ENG_FRUSTUM_OUTSIDE);
    /* une boîte intérieure qui affleure une face reste INSIDE */
    EXPECT_EQ(eng_frustum_classify_aabb(&f, (float[3]){0.5F, -1, 0}, (float[3]){1, 1, 1}),
              ENG_FRUSTUM_INSIDE);
    EXPECT_EQ(eng_frustum_classify_aabb(&f, (float[3]){0.5F, -1, 0}, (float[3]){1.001F, 1, 1}),
              ENG_FRUSTUM_INTERSECT);
}

UTEST(eng_frustum, the_chunk_array_variant_reads_exactly_chunk_count_boxes_and_honours_capacity)
{
    mat4 vp;
    glm_mat4_identity(vp);
    EngFrustum f;
    ASSERT_TRUE(eng_frustum_from_view_proj(&f, vp));
    /* tableau de pile de taille exacte : ASan voit toute lecture au-delà de `count` */
    EngTerrainChunk chunks[3] = {
        {.bounds_min = {-0.5F, -0.5F, 0.1F}, .bounds_max = {0.5F, 0.5F, 0.9F}}, /* dedans */
        {.bounds_min = {5, 5, 0.1F}, .bounds_max = {6, 6, 0.9F}},               /* dehors */
        {.bounds_min = {0.5F, 0.5F, 0.1F}, .bounds_max = {3, 3, 0.9F}},         /* à cheval */
    };
    uint32_t out[8];
    EXPECT_EQ(eng_chunks_visible(chunks, 3, &f, out, 8), (uint32_t)2);
    EXPECT_EQ(out[0], (uint32_t)0);
    EXPECT_EQ(out[1], (uint32_t)2);
    EXPECT_EQ(eng_chunks_visible(chunks, 3, &f, out, 1), (uint32_t)1);
    EXPECT_EQ(eng_chunks_visible(chunks, 3, &f, out, 0), (uint32_t)0);
    EXPECT_EQ(eng_chunks_visible(chunks, 2, &f, out, 8), (uint32_t)1);
    EXPECT_EQ(eng_chunks_visible(chunks, 0, &f, out, 8), (uint32_t)0);
}
