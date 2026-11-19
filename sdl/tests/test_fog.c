#include <math.h>
#include <stdbool.h>

#include "engine/atmosphere.h"
#include "engine/fog.h"
#include "utest.h"

static bool same3(const float a[3], const float b[3])
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

/* Intégrale numérique (point milieu) de la densité le long du segment : la référence de la formule
 * analytique. */
static float numeric_depth(const EngFog *fog, const float a[3], const float b[3])
{
    const int steps = 20000;
    const float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const float length = sqrtf((d[0] * d[0]) + (d[1] * d[1]) + (d[2] * d[2]));
    double sum = 0.0;
    for (int i = 0; i < steps; i++) {
        const double t = ((double)i + 0.5) / (double)steps;
        const double y = (double)a[1] + (t * (double)d[1]);
        sum += (double)fog->density *
               exp(-(double)fog->height_falloff * (y - (double)fog->base_height));
    }
    return (float)(sum * (double)length / (double)steps);
}

UTEST(eng_fog, defaults_are_valid_and_invalid_values_are_rejected)
{
    EngFog fog;
    eng_fog_default(&fog);
    EXPECT_TRUE(eng_fog_valid(&fog));
    EXPECT_FALSE(eng_fog_valid(NULL));
    EngFog bad = fog;
    bad.density = -0.1F;
    EXPECT_FALSE(eng_fog_valid(&bad));
    bad = fog;
    bad.density = 1.5F;
    EXPECT_FALSE(eng_fog_valid(&bad));
    bad = fog;
    bad.height_falloff = NAN;
    EXPECT_FALSE(eng_fog_valid(&bad));
    bad = fog;
    bad.height_falloff = -1.0F;
    EXPECT_FALSE(eng_fog_valid(&bad));
    bad = fog;
    bad.base_height = INFINITY;
    EXPECT_FALSE(eng_fog_valid(&bad));
    bad = fog;
    bad.max_opacity = 1.1F;
    EXPECT_FALSE(eng_fog_valid(&bad));
    bad = fog;
    bad.max_opacity = -0.1F;
    EXPECT_FALSE(eng_fog_valid(&bad));
}

UTEST(eng_fog, the_bounds_of_every_parameter_are_valid)
{
    EngFog fog;
    for (int field = 0; field < 3; field++) {
        for (int end = 0; end < 2; end++) {
            eng_fog_default(&fog);
            float *value =
                field == 0 ? &fog.density : (field == 1 ? &fog.height_falloff : &fog.max_opacity);
            *value = end == 0 ? 0.0F : 1.0F;
            EXPECT_TRUE(eng_fog_valid(&fog));
        }
    }
}

UTEST(eng_fog, uniform_density_gives_density_times_distance)
{
    EngFog fog;
    eng_fog_default(&fog);
    fog.density = 0.002F;
    fog.height_falloff = 0.0F;
    const float cam[3] = {0, 10, 0};
    const float p[3] = {300, 10, 400};
    EXPECT_NEAR(eng_fog_optical_depth(&fog, cam, p), 0.002F * 500.0F, 1e-4F);
    const float q[3] = {30, 50, 40}; /* distance sqrt(900 + 1600 + 1600) */
    EXPECT_NEAR(eng_fog_optical_depth(&fog, cam, q), 0.002F * sqrtf(4100.0F), 1e-4F);
}

UTEST(eng_fog, the_analytic_depth_matches_numerical_integration)
{
    EngFog fog;
    eng_fog_default(&fog);
    fog.density = 0.01F;
    fog.height_falloff = 0.01F;
    fog.base_height = 20.0F;
    const float cams[4][3] = {{0, 100, 0}, {0, -30, 0}, {5, 20, 5}, {0, 400, 0}};
    const float pts[4][3] = {{200, 0, 300}, {-100, 250, 40}, {5, 20, 905}, {0, 0, 1}};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            const float analytic = eng_fog_optical_depth(&fog, cams[i], pts[j]);
            const float reference = numeric_depth(&fog, cams[i], pts[j]);
            EXPECT_NEAR(analytic, reference, 2e-4F + (reference * 1e-3F));
        }
    }
}

UTEST(eng_fog, the_depth_is_symmetric_and_near_zero_slope_is_continuous)
{
    EngFog fog;
    eng_fog_default(&fog);
    fog.height_falloff = 0.02F;
    const float a[3] = {0, 100, 0};
    const float b[3] = {300, 20, 200};
    EXPECT_NEAR(eng_fog_optical_depth(&fog, a, b), eng_fog_optical_depth(&fog, b, a), 1e-4F);
    /* autour de x = falloff * dy = 0 la formule change de branche : pas de saut */
    const float c[3] = {0, 100, 0};
    /* x = falloff * dy = +-9e-5, 0 : sous 1e-4, la branche en série ; elle doit rejoindre
     * l'intégrale à 2e-5 près en valeur relative (un signe faux se verrait à 9e-5) */
    fog.density = 0.5F;
    for (int i = -1; i <= 1; i++) {
        const float p[3] = {100, 100 + ((float)i * 0.0045F), 0};
        const float v = eng_fog_optical_depth(&fog, c, p);
        const float reference = numeric_depth(&fog, c, p);
        EXPECT_NEAR(v, reference, 2e-5F * reference);
    }
}

UTEST(eng_fog, a_viewer_above_the_layer_sees_less_fog_than_one_inside_it)
{
    EngFog fog;
    eng_fog_default(&fog);
    fog.density = 0.004F;
    fog.height_falloff = 0.02F;
    const float ground[3] = {0, 0, 300};
    const float inside[3] = {0, 0, 0};  /* au niveau du sol : même épaisseur horizontale 300 m */
    const float above[3] = {0, 300, 0}; /* à 300 m d'altitude : distance 424 m mais air raréfié */
    const float horizontal = eng_fog_optical_depth(&fog, inside, ground);
    const float from_above = eng_fog_optical_depth(&fog, above, ground);
    EXPECT_GT(horizontal, 0.0F);
    EXPECT_LT(from_above, horizontal);
}

UTEST(eng_fog, the_factor_grows_with_distance_and_stays_below_the_cap)
{
    EngFog fog;
    eng_fog_default(&fog);
    fog.max_opacity = 0.8F;
    const float cam[3] = {0, 5, 0};
    float last = -1.0F;
    for (int i = 0; i <= 40; i++) {
        const float p[3] = {0, 5, (float)i * 100.0F};
        const float f = eng_fog_factor(&fog, cam, p);
        EXPECT_GE(f, last);
        EXPECT_LE(f, 0.8F + 1e-6F);
        last = f;
    }
    EXPECT_NEAR(last, 0.8F, 1e-3F); /* à 4 km, le plafond est atteint */
    const float origin[3] = {0, 5, 0};
    EXPECT_NEAR(eng_fog_factor(&fog, cam, origin), 0.0F, 1e-7F);
    /* valeur de référence : densité 0,003 uniforme à 5 m, falloff 0,004 : 1 km, f(1000) */
    EngFog d;
    eng_fog_default(&d);
    const float far_point[3] = {0, 5, 1000};
    const float expected = 1.0F - expf(-numeric_depth(&d, cam, far_point));
    EXPECT_NEAR(eng_fog_factor(&d, cam, far_point), expected, 1e-3F);
}

UTEST(eng_fog, zero_density_zero_opacity_and_invalid_inputs_give_no_fog)
{
    EngFog fog;
    eng_fog_default(&fog);
    const float cam[3] = {0, 0, 0};
    const float p[3] = {1000, 0, 0};
    fog.density = 0.0F;
    EXPECT_EQ(eng_fog_factor(&fog, cam, p), 0.0F);
    eng_fog_default(&fog);
    fog.max_opacity = 0.0F;
    EXPECT_EQ(eng_fog_factor(&fog, cam, p), 0.0F);
    eng_fog_default(&fog);
    const float nan_point[3] = {NAN, 0, 0};
    EXPECT_EQ(eng_fog_optical_depth(&fog, cam, nan_point), 0.0F);
    EXPECT_EQ(eng_fog_optical_depth(&fog, nan_point, p), 0.0F);
    EXPECT_EQ(eng_fog_factor(NULL, cam, p), 0.0F);
    /* des altitudes extrêmes ne débordent pas en NaN / infini */
    const float deep[3] = {0, -1.0e6F, 0};
    const float high[3] = {0, 1.0e6F, 0};
    fog.height_falloff = 1.0F;
    EXPECT_TRUE(isfinite(eng_fog_optical_depth(&fog, deep, high)));
    EXPECT_TRUE(isfinite(eng_fog_optical_depth(&fog, high, deep)));
}

UTEST(eng_fog, the_horizon_colour_follows_the_azimuth_and_equals_the_sky_at_the_horizon)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    const float sun[3] = {0.0F, 0.2588190F, 0.9659258F}; /* 15 deg, vers +Z */
    const float toward[3] = {0, 0, 1};
    const float away[3] = {0, 0, -1};
    float h_toward[3];
    float h_away[3];
    ASSERT_TRUE(eng_atmosphere_horizon(&a, sun, toward, h_toward));
    ASSERT_TRUE(eng_atmosphere_horizon(&a, sun, away, h_away));
    EXPECT_GT(h_toward[0] + h_toward[1] + h_toward[2], h_away[0] + h_away[1] + h_away[2]);
    /* c'est le ciel dessiné juste à l'horizon */
    float sky[3];
    ASSERT_TRUE(eng_atmosphere_sky(&a, sun, toward, sky));
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(h_toward[c], sky[c], 1e-6F);
    }
    /* la composante verticale de la direction est ignorée */
    const float tilted[3] = {0, 0.7F, 1};
    float h_tilted[3];
    ASSERT_TRUE(eng_atmosphere_horizon(&a, sun, tilted, h_tilted));
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(h_tilted[c], h_toward[c], 1e-6F);
    }
    /* direction verticale ou nulle : un azimut de repli, valeurs finies */
    const float up[3] = {0, 1, 0};
    float h_up[3];
    ASSERT_TRUE(eng_atmosphere_horizon(&a, sun, up, h_up));
    EXPECT_TRUE(isfinite(h_up[0]) && isfinite(h_up[1]) && isfinite(h_up[2]));
    /* entrées invalides : false et noir */
    const float bad[3] = {NAN, 0, 1};
    float out[3] = {9, 9, 9};
    EXPECT_FALSE(eng_atmosphere_horizon(&a, sun, bad, out));
    EXPECT_EQ(out[0], 0.0F);
    const float zero_sun[3] = {0, 0, 0};
    EXPECT_FALSE(eng_atmosphere_horizon(&a, zero_sun, toward, out));
    EXPECT_FALSE(eng_atmosphere_horizon(NULL, sun, toward, out));
}

UTEST(eng_fog, apply_blends_towards_the_sky_colour_and_far_points_become_the_sky)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    EngFog fog;
    eng_fog_default(&fog);
    const float sun[3] = {0.3F, 0.6F, 0.7416198F};
    const float cam[3] = {700, 50, -900};
    const float lit[3] = {0.1F, 0.4F, 0.05F};
    const float scale = 0.7F;
    float out[3];
    /* point collé à la caméra : pratiquement pas de brouillard */
    const float near_point[3] = {700.01F, 50, -900};
    ASSERT_TRUE(eng_fog_apply(&fog, &a, sun, scale, cam, near_point, lit, out));
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(out[c], lit[c], 1e-3F);
    }
    /* point très lointain : la couleur du ciel à l'horizon x l'échelle */
    const float far_point[3] = {30700, 50, 39100};
    ASSERT_TRUE(eng_fog_apply(&fog, &a, sun, scale, cam, far_point, lit, out));
    float horizon[3];
    const float dir[3] = {30000, 0, 40000}; /* far_point - cam */
    ASSERT_TRUE(eng_atmosphere_horizon(&a, sun, dir, horizon));
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(out[c], horizon[c] * scale, 1e-4F);
    }
    /* à mi-chemin : strictement entre les deux, canal par canal */
    fog.max_opacity = 0.5F;
    ASSERT_TRUE(eng_fog_apply(&fog, &a, sun, scale, cam, far_point, lit, out));
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(out[c], 0.5F * (lit[c] + (horizon[c] * scale)), 1e-4F);
    }
}

UTEST(eng_fog, apply_with_an_invalid_input_returns_the_lit_colour_unchanged)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    EngFog fog;
    eng_fog_default(&fog);
    const float sun[3] = {0, 1, 0};
    const float cam[3] = {0, 0, 0};
    const float p[3] = {500, 0, 500};
    const float lit[3] = {0.2F, 0.3F, 0.4F};
    float out[3];
    /* échelle 0 : valide, le brouillard devient noir */
    EXPECT_TRUE(eng_fog_apply(&fog, &a, sun, 0.0F, cam, p, lit, out));
    float packed[ENG_FOG_UNIFORM_FLOATS];
    eng_fog_pack(&fog, &a, 0.0F, packed);
    EXPECT_GT(packed[0], 0.0F);
    EngFog bad = fog;
    bad.density = NAN;
    EXPECT_FALSE(eng_fog_apply(&bad, &a, sun, 1.0F, cam, p, lit, out));
    EXPECT_TRUE(same3(out, lit));
    EXPECT_FALSE(eng_fog_apply(&fog, &a, sun, -1.0F, cam, p, lit, out));
    EXPECT_TRUE(same3(out, lit));
    EXPECT_FALSE(eng_fog_apply(&fog, &a, sun, NAN, cam, p, lit, out));
    EXPECT_TRUE(same3(out, lit));
    const float zero_sun[3] = {0, 0, 0};
    EXPECT_FALSE(eng_fog_apply(&fog, &a, zero_sun, 1.0F, cam, p, lit, out));
    EXPECT_TRUE(same3(out, lit));
    EXPECT_FALSE(eng_fog_apply(&fog, NULL, sun, 1.0F, cam, p, lit, out));
    EXPECT_TRUE(same3(out, lit));
}

UTEST(eng_fog, pack_lays_out_the_sixteen_floats_the_shader_reads)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    EngFog fog = {
        .density = 0.01F, .height_falloff = 0.02F, .base_height = 33.0F, .max_opacity = 0.9F};
    float out[ENG_FOG_UNIFORM_FLOATS];
    eng_fog_pack(&fog, &a, 0.6F, out);
    EXPECT_EQ(out[0], 0.01F);
    EXPECT_EQ(out[1], 0.02F);
    EXPECT_EQ(out[2], 33.0F);
    EXPECT_EQ(out[3], 0.9F);
    for (int c = 0; c < 3; c++) {
        EXPECT_EQ(out[4 + c], a.rayleigh[c]);
        EXPECT_EQ(out[8 + c], a.night[c]);
    }
    EXPECT_EQ(out[7], a.mie);
    EXPECT_EQ(out[11], a.irradiance);
    EXPECT_EQ(out[12], a.mie_g);
    EXPECT_EQ(out[13], a.path_limit);
    EXPECT_EQ(out[14], a.sun_path_limit);
    EXPECT_EQ(out[15], 0.6F);
    eng_fog_pack(&fog, &a, -1.0F, out);
    for (int i = 0; i < (int)ENG_FOG_UNIFORM_FLOATS; i++) {
        EXPECT_EQ(out[i], 0.0F);
    }
    eng_fog_pack(NULL, &a, 1.0F, out);
    EXPECT_EQ(out[0], 0.0F);
    eng_fog_pack(&fog, NULL, 1.0F, out);
    EXPECT_EQ(out[3], 0.0F);
}
