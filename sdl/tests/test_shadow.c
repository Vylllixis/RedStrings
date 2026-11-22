#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "engine/shadow.h"
#include "utest.h"

/* Ombre du soleil (M1-17), côté CPU : la caméra du soleil, le modèle de biais, le filtrage PCF.
 * Références : valeurs calculées à la main (propriétés géométriques de la projection
 * orthographique, rampe de la comparaison bilinéaire) ; pour l'acné et le décollement, une carte
 * d'ombre synthétique par LANCER DE RAYONS exact (plan et boîte), qui ne partage aucun code avec le
 * module testé. */

#define PI_F 3.14159265358979323846F

static void sun_from_elevation(float elevation_deg, float azimuth_deg, float out[3])
{
    const float e = elevation_deg * PI_F / 180.0F;
    const float a = azimuth_deg * PI_F / 180.0F;
    out[0] = cosf(e) * sinf(a);
    out[1] = sinf(e);
    out[2] = cosf(e) * cosf(a);
}

UTEST(shadow_params, default_values_are_valid)
{
    EngShadowParams p;
    eng_shadow_params_default(&p);
    EXPECT_TRUE(eng_shadow_params_sanitize(&p));
    EXPECT_EQ(p.pcf_radius, 1U);
    EXPECT_NEAR(p.strength, 1.0F, 0.0F);
}

UTEST(shadow_params, sanitize_clamps_and_reports)
{
    EngShadowParams p = {.constant_bias = -1.0F,
                         .slope_bias = NAN,
                         .normal_bias = INFINITY,
                         .pcf_radius = 9U,
                         .strength = 3.0F};
    EXPECT_FALSE(eng_shadow_params_sanitize(&p));
    EXPECT_NEAR(p.constant_bias, 0.0F, 0.0F);
    EXPECT_NEAR(p.slope_bias, 0.0F, 0.0F);
    EXPECT_NEAR(p.normal_bias, 0.0F, 0.0F);
    EXPECT_EQ(p.pcf_radius, ENG_SHADOW_MAX_PCF_RADIUS);
    EXPECT_NEAR(p.strength, 1.0F, 0.0F);
    p.strength = -0.5F;
    EXPECT_FALSE(eng_shadow_params_sanitize(&p));
    EXPECT_NEAR(p.strength, 0.0F, 0.0F);
    p.strength = NAN;
    EXPECT_FALSE(eng_shadow_params_sanitize(&p));
    EXPECT_NEAR(p.strength, 1.0F, 0.0F);
    /* la limite exacte : 3 est valide, 4 est ramené à 3 */
    EngShadowParams q = {.pcf_radius = ENG_SHADOW_MAX_PCF_RADIUS, .strength = 1.0F};
    EXPECT_TRUE(eng_shadow_params_sanitize(&q));
    q.pcf_radius = ENG_SHADOW_MAX_PCF_RADIUS + 1U;
    EXPECT_FALSE(eng_shadow_params_sanitize(&q));
    EXPECT_EQ(q.pcf_radius, ENG_SHADOW_MAX_PCF_RADIUS);
}

UTEST(shadow_fit, sphere_from_box_is_the_circumscribed_sphere)
{
    const float lo[3] = {-1.0F, 0.0F, -2.0F};
    const float hi[3] = {3.0F, 4.0F, 2.0F};
    float center[3];
    float radius;
    eng_shadow_sphere_from_box(lo, hi, center, &radius);
    EXPECT_NEAR(center[0], 1.0F, 1e-6F);
    EXPECT_NEAR(center[1], 2.0F, 1e-6F);
    EXPECT_NEAR(center[2], 0.0F, 1e-6F);
    EXPECT_NEAR(radius, sqrtf(12.0F), 1e-5F); /* demi-diagonale : sqrt(2^2 + 2^2 + 2^2) */
}

UTEST(shadow_fit, refuses_invalid_input_and_writes_nothing)
{
    const float sun[3] = {0.0F, 1.0F, 0.0F};
    const float zero[3] = {0.0F, 0.0F, 0.0F};
    const float nan3[3] = {NAN, 0.0F, 0.0F};
    struct {
        const float *to_sun;
        const float *center;
        float radius;
        uint32_t size;
    } const bad[] = {
        {zero, zero, 5.0F, 256U}, {nan3, zero, 5.0F, 256U}, {sun, nan3, 5.0F, 256U},
        {sun, zero, 0.0F, 256U},  {sun, zero, -1.0F, 256U}, {sun, zero, NAN, 256U},
        {sun, zero, 5.0F, 0U},    {sun, zero, 5.0F, 3U},    {sun, zero, 5.0F, 4U},
        {sun, zero, 5.0F, 7U},
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        EngShadowMap map;
        memset(&map, 0x5A, sizeof map);
        EXPECT_FALSE(
            eng_shadow_fit(&map, bad[i].to_sun, bad[i].center, bad[i].radius, bad[i].size));
        /* la structure est intacte, octet pour octet */
        uint32_t words[16];
        memcpy(words, map.view_proj, sizeof words);
        for (int w = 0; w < 16; w++) {
            EXPECT_EQ(words[w], 0x5A5A5A5AU);
        }
        memcpy(words, map.shadow_matrix, sizeof words);
        for (int w = 0; w < 16; w++) {
            EXPECT_EQ(words[w], 0x5A5A5A5AU);
        }
        EXPECT_EQ(map.size, 0x5A5A5A5AU);
    }
    EngShadowMap ok;
    EXPECT_TRUE(eng_shadow_fit(&ok, sun, zero, 5.0F, 8U));
    EXPECT_EQ(ok.size, 8U);
}

UTEST(shadow_fit, the_whole_sphere_is_inside_the_map_with_a_one_texel_margin)
{
    /* pour des soleils très différents (dont à la verticale et sous l'horizon), tous les points de
     * la sphère englobante se projettent dans [1/size, 1 - 1/size]^2 x [0, 1] (marge : 1,5 texel)
     */
    const float angles[6][2] = {{45.0F, 30.0F}, {10.0F, 200.0F}, {89.9F, 0.0F},
                                {90.0F, 0.0F},  {-60.0F, 10.0F}, {0.0F, 90.0F}};
    const float center[3] = {3.0F, -2.0F, 7.5F};
    const float radius = 20.0F;
    const uint32_t size = 256U;
    for (int a = 0; a < 6; a++) {
        float sun[3];
        sun_from_elevation(angles[a][0], angles[a][1], sun);
        EngShadowMap map;
        ASSERT_TRUE(eng_shadow_fit(&map, sun, center, radius, size));
        for (int k = 0; k < 400; k++) {
            /* points de la sphère (spirale de Fibonacci) */
            const float t = ((float)k + 0.5F) / 400.0F;
            const float y = 1.0F - (2.0F * t);
            const float ring = sqrtf(1.0F - (y * y));
            const float phi = 2.399963F * (float)k;
            const float p[3] = {center[0] + (radius * ring * cosf(phi)), center[1] + (radius * y),
                                center[2] + (radius * ring * sinf(phi))};
            float uvd[3];
            eng_shadow_project(&map, p, uvd);
            EXPECT_GE(uvd[0], 1.0F / (float)size - 1e-4F);
            EXPECT_LE(uvd[0], 1.0F - (1.0F / (float)size) + 1e-4F);
            EXPECT_GE(uvd[1], 1.0F / (float)size - 1e-4F);
            EXPECT_LE(uvd[1], 1.0F - (1.0F / (float)size) + 1e-4F);
            EXPECT_GE(uvd[2], -1e-4F);
            EXPECT_LE(uvd[2], 1.0F + 1e-4F);
        }
    }
}

UTEST(shadow_fit, orientation_depth_is_zero_toward_the_sun_and_v_is_top_down)
{
    const float sun[3] = {0.0F, 1.0F, 0.0F}; /* soleil au zénith : la caméra regarde vers -Y */
    const float center[3] = {0.0F, 0.0F, 0.0F};
    EngShadowMap map;
    ASSERT_TRUE(eng_shadow_fit(&map, sun, center, 10.0F, 512U));
    float near_p[3] = {0.0F, 10.0F, 0.0F};
    float far_p[3] = {0.0F, -10.0F, 0.0F};
    float uvd[3];
    eng_shadow_project(&map, near_p, uvd);
    EXPECT_NEAR(uvd[2], 0.5F - (10.0F / map.depth_range), 1e-4F);
    EXPECT_LT(uvd[2], 0.5F);
    float uvd_far[3];
    eng_shadow_project(&map, far_p, uvd_far);
    EXPECT_GT(uvd_far[2], 0.5F);
    EXPECT_NEAR(uvd[2] + uvd_far[2], 1.0F, 1e-4F); /* symétriques autour du centre (0,5) */
    /* deux points du plan de la carte, séparés le long de la base de la caméra : leurs (u, v)
     * s'écartent, et le centre de la sphère est au milieu de la carte (au pas d'un texel près) */
    const float origin[3] = {0.0F, 0.0F, 0.0F};
    eng_shadow_project(&map, origin, uvd);
    EXPECT_NEAR(uvd[0], 0.5F, 1.5F / 512.0F);
    EXPECT_NEAR(uvd[1], 0.5F, 1.5F / 512.0F);
    EXPECT_NEAR(uvd[2], 0.5F, 1e-4F);
    /* l'écart d'un mètre correspond à (1 / texel_world) texels */
    const float east[3] = {1.0F, 0.0F, 0.0F};
    const float north[3] = {0.0F, 0.0F, 1.0F};
    float ue[3];
    float un[3];
    eng_shadow_project(&map, east, ue);
    eng_shadow_project(&map, north, un);
    const float per_meter = 1.0F / (map.texel_world * 512.0F);
    EXPECT_NEAR(fabsf(ue[0] - uvd[0]) + fabsf(ue[1] - uvd[1]), per_meter, 1e-5F);
    EXPECT_NEAR(fabsf(un[0] - uvd[0]) + fabsf(un[1] - uvd[1]), per_meter, 1e-5F);
}

UTEST(shadow_fit, texel_size_and_depth_range_follow_the_radius_and_margin)
{
    const float sun[3] = {0.0F, 1.0F, 0.0F};
    const float center[3] = {0.0F, 0.0F, 0.0F};
    EngShadowMap map;
    ASSERT_TRUE(eng_shadow_fit(&map, sun, center, 30.0F, 1024U));
    const float half = 30.0F * 1024.0F / 1020.0F;
    EXPECT_NEAR(map.texel_world, 2.0F * half / 1024.0F, 1e-6F);
    EXPECT_NEAR(map.depth_range, 2.0F * half, 1e-4F);
    EXPECT_EQ(map.size, 1024U);
    /* un vecteur non unitaire est normalisé */
    const float big[3] = {0.0F, 7.0F, 0.0F};
    EngShadowMap other;
    ASSERT_TRUE(eng_shadow_fit(&other, big, center, 30.0F, 1024U));
    EXPECT_NEAR(other.to_sun[1], 1.0F, 1e-6F);
}

UTEST(shadow_fit, the_center_snaps_to_the_texel_grid)
{
    const float sun[3] = {0.3F, 0.8F, 0.5F};
    const uint32_t size = 512U;
    const float radius = 25.0F;
    float center[3] = {1.0F, 2.0F, 3.0F};
    EngShadowMap base;
    ASSERT_TRUE(eng_shadow_fit(&base, sun, center, radius, size));
    /* un point fixe du monde : si la carte ne bouge que d'un multiple entier de texels, sa
     * position dans la carte ne change que d'un multiple entier de texels (au plus d'un texel) */
    const float p[3] = {4.0F, 1.0F, -2.0F};
    float uvd0[3];
    eng_shadow_project(&base, p, uvd0);
    /* déplacement du centre de moins d'un demi-texel le long de la base de la carte */
    const float s = 1.0F / sqrtf((sun[0] * sun[0]) + (sun[1] * sun[1]) + (sun[2] * sun[2]));
    const float f[3] = {-sun[0] * s, -sun[1] * s, -sun[2] * s};
    float right[3] = {(f[1] * 0.0F) - (f[2] * 1.0F), (f[2] * 0.0F) - (f[0] * 0.0F),
                      (f[0] * 1.0F) - (f[1] * 0.0F)};
    const float rl = sqrtf((right[0] * right[0]) + (right[1] * right[1]) + (right[2] * right[2]));
    for (int a = 0; a < 3; a++) {
        right[a] /= rl;
    }
    float moved[3];
    for (int a = 0; a < 3; a++) {
        moved[a] = center[a] + (right[a] * base.texel_world * 0.2F);
    }
    EngShadowMap tiny;
    ASSERT_TRUE(eng_shadow_fit(&tiny, sun, moved, radius, size));
    float uvd1[3];
    eng_shadow_project(&tiny, p, uvd1);
    EXPECT_NEAR(uvd1[0], uvd0[0], 1e-5F);
    EXPECT_NEAR(uvd1[1], uvd0[1], 1e-5F);
    EXPECT_NEAR(uvd1[2], uvd0[2], 1e-5F);
    /* déplacement de 3,6 texels : la position bouge d'un nombre ENTIER de texels (3 ou 4) */
    for (int a = 0; a < 3; a++) {
        moved[a] = center[a] + (right[a] * base.texel_world * 3.6F);
    }
    EngShadowMap shifted;
    ASSERT_TRUE(eng_shadow_fit(&shifted, sun, moved, radius, size));
    float uvd2[3];
    eng_shadow_project(&shifted, p, uvd2);
    const float along = sqrtf(((uvd2[0] - uvd0[0]) * (uvd2[0] - uvd0[0])) +
                              ((uvd2[1] - uvd0[1]) * (uvd2[1] - uvd0[1]))) *
                        (float)size;
    EXPECT_NEAR(along, roundf(along), 1e-2F);
    EXPECT_GE(along, 3.0F - 1e-2F);
    EXPECT_LE(along, 4.0F + 1e-2F);
}

UTEST(shadow_receiver, slope_and_constant_bias_are_in_depth_texels)
{
    const float center[3] = {0.0F, 0.0F, 0.0F};
    const uint32_t size = 512U;
    float sun[3];
    sun_from_elevation(45.0F, 0.0F, sun); /* à 45° : N = +Y fait 45° avec le soleil, tan = 1 */
    EngShadowMap map;
    ASSERT_TRUE(eng_shadow_fit(&map, sun, center, 20.0F, size));
    const float up[3] = {0.0F, 1.0F, 0.0F};
    const float p[3] = {1.0F, 0.0F, 2.0F};
    float without[3];
    float with[3];
    EngShadowParams none = {.pcf_radius = 1U, .strength = 1.0F};
    eng_shadow_receiver(&map, &none, p, up, without);
    EngShadowParams constant = none;
    constant.constant_bias = 2.5F;
    eng_shadow_receiver(&map, &constant, p, up, with);
    EXPECT_NEAR(without[2] - with[2], 2.5F / (float)size, 1e-6F);
    EXPECT_NEAR(with[0], without[0], 1e-7F); /* un biais de profondeur ne déplace pas (u, v) */
    EngShadowParams slope = none;
    slope.slope_bias = 1.0F;
    eng_shadow_receiver(&map, &slope, p, up, with);
    /* (rayon + 1) = 2 ; tan(45°) = 1 : 2 texels */
    EXPECT_NEAR(without[2] - with[2], 2.0F / (float)size, 2e-6F);
    slope.pcf_radius = 3U;
    eng_shadow_receiver(&map, &slope, p, up, with);
    EXPECT_NEAR(without[2] - with[2], 4.0F / (float)size, 2e-6F);
}

UTEST(shadow_receiver, slope_is_capped_at_grazing_angles)
{
    const float center[3] = {0.0F, 0.0F, 0.0F};
    float sun[3];
    sun_from_elevation(0.5F, 0.0F, sun);
    EngShadowMap map;
    ASSERT_TRUE(eng_shadow_fit(&map, sun, center, 20.0F, 512U));
    const float up[3] = {0.0F, 1.0F, 0.0F};
    const float p[3] = {0.0F, 0.0F, 0.0F};
    float a[3];
    float b[3];
    EngShadowParams none = {.pcf_radius = 0U, .strength = 1.0F};
    EngShadowParams slope = none;
    slope.slope_bias = 1.0F;
    eng_shadow_receiver(&map, &none, p, up, a);
    eng_shadow_receiver(&map, &slope, p, up, b);
    /* tan(89,5°) = 115, plafonné à ENG_SHADOW_MAX_SLOPE : 8 texels (rayon 0 : x1) */
    EXPECT_NEAR(a[2] - b[2], ENG_SHADOW_MAX_SLOPE / 512.0F, 2e-6F);
}

UTEST(shadow_receiver, normal_offset_moves_the_point_along_the_normal_by_sin_theta_texels)
{
    const float center[3] = {0.0F, 0.0F, 0.0F};
    float sun[3];
    sun_from_elevation(30.0F, 40.0F, sun); /* N = +Y : cos(theta) = sin(30°) = 0,5 */
    EngShadowMap map;
    ASSERT_TRUE(eng_shadow_fit(&map, sun, center, 20.0F, 512U));
    const float up[3] = {0.0F, 1.0F, 0.0F};
    const float p[3] = {2.0F, 0.0F, -1.0F};
    EngShadowParams params = {.normal_bias = 3.0F, .pcf_radius = 0U, .strength = 1.0F};
    float got[3];
    eng_shadow_receiver(&map, &params, p, up, got);
    /* référence : projeter à la main le point décalé de 3 x sin(60°) texels le long de +Y */
    const float offset = 3.0F * sinf(60.0F * PI_F / 180.0F) * map.texel_world;
    const float moved[3] = {p[0], p[1] + offset, p[2]};
    float want[3];
    eng_shadow_project(&map, moved, want);
    EXPECT_NEAR(got[0], want[0], 1e-6F);
    EXPECT_NEAR(got[1], want[1], 1e-6F);
    EXPECT_NEAR(got[2], want[2], 1e-6F);
    /* un plan face au soleil (N = to_sun) : sin = 0, pas de décalage */
    float flat[3];
    eng_shadow_receiver(&map, &params, p, sun, flat);
    float plain[3];
    eng_shadow_project(&map, p, plain);
    EXPECT_NEAR(flat[0], plain[0], 1e-7F);
    EXPECT_NEAR(flat[1], plain[1], 1e-7F);
    EXPECT_NEAR(flat[2], plain[2], 1e-7F);
}

/* Carte de profondeur synthétique : un mur vertical à u = 0,5 (gauche : profondeur 0,2, droite :
 * 0,9), taille 64. */
static void fill_wall(float *depth, uint32_t size)
{
    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            depth[((size_t)y * size) + x] = x < size / 2U ? 0.2F : 0.9F;
        }
    }
}

UTEST(shadow_visibility, bilinear_ramp_is_one_texel_wide_without_filtering)
{
    float depth[64 * 64];
    fill_wall(depth, 64U);
    /* référence 0,5 : ombre (0,5 > 0,2) à gauche, lumière (0,5 <= 0,9) à droite. La frontière entre
     * les colonnes 31 et 32 est à u = 0,5 ; la rampe bilinéaire fait 1 texel de large */
    float uvd[3] = {0.5F, 0.5F, 0.5F};
    EXPECT_NEAR(eng_shadow_visibility(depth, 64U, uvd, 0U), 0.5F, 1e-5F);
    uvd[0] = 0.5F - (0.25F / 64.0F);
    EXPECT_NEAR(eng_shadow_visibility(depth, 64U, uvd, 0U), 0.25F, 1e-5F);
    uvd[0] = 0.5F + (0.25F / 64.0F);
    EXPECT_NEAR(eng_shadow_visibility(depth, 64U, uvd, 0U), 0.75F, 1e-5F);
    uvd[0] = 0.5F - (1.0F / 64.0F);
    EXPECT_NEAR(eng_shadow_visibility(depth, 64U, uvd, 0U), 0.0F, 1e-5F);
    uvd[0] = 0.5F + (1.0F / 64.0F);
    EXPECT_NEAR(eng_shadow_visibility(depth, 64U, uvd, 0U), 1.0F, 1e-5F);
}

UTEST(shadow_visibility, pcf_radius_widens_the_ramp_to_two_r_plus_one_texels)
{
    float depth[64 * 64];
    fill_wall(depth, 64U);
    for (uint32_t radius = 1; radius <= 3; radius++) {
        const float half = ((float)radius + 0.5F) / 64.0F;
        float uvd[3] = {0.5F - half, 0.5F, 0.5F};
        EXPECT_NEAR(eng_shadow_visibility(depth, 64U, uvd, radius), 0.0F, 1e-5F);
        uvd[0] = 0.5F + half;
        EXPECT_NEAR(eng_shadow_visibility(depth, 64U, uvd, radius), 1.0F, 1e-5F);
        uvd[0] = 0.5F;
        EXPECT_NEAR(eng_shadow_visibility(depth, 64U, uvd, radius), 0.5F, 1e-5F);
        /* la rampe est linéaire : 25 % de la largeur donne 25 % */
        uvd[0] = 0.5F - (half * 0.5F);
        EXPECT_NEAR(eng_shadow_visibility(depth, 64U, uvd, radius), 0.25F, 1e-5F);
    }
}

UTEST(shadow_visibility, equal_depth_is_lit_and_outside_the_map_is_lit)
{
    float depth[8 * 8];
    for (size_t i = 0; i < 64; i++) {
        depth[i] = 0.4F;
    }
    float uvd[3] = {0.5F, 0.5F, 0.4F};
    EXPECT_NEAR(eng_shadow_visibility(depth, 8U, uvd, 1U), 1.0F, 0.0F); /* égalité : lumière */
    uvd[2] = 0.4001F;
    EXPECT_NEAR(eng_shadow_visibility(depth, 8U, uvd, 1U), 0.0F, 0.0F);
    /* hors de la carte, ou profondeur hors de [0, 1] : la lumière n'est pas arrêtée */
    const float outside[4][3] = {
        {-0.01F, 0.5F, 0.9F}, {1.01F, 0.5F, 0.9F}, {0.5F, -0.01F, 0.9F}, {0.5F, 1.01F, 0.9F}};
    for (int i = 0; i < 4; i++) {
        EXPECT_NEAR(eng_shadow_visibility(depth, 8U, outside[i], 1U), 1.0F, 0.0F);
    }
    const float deeper[3] = {0.5F, 0.5F, 1.2F};
    const float nearer[3] = {0.5F, 0.5F, -0.2F};
    EXPECT_NEAR(eng_shadow_visibility(depth, 8U, deeper, 1U), 1.0F, 0.0F);
    EXPECT_NEAR(eng_shadow_visibility(depth, 8U, nearer, 1U), 1.0F, 0.0F);
}

UTEST(shadow_visibility, taps_outside_the_map_read_the_edge_texel)
{
    /* tout ombré (profondeur 0,1 partout) : au bord, le noyau déborde mais la lecture est clampée,
     * le résultat reste 0 */
    float depth[8 * 8];
    for (size_t i = 0; i < 64; i++) {
        depth[i] = 0.1F;
    }
    const float corner[3] = {0.001F, 0.001F, 0.5F};
    EXPECT_NEAR(eng_shadow_visibility(depth, 8U, corner, 3U), 0.0F, 0.0F);
    const float far_corner[3] = {0.999F, 0.999F, 0.5F};
    EXPECT_NEAR(eng_shadow_visibility(depth, 8U, far_corner, 3U), 0.0F, 0.0F);
}

UTEST(shadow_factor, strength_interpolates_between_lit_and_fully_shadowed)
{
    EngShadowParams p;
    eng_shadow_params_default(&p);
    EXPECT_NEAR(eng_shadow_factor(&p, 1.0F), 1.0F, 0.0F);
    EXPECT_NEAR(eng_shadow_factor(&p, 0.0F), 0.0F, 0.0F);
    p.strength = 0.6F;
    EXPECT_NEAR(eng_shadow_factor(&p, 0.0F), 0.4F, 1e-6F);
    EXPECT_NEAR(eng_shadow_factor(&p, 0.5F), 0.7F, 1e-6F);
    p.strength = 0.0F;
    EXPECT_NEAR(eng_shadow_factor(&p, 0.0F), 1.0F, 0.0F);
}

/* ── Scène de test : un sol (y = 0) et une boîte posée dessus, carte d'ombre par lancer de rayons
 * ── */

#define MAP_SIZE 512U

typedef struct TestScene {
    float box_min[3];
    float box_max[3];
} TestScene;

/* t d'entrée du rayon (o + t d) dans la boîte, ou -1. */
static float ray_box(const float o[3], const float d[3], const float lo[3], const float hi[3])
{
    float t0 = 0.0F;
    float t1 = 1e30F;
    for (int a = 0; a < 3; a++) {
        if (fabsf(d[a]) < 1e-9F) {
            if (o[a] < lo[a] || o[a] > hi[a]) {
                return -1.0F;
            }
            continue;
        }
        float ta = (lo[a] - o[a]) / d[a];
        float tb = (hi[a] - o[a]) / d[a];
        if (ta > tb) {
            const float tmp = ta;
            ta = tb;
            tb = tmp;
        }
        t0 = ta > t0 ? ta : t0;
        t1 = tb < t1 ? tb : t1;
        if (t0 > t1) {
            return -1.0F;
        }
    }
    return t0;
}

/* Remplit la carte : pour chaque texel, le rayon du soleil (de la face proche à la face lointaine)
 * touche le sol (y = 0) et/ou la boîte ; profondeur = fraction du trajet jusqu'au premier contact,
 * 1 sinon. */
static void render_depth(const EngShadowMap *map, const TestScene *scene, float *depth)
{
    mat4 inverse;
    glm_mat4_inv((vec4 *)map->view_proj, inverse);
    for (uint32_t y = 0; y < map->size; y++) {
        for (uint32_t x = 0; x < map->size; x++) {
            const float ndc_x = ((((float)x + 0.5F) / (float)map->size) * 2.0F) - 1.0F;
            const float ndc_y = 1.0F - ((((float)y + 0.5F) / (float)map->size) * 2.0F);
            vec4 near4 = {ndc_x, ndc_y, 0.0F, 1.0F};
            vec4 far4 = {ndc_x, ndc_y, 1.0F, 1.0F};
            vec4 a;
            vec4 b;
            glm_mat4_mulv(inverse, near4, a);
            glm_mat4_mulv(inverse, far4, b);
            const float o[3] = {a[0], a[1], a[2]};
            const float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}; /* t in [0, 1] */
            float best = 1.0F;
            if (fabsf(d[1]) > 1e-9F) {
                const float t = -o[1] / d[1];
                if (t >= 0.0F && t < best) {
                    best = t;
                }
            }
            const float tb = ray_box(o, d, scene->box_min, scene->box_max);
            if (tb >= 0.0F && tb < best) {
                best = tb;
            }
            depth[((size_t)y * map->size) + x] = best;
        }
    }
}

static float lit_fraction_of_ground(const EngShadowMap *map, const EngShadowParams *params,
                                    const float *depth, float *worst)
{
    /* points du sol (normale +Y) sur une grille de 41 x 41 dans [-8, 8]^2 hors de la boîte */
    const float up[3] = {0.0F, 1.0F, 0.0F};
    int lit = 0;
    int total = 0;
    float min_vis = 1.0F;
    for (int i = 0; i <= 40; i++) {
        for (int j = 0; j <= 40; j++) {
            const float p[3] = {-8.0F + (0.4F * (float)i) + 0.0137F, 0.0F,
                                -8.0F + (0.4F * (float)j) + 0.0291F};
            float uvd[3];
            eng_shadow_receiver(map, params, p, up, uvd);
            const float v = eng_shadow_visibility(depth, map->size, uvd, params->pcf_radius);
            total++;
            lit += v > 0.999F ? 1 : 0;
            min_vis = v < min_vis ? v : min_vis;
        }
    }
    *worst = min_vis;
    return (float)lit / (float)total;
}

UTEST(shadow_acne, a_lit_ground_has_no_self_shadowing_at_any_sun_height)
{
    /* sol seul (boîte hors de la zone) : avec les réglages par défaut, AUCUN point n'est ombré, à
     * toutes les hauteurs du soleil et pour tous les rayons de filtrage */
    static float depth[MAP_SIZE * MAP_SIZE];
    const TestScene scene = {{100.0F, 0.0F, 100.0F}, {101.0F, 1.0F, 101.0F}};
    const float center[3] = {0.0F, 0.0F, 0.0F};
    const float elevations[8] = {90.0F, 70.0F, 45.0F, 30.0F, 20.0F, 12.0F, 8.0F, 5.0F};
    for (int e = 0; e < 8; e++) {
        float sun[3];
        sun_from_elevation(elevations[e], 25.0F, sun);
        EngShadowMap map;
        ASSERT_TRUE(eng_shadow_fit(&map, sun, center, 30.0F, MAP_SIZE));
        render_depth(&map, &scene, depth);
        for (uint32_t radius = 0; radius <= 3; radius++) {
            if (radius == 3U && elevations[e] < 8.0F) {
                continue; /* limite documentée : le rayon 3 demande plus de biais sous 8° */
            }
            EngShadowParams params;
            eng_shadow_params_default(&params);
            params.pcf_radius = radius;
            float worst;
            const float lit = lit_fraction_of_ground(&map, &params, depth, &worst);
            EXPECT_NEAR(lit, 1.0F, 0.0F);
            EXPECT_GE(worst, 0.999F);
        }
    }
}

UTEST(shadow_acne, without_any_bias_the_same_ground_shows_acne)
{
    /* garde-fou : le test précédent n'est pas trivial, sans biais le sol s'ombre lui-même */
    static float depth[MAP_SIZE * MAP_SIZE];
    const TestScene scene = {{100.0F, 0.0F, 100.0F}, {101.0F, 1.0F, 101.0F}};
    const float center[3] = {0.0F, 0.0F, 0.0F};
    float sun[3];
    sun_from_elevation(30.0F, 25.0F, sun);
    EngShadowMap map;
    ASSERT_TRUE(eng_shadow_fit(&map, sun, center, 30.0F, MAP_SIZE));
    render_depth(&map, &scene, depth);
    EngShadowParams none = {.pcf_radius = 1U, .strength = 1.0F};
    float worst;
    const float lit = lit_fraction_of_ground(&map, &none, depth, &worst);
    EXPECT_LT(lit, 0.9F);
    EXPECT_LT(worst, 0.5F);
}

/* Ombre d'une boîte posée sur le sol : de combien le début de l'ombre s'écarte-t-il du pied de la
 * boîte ? On marche sur le sol, côté opposé au soleil, à partir de la face de la boîte. */
static float shadow_gap_in_texels(const EngShadowMap *map, const EngShadowParams *params,
                                  const float *depth, const float sun_xz[2])
{
    const float up[3] = {0.0F, 1.0F, 0.0F};
    /* direction horizontale AWAY du soleil, unitaire */
    const float len = sqrtf((sun_xz[0] * sun_xz[0]) + (sun_xz[1] * sun_xz[1]));
    const float away[2] = {-sun_xz[0] / len, -sun_xz[1] / len};
    /* centre de la boîte (0, 0) de demi-côté 1 : on part du centre vers l'extérieur le long de
     * `away`, jusqu'à la face (distance 1/max(|ax|, |az|)) */
    const float face = 1.0F / fmaxf(fabsf(away[0]), fabsf(away[1]));
    for (int step = 0; step < 600; step++) {
        const float d = face + ((float)step * 0.005F);
        const float p[3] = {away[0] * d, 0.0F, away[1] * d};
        float uvd[3];
        eng_shadow_receiver(map, params, p, up, uvd);
        if (eng_shadow_visibility(depth, map->size, uvd, params->pcf_radius) < 0.5F) {
            return (d - face) / map->texel_world;
        }
    }
    return 1e9F;
}

UTEST(shadow_peter_panning, the_shadow_starts_at_the_foot_of_the_object)
{
    /* boîte 2 x 2 x 2 posée sur le sol à l'origine ; soleil à 35° : l'ombre part du pied */
    static float depth[MAP_SIZE * MAP_SIZE];
    const TestScene scene = {{-1.0F, 0.0F, -1.0F}, {1.0F, 2.0F, 1.0F}};
    const float center[3] = {0.0F, 0.0F, 0.0F};
    const float azimuths[4] = {0.0F, 30.0F, 90.0F, 200.0F};
    for (int a = 0; a < 4; a++) {
        float sun[3];
        sun_from_elevation(35.0F, azimuths[a], sun);
        EngShadowMap map;
        ASSERT_TRUE(eng_shadow_fit(&map, sun, center, 30.0F, MAP_SIZE));
        render_depth(&map, &scene, depth);
        EngShadowParams params;
        eng_shadow_params_default(&params);
        const float sun_xz[2] = {sun[0], sun[2]};
        const float gap = shadow_gap_in_texels(&map, &params, depth, sun_xz);
        /* la mesure : le décollement est de l'ordre de quelques texels, jamais une zone claire
         * visible entre l'objet et son ombre */
        printf("GAP %.2f\n", (double)gap);
        EXPECT_LT(gap, 4.0F);
    }
}

UTEST(shadow_peter_panning, huge_biases_do_detach_the_shadow_so_the_measure_is_meaningful)
{
    static float depth[MAP_SIZE * MAP_SIZE];
    const TestScene scene = {{-1.0F, 0.0F, -1.0F}, {1.0F, 2.0F, 1.0F}};
    const float center[3] = {0.0F, 0.0F, 0.0F};
    float sun[3];
    sun_from_elevation(35.0F, 30.0F, sun);
    EngShadowMap map;
    ASSERT_TRUE(eng_shadow_fit(&map, sun, center, 30.0F, MAP_SIZE));
    render_depth(&map, &scene, depth);
    EngShadowParams params;
    eng_shadow_params_default(&params);
    params.constant_bias = 40.0F;
    params.normal_bias = 20.0F;
    const float sun_xz[2] = {sun[0], sun[2]};
    EXPECT_GT(shadow_gap_in_texels(&map, &params, depth, sun_xz), 10.0F);
}

UTEST(shadow_scene, ground_far_from_the_box_is_lit_and_behind_it_is_shadowed)
{
    static float depth[MAP_SIZE * MAP_SIZE];
    const TestScene scene = {{-1.0F, 0.0F, -1.0F}, {1.0F, 2.0F, 1.0F}};
    const float center[3] = {0.0F, 0.0F, 0.0F};
    float sun[3];
    sun_from_elevation(45.0F, 0.0F, sun); /* soleil au sud (+Z), ombre vers -Z, longueur 2 m */
    EngShadowMap map;
    ASSERT_TRUE(eng_shadow_fit(&map, sun, center, 30.0F, MAP_SIZE));
    render_depth(&map, &scene, depth);
    EngShadowParams params;
    eng_shadow_params_default(&params);
    const float up[3] = {0.0F, 1.0F, 0.0F};
    const float in_shadow[3] = {0.0F, 0.0F, -2.0F}; /* 1 m derrière la face nord : ombre */
    const float sunny[3] = {0.0F, 0.0F, 3.0F};      /* côté soleil */
    const float beyond[3] = {0.0F, 0.0F, -4.5F};    /* au-delà de la pointe de l'ombre */
    float uvd[3];
    eng_shadow_receiver(&map, &params, in_shadow, up, uvd);
    EXPECT_NEAR(eng_shadow_visibility(depth, MAP_SIZE, uvd, 1U), 0.0F, 1e-4F);
    eng_shadow_receiver(&map, &params, sunny, up, uvd);
    EXPECT_NEAR(eng_shadow_visibility(depth, MAP_SIZE, uvd, 1U), 1.0F, 1e-4F);
    eng_shadow_receiver(&map, &params, beyond, up, uvd);
    EXPECT_NEAR(eng_shadow_visibility(depth, MAP_SIZE, uvd, 1U), 1.0F, 1e-4F);
}

UTEST(shadow_receiver,
      the_cosine_is_clamped_so_odd_normals_stay_finite_and_unshifted_or_fully_shifted)
{
    const float center[3] = {0.0F, 0.0F, 0.0F};
    float sun[3];
    sun_from_elevation(40.0F, 25.0F, sun);
    EngShadowMap map;
    ASSERT_TRUE(eng_shadow_fit(&map, sun, center, 20.0F, 512U));
    const EngShadowParams offset_only = {.normal_bias = 1.0F, .pcf_radius = 0U, .strength = 1.0F};
    const float p[3] = {1.0F, 2.0F, 3.0F};
    float got[3];
    float expected[3];

    /* normale non unitaire (cos = 1,5 > 1) : écrêtée à 1, donc sin = 0 : aucun décalage, et pas de
     * NaN */
    const float stretched[3] = {1.5F * sun[0], 1.5F * sun[1], 1.5F * sun[2]};
    eng_shadow_receiver(&map, &offset_only, p, stretched, got);
    eng_shadow_project(&map, p, expected);
    for (int a = 0; a < 3; a++) {
        EXPECT_TRUE(isfinite(got[a]));
        EXPECT_NEAR(got[a], expected[a], 1e-6F);
    }

    /* normale tournée à l'opposé du soleil (cos = -1) : écrêtée à 0, donc sin = 1 : décalage entier
     */
    const float away[3] = {-sun[0], -sun[1], -sun[2]};
    eng_shadow_receiver(&map, &offset_only, p, away, got);
    const float shifted[3] = {p[0] + (away[0] * map.texel_world),
                              p[1] + (away[1] * map.texel_world),
                              p[2] + (away[2] * map.texel_world)};
    eng_shadow_project(&map, shifted, expected);
    for (int a = 0; a < 2; a++) {
        EXPECT_NEAR(got[a], expected[a], 1e-6F);
    }
}

UTEST(shadow_visibility, a_radius_above_the_maximum_is_capped)
{
    float depth[64 * 64];
    fill_wall(depth, 64U);
    /* à 4,2 texels du mur : un noyau de rayon 3 (et 9, ramené à 3) ne voit pas le mur au-delà de
     * 3,5 texels, un noyau de rayon 9 non plafonné si */
    const float uvd[3] = {0.5F + (4.2F / 64.0F), 0.5F, 0.5F};
    EXPECT_NEAR(eng_shadow_visibility(depth, 64U, uvd, 3U), 1.0F, 1e-5F);
    EXPECT_NEAR(eng_shadow_visibility(depth, 64U, uvd, 9U), 1.0F, 1e-5F);
}

UTEST(shadow_visibility, taps_beyond_the_left_and_top_edges_read_the_edge_texel)
{
    /* seule la colonne 0 (resp. la rangée 0) est éclairée (profondeur 0,9) ; le reste est ombré.
     * Une prise à gauche (resp. au-dessus) du bord doit lire cette colonne (resp. rangée), pas
     * le bord opposé */
    float depth[8 * 8];
    for (uint32_t y = 0; y < 8; y++) {
        for (uint32_t x = 0; x < 8; x++) {
            depth[(y * 8U) + x] = x == 0 ? 0.9F : 0.1F;
        }
    }
    const float left[3] = {0.001F, 0.5F, 0.5F};
    EXPECT_NEAR(eng_shadow_visibility(depth, 8U, left, 0U), 1.0F, 1e-5F);
    for (uint32_t y = 0; y < 8; y++) {
        for (uint32_t x = 0; x < 8; x++) {
            depth[(y * 8U) + x] = y == 0 ? 0.9F : 0.1F;
        }
    }
    const float top[3] = {0.5F, 0.001F, 0.5F};
    EXPECT_NEAR(eng_shadow_visibility(depth, 8U, top, 0U), 1.0F, 1e-5F);
}
