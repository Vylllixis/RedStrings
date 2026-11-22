/* Ombres en cascade (M7-09), LOD du terrain (M7-05), LOD des meshes (M7-06) : parties CPU. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/csm.h"
#include "engine/mesh_lod.h"
#include "engine/terrain_lod.h"
#include "utest.h"

static EngCsmView view_at(float x, float z, float yaw)
{
    EngCsmView v;
    memset(&v, 0, sizeof v);
    v.position[0] = x;
    v.position[1] = 2.0F;
    v.position[2] = z;
    v.forward[0] = -sinf(yaw);
    v.forward[2] = -cosf(yaw);
    v.fov_y = 1.0472F;
    v.aspect = 16.0F / 9.0F;
    v.near_plane = 0.3F;
    v.far_plane = 600.0F;
    return v;
}

UTEST(csm, splits_are_increasing_and_cover_near_to_far)
{
    float s[ENG_CSM_MAX + 1];
    ASSERT_TRUE(eng_csm_splits(0.3F, 600.0F, 4, 0.75F, s));
    EXPECT_NEAR(s[0], 0.3F, 1e-6F);
    EXPECT_NEAR(s[4], 600.0F, 1e-3F);
    for (int i = 0; i < 4; i++) {
        EXPECT_LT(s[i], s[i + 1]);
    }
    EXPECT_LT(s[1], 600.0F / 4.0F); /* plus de résolution près de la caméra */
    EXPECT_FALSE(eng_csm_splits(0.0F, 600.0F, 4, 0.5F, s));
    EXPECT_FALSE(eng_csm_splits(1.0F, 600.0F, 5, 0.5F, s));
    EXPECT_FALSE(eng_csm_splits(1.0F, 600.0F, 3, 1.5F, s));
}

UTEST(csm, every_corner_of_each_slice_is_inside_its_cascade)
{
    EngCsm csm;
    const float sun[3] = {0.3F, 0.8F, 0.2F};
    const EngCsmView v = view_at(10.0F, -20.0F, 0.7F);
    ASSERT_TRUE(eng_csm_update(&csm, &v, sun, 4, 0.75F, 2048));
    const float t = tanf(v.fov_y * 0.5F);
    const float right[3] = {cosf(0.7F), 0.0F, -sinf(0.7F)};
    for (uint32_t i = 0; i < 4U; i++) {
        for (int e = 0; e < 2; e++) {
            const float d = csm.splits[i + (uint32_t)e];
            for (int c = 0; c < 4; c++) {
                const float sx = (c & 1) != 0 ? 1.0F : -1.0F;
                const float sy = (c & 2) != 0 ? 1.0F : -1.0F;
                const float p[3] = {
                    v.position[0] + v.forward[0] * d + right[0] * sx * d * t * v.aspect,
                    v.position[1] + sy * d * t,
                    v.position[2] + v.forward[2] * d + right[2] * sx * d * t * v.aspect};
                float uvd[3];
                eng_shadow_project(&csm.maps[i], p, uvd);
                EXPECT_GE(uvd[0], 0.0F);
                EXPECT_LE(uvd[0], 1.0F);
                EXPECT_GE(uvd[1], 0.0F);
                EXPECT_LE(uvd[1], 1.0F);
            }
        }
    }
    EXPECT_EQ(eng_csm_pick(&csm, 0.5F), 0U);
    EXPECT_EQ(eng_csm_pick(&csm, 599.0F), 3U);
    EXPECT_EQ(eng_csm_pick(&csm, 700.0F), 4U); /* au-delà : pas d'ombre */
    /* cascades de plus en plus grandes, texels de plus en plus gros */
    for (uint32_t i = 1; i < 4U; i++) {
        EXPECT_GT(csm.maps[i].texel_world, csm.maps[i - 1U].texel_world);
    }
    EXPECT_LT(csm.maps[0].texel_world, 0.05F); /* net de près : < 5 cm par texel */
}

UTEST(csm, turning_the_head_keeps_texel_size_and_small_moves_snap_to_whole_texels)
{
    const float sun[3] = {0.3F, 0.8F, 0.2F};
    EngCsm a;
    EngCsm b;
    EngCsmView v = view_at(0.0F, 0.0F, 0.0F);
    ASSERT_TRUE(eng_csm_update(&a, &v, sun, 3, 0.75F, 2048));
    for (int k = 0; k < 17; k++) { /* tour complet sur place */
        v = view_at(0.0F, 0.0F, 0.37F * (float)k);
        ASSERT_TRUE(eng_csm_update(&b, &v, sun, 3, 0.75F, 2048));
        for (uint32_t i = 0; i < 3U; i++) {
            EXPECT_EQ(b.maps[i].texel_world, a.maps[i].texel_world);
        }
    }
    /* avancer par petits pas : un point fixe du monde se déplace sur la carte d'un nombre ENTIER de
     * texels (0 la plupart du temps) — jamais d'une fraction : pas de « nage » des bords d'ombre */
    const float fixed[3] = {3.0F, 0.0F, -15.0F};
    v = view_at(0.0F, 0.0F, 0.0F);
    ASSERT_TRUE(eng_csm_update(&a, &v, sun, 3, 0.75F, 2048));
    float uvd0[3];
    eng_shadow_project(&a.maps[0], fixed, uvd0);
    uint32_t moves = 0;
    for (int k = 1; k <= 200; k++) {
        v = view_at(0.0F, -0.003F * (float)k, 0.0F); /* 3 mm par image */
        ASSERT_TRUE(eng_csm_update(&b, &v, sun, 3, 0.75F, 2048));
        float uvd[3];
        eng_shadow_project(&b.maps[0], fixed, uvd);
        const float du = (uvd[0] - uvd0[0]) * 2048.0F;
        const float dv = (uvd[1] - uvd0[1]) * 2048.0F;
        EXPECT_NEAR(du, roundf(du), 0.02F);
        EXPECT_NEAR(dv, roundf(dv), 0.02F);
        moves += (fabsf(du) > 0.5F || fabsf(dv) > 0.5F) ? 1U : 0U;
    }
    EXPECT_GT(moves, 0U); /* 60 cm parcourus : la carte a bien suivi */
}

/* ── LOD du terrain ── */

static EngTlodSelection g_sel;

static EngTlodParams params_8km(void)
{
    EngTlodParams p;
    memset(&p, 0, sizeof p);
    p.origin[0] = -4096.0F;
    p.origin[1] = -4096.0F;
    p.leaf = 32.0F;
    p.levels = 9; /* 32 · 2^8 = 8192 m */
    p.extent = 8192.0F;
    p.lod_range0 = 64.0F;
    p.morph_ratio = 0.7F;
    p.height_min = -20.0F;
    p.height_max = 120.0F;
    return p;
}

UTEST(terrain_lod, an_8km_terrain_is_covered_exactly_once_with_few_nodes)
{
    const EngTlodParams p = params_8km();
    const float cam[3] = {100.0F, 10.0F, -300.0F};
    ASSERT_TRUE(eng_tlod_select(&p, cam, &g_sel));
    double area = 0.0;
    for (uint32_t i = 0; i < g_sel.count; i++) {
        area += (double)g_sel.nodes[i].size * (double)g_sel.nodes[i].size;
    }
    EXPECT_NEAR(area, 8192.0 * 8192.0, 1.0); /* couverture exacte : ni trou ni recouvrement */
    EXPECT_LT(g_sel.count, 400U);
    EXPECT_GT(g_sel.per_level[0], 0U); /* fin sous les pieds */
    /* grille 32 x 32 quads par nœud : triangles dessinés vs pleine résolution (1 m) */
    const double tris = (double)g_sel.count * 32.0 * 32.0 * 2.0;
    const double full = 8192.0 * 8192.0 * 2.0;
    (void)fprintf(stderr, "terrain_lod: nodes=%u tris=%.0f (%.3f %% de la pleine résolution)\n",
                  g_sel.count, tris, 100.0 * tris / full);
    EXPECT_LT(tris, full / 100.0);
    /* deux nœuds voisins diffèrent d'au plus un niveau (condition sans fissure du CDLOD) */
    uint32_t bad = 0;
    for (uint32_t i = 0; i < g_sel.count; i++) {
        for (uint32_t j = i + 1U; j < g_sel.count; j++) {
            const EngTlodNode *a = &g_sel.nodes[i];
            const EngTlodNode *b = &g_sel.nodes[j];
            const bool touch_x = (fabsf(a->min[0] + a->size - b->min[0]) < 1e-3F ||
                                  fabsf(b->min[0] + b->size - a->min[0]) < 1e-3F) &&
                                 a->min[1] < b->min[1] + b->size && b->min[1] < a->min[1] + a->size;
            const bool touch_z = (fabsf(a->min[1] + a->size - b->min[1]) < 1e-3F ||
                                  fabsf(b->min[1] + b->size - a->min[1]) < 1e-3F) &&
                                 a->min[0] < b->min[0] + b->size && b->min[0] < a->min[0] + a->size;
            if ((touch_x || touch_z) && (a->level > b->level + 1U || b->level > a->level + 1U)) {
                bad++;
            }
        }
    }
    EXPECT_EQ(bad, 0U);
}

UTEST(terrain_lod, morph_goes_from_0_to_1_across_the_range_and_bad_params_are_refused)
{
    const EngTlodParams p = params_8km();
    const float cam[3] = {0.0F, 5.0F, 0.0F};
    ASSERT_TRUE(eng_tlod_select(&p, cam, &g_sel));
    const EngTlodNode *n = &g_sel.nodes[0];
    EXPECT_NEAR(eng_tlod_morph(n, 0.0F), 0.0F, 1e-6F);
    EXPECT_NEAR(eng_tlod_morph(n, n->morph_end), 1.0F, 1e-6F);
    const float mid = 0.5F * (n->morph_start + n->morph_end);
    EXPECT_NEAR(eng_tlod_morph(n, mid), 0.5F, 1e-5F);
    EngTlodParams bad = p;
    bad.extent = 5000.0F;
    EXPECT_FALSE(eng_tlod_select(&bad, cam, &g_sel));
    bad = p;
    bad.lod_range0 = 10.0F; /* plus petit qu'un nœud */
    EXPECT_FALSE(eng_tlod_select(&bad, cam, &g_sel));
    const float nan_cam[3] = {NAN, 0.0F, 0.0F};
    EXPECT_FALSE(eng_tlod_select(&p, nan_cam, &g_sel));
    /* caméra très haute : le terrain entier est grossier */
    const float sky[3] = {0.0F, 20000.0F, 0.0F};
    ASSERT_TRUE(eng_tlod_select(&p, sky, &g_sel));
    EXPECT_EQ(g_sel.count, 1U);
}

/* ── LOD des meshes ── */

static float g_pos[3 * 16641];
static uint32_t g_idx[6 * 128 * 128];
static float g_out_pos[3 * 16641];
static uint32_t g_out_idx[6 * 128 * 128];

UTEST(mesh_lod, a_dense_sphere_is_divided_by_4_and_stays_close_to_its_surface)
{
    /* sphère UV de 129 x 129 sommets, rayon 1 */
    const uint32_t n = 128;
    uint32_t vc = 0;
    for (uint32_t i = 0; i <= n; i++) {
        const float th = 3.14159265F * (float)i / (float)n;
        for (uint32_t j = 0; j <= n; j++) {
            const float ph = 6.2831853F * (float)j / (float)n;
            g_pos[(size_t)vc * 3U] = sinf(th) * cosf(ph);
            g_pos[(size_t)vc * 3U + 1U] = cosf(th);
            g_pos[(size_t)vc * 3U + 2U] = sinf(th) * sinf(ph);
            vc++;
        }
    }
    uint32_t ic = 0;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            const uint32_t a = i * (n + 1U) + j;
            const uint32_t b = a + n + 1U;
            g_idx[ic++] = a;
            g_idx[ic++] = b;
            g_idx[ic++] = a + 1U;
            g_idx[ic++] = a + 1U;
            g_idx[ic++] = b;
            g_idx[ic++] = b + 1U;
        }
    }
    uint32_t ovc = 0;
    uint32_t oic = 0;
    ASSERT_TRUE(eng_mesh_simplify(g_pos, vc, g_idx, ic, 32, g_out_pos, 16641, &ovc, g_out_idx,
                                  6 * 128 * 128, &oic));
    (void)fprintf(stderr, "mesh_lod: %u -> %u triangles, %u -> %u sommets\n", ic / 3U, oic / 3U, vc,
                  ovc);
    EXPECT_LE(oic * 4U, ic); /* divisé par 4 au moins */
    EXPECT_GT(oic, 0U);
    float worst = 0.0F;
    for (uint32_t i = 0; i < ovc; i++) {
        const float r = sqrtf(g_out_pos[(size_t)i * 3U] * g_out_pos[(size_t)i * 3U] +
                              g_out_pos[(size_t)i * 3U + 1U] * g_out_pos[(size_t)i * 3U + 1U] +
                              g_out_pos[(size_t)i * 3U + 2U] * g_out_pos[(size_t)i * 3U + 2U]);
        worst = fabsf(1.0F - r) > worst ? fabsf(1.0F - r) : worst;
    }
    const float cell = 2.0F / 32.0F;
    EXPECT_LT(worst, cell * 1.7321F); /* erreur bornée par la diagonale d'une case */
    for (uint32_t t = 0; t < oic; t += 3U) {
        EXPECT_NE(g_out_idx[t], g_out_idx[t + 1U]);
        EXPECT_NE(g_out_idx[t + 1U], g_out_idx[t + 2U]);
        EXPECT_LT(g_out_idx[t], ovc);
    }
    /* capacité insuffisante et arguments invalides */
    EXPECT_FALSE(eng_mesh_simplify(g_pos, vc, g_idx, ic, 32, g_out_pos, 10, &ovc, g_out_idx,
                                   6 * 128 * 128, &oic));
    EXPECT_FALSE(eng_mesh_simplify(g_pos, vc, g_idx, ic - 1U, 32, g_out_pos, 16641, &ovc, g_out_idx,
                                   6 * 128 * 128, &oic));
    EXPECT_FALSE(eng_mesh_simplify(g_pos, vc, g_idx, ic, 0, g_out_pos, 16641, &ovc, g_out_idx,
                                   6 * 128 * 128, &oic));
}

UTEST(mesh_lod, selection_with_hysteresis_does_not_flicker_on_a_threshold)
{
    const float th[3] = {20.0F, 60.0F, 150.0F}; /* 4 niveaux */
    EXPECT_EQ(eng_mesh_lod_select(0, 10.0F, th, 4, 0.1F), 0U);
    EXPECT_EQ(eng_mesh_lod_select(0, 25.0F, th, 4, 0.1F), 1U);
    EXPECT_EQ(eng_mesh_lod_select(0, 500.0F, th, 4, 0.1F), 3U);
    /* au niveau 1, juste sous le seuil 20 : on garde 1 tant qu'on n'est pas sous 18 */
    EXPECT_EQ(eng_mesh_lod_select(1, 19.5F, th, 4, 0.1F), 1U);
    EXPECT_EQ(eng_mesh_lod_select(1, 17.9F, th, 4, 0.1F), 0U);
    uint32_t lod = 1;
    uint32_t changes = 0;
    for (int k = 0; k < 100; k++) { /* caméra qui oscille de ±0,5 m autour de 20 m */
        const uint32_t next =
            eng_mesh_lod_select(lod, 20.0F + ((k % 2) != 0 ? 0.5F : -0.5F), th, 4, 0.1F);
        changes += next != lod ? 1U : 0U;
        lod = next;
    }
    EXPECT_EQ(changes, 0U);
    EXPECT_EQ(eng_mesh_lod_select(3, 10.0F, th, 4, 0.1F), 0U); /* saute plusieurs niveaux */
    EXPECT_EQ(eng_mesh_lod_select(9, 30.0F, th, 4, 0.1F), 1U);
    EXPECT_EQ(eng_mesh_lod_select(0, NAN, th, 4, 0.1F), 0U);
}
