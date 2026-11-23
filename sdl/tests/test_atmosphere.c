#include <math.h>
#include <string.h>

#include "engine/atmosphere.h"
#include "utest.h"

/* Ciel atmosphérique (M3-05), côté CPU. Convention : azimut 0 = vers +Z, 90 = vers +X
 * (engine/lighting.h) ; hauteur au-dessus de l'horizon en degrés. */

static void dir(float elevation_deg, float azimuth_deg, float out[3])
{
    const float e = elevation_deg * (3.14159265F / 180.0F);
    const float a = azimuth_deg * (3.14159265F / 180.0F);
    out[0] = cosf(e) * sinf(a);
    out[1] = sinf(e);
    out[2] = cosf(e) * cosf(a);
}

static float luma(const float c[3])
{
    return (0.2126F * c[0]) + (0.7152F * c[1]) + (0.0722F * c[2]);
}

static void sky(float sun_el, float sun_az, float view_el, float view_az, float rgb[3])
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    float s[3];
    float v[3];
    dir(sun_el, sun_az, s);
    dir(view_el, view_az, v);
    if (!eng_atmosphere_sky(&a, s, v, rgb)) {
        rgb[0] = rgb[1] = rgb[2] = -1.0F; /* les tests qui suivent échouent alors */
    }
}

static bool near_rel(const float got[3], const float want[3], float tolerance)
{
    for (int c = 0; c < 3; c++) {
        if (fabsf(got[c] - want[c]) > tolerance * fabsf(want[c]) + 1e-5F) {
            return false;
        }
    }
    return true;
}

UTEST(eng_atmosphere, the_defaults_are_valid_and_every_field_is_checked)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    EXPECT_TRUE(eng_atmosphere_valid(&a));
    EXPECT_FALSE(eng_atmosphere_valid(NULL));
    float *const fields[] = {&a.rayleigh[0],
                             &a.rayleigh[1],
                             &a.rayleigh[2],
                             &a.mie,
                             &a.mie_g,
                             &a.irradiance,
                             &a.sun_radiance,
                             &a.sun_radius_deg,
                             &a.path_limit,
                             &a.sun_path_limit,
                             &a.ground_albedo[0],
                             &a.ground_albedo[1],
                             &a.ground_albedo[2],
                             &a.night[0],
                             &a.night[1],
                             &a.night[2]};
    const float bad[] = {NAN, INFINITY, -INFINITY, -1.0F, 1e9F};
    for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++) {
        for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
            EngAtmosphere b = a;
            float *field = (float *)((char *)&b + ((char *)fields[i] - (char *)&a));
            *field = bad[k];
            EXPECT_FALSE(eng_atmosphere_valid(&b));
        }
    }
    /* bornes précises */
    EngAtmosphere b = a;
    b.mie_g = 0.999F;
    EXPECT_FALSE(eng_atmosphere_valid(&b));
    b.mie_g = -0.999F;
    EXPECT_FALSE(eng_atmosphere_valid(&b));
    b.mie_g = 0.0F;
    EXPECT_TRUE(eng_atmosphere_valid(&b));
    b = a;
    b.sun_radius_deg = 0.0F;
    EXPECT_FALSE(eng_atmosphere_valid(&b));
    b.sun_radius_deg = 45.0F;
    EXPECT_TRUE(eng_atmosphere_valid(&b));
    b.sun_radius_deg = 45.1F;
    EXPECT_FALSE(eng_atmosphere_valid(&b));
    b = a;
    b.path_limit = 0.99F;
    EXPECT_FALSE(eng_atmosphere_valid(&b));
    b.path_limit = 1.0F;
    EXPECT_TRUE(eng_atmosphere_valid(&b));
    b = a;
    b.sun_path_limit = 0.99F;
    EXPECT_FALSE(eng_atmosphere_valid(&b));
    b = a;
    b.ground_albedo[1] = 1.0F;
    EXPECT_TRUE(eng_atmosphere_valid(&b));
    b.ground_albedo[1] = 1.01F;
    EXPECT_FALSE(eng_atmosphere_valid(&b));
}

UTEST(eng_atmosphere, the_air_mass_is_one_at_the_zenith_forty_at_the_horizon_and_monotonic)
{
    EXPECT_NEAR(eng_air_mass(1.0F), 1.0F, 1e-5F);
    EXPECT_NEAR(eng_air_mass(0.0F), 40.0F, 1e-4F);
    EXPECT_NEAR(eng_air_mass(0.5F), 1.9996F, 1e-3F);
    EXPECT_NEAR(eng_air_mass(-0.5F), 40.0F, 1e-4F); /* sous l'horizon : comme l'horizon */
    EXPECT_NEAR(eng_air_mass(2.0F), 1.0F, 1e-5F);   /* borné */
    EXPECT_NEAR(eng_air_mass(NAN), 40.0F, 1e-4F);
    float previous = eng_air_mass(0.0F);
    for (int i = 1; i <= 100; i++) {
        const float m = eng_air_mass((float)i / 100.0F);
        EXPECT_LT(m, previous);
        previous = m;
    }
}

UTEST(eng_atmosphere, pinned_values_match_an_independent_python_implementation)
{
    /* valeurs recalculées par un script séparé (mêmes formules, écrit à part) */
    const struct {
        float sun_el, sun_az, view_el, view_az;
        float want[3];
    } cases[] = {
        {45, 30, 45, 210, {0.18839F, 0.34477F, 0.59450F}},
        {5, 0, 2, 90, {0.45651F, 0.44536F, 0.17236F}},
        {90, 0, 30, 0, {0.41243F, 0.68719F, 1.13942F}},
        {-6, 0, 20, 180, {0.05896F, 0.05659F, 0.02022F}},
        {30, 0, 0, 180, {1.18742F, 2.00104F, 2.43803F}},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        float rgb[3];
        sky(cases[i].sun_el, cases[i].sun_az, cases[i].view_el, cases[i].view_az, rgb);
        EXPECT_TRUE(near_rel(rgb, cases[i].want, 2e-3F));
    }
}

UTEST(eng_atmosphere, the_sky_colour_follows_the_sun_height)
{
    /* midi (45°), on regarde à l'opposé du soleil, à 45° de hauteur : bleu > vert > rouge */
    float noon[3];
    sky(45, 0, 45, 180, noon);
    EXPECT_GT(noon[2], noon[1]);
    EXPECT_GT(noon[1], noon[0]);
    /* le zénith est plus saturé que l'horizon (l'horizon est plus pâle) */
    float zenith[3];
    float horizon[3];
    sky(45, 0, 90, 0, zenith);
    sky(45, 0, 0, 180, horizon);
    const float sat_zenith = (zenith[2] - zenith[0]) / (zenith[2] + zenith[0]);
    const float sat_horizon = (horizon[2] - horizon[0]) / (horizon[2] + horizon[0]);
    EXPECT_GT(sat_zenith, sat_horizon);
    EXPECT_GT(luma(horizon), luma(zenith)); /* et plus lumineux */
    /* au coucher (1°), vers le soleil à l'horizon : rouge > bleu, et bien plus rouge qu'à midi */
    float sunset[3];
    sky(1, 0, 3, 0, sunset);
    EXPECT_GT(sunset[0], sunset[2]);
    EXPECT_GT(sunset[0] / sunset[2], 20.0F * (noon[0] / noon[2]));
    /* le ciel s'assombrit quand le soleil descend : luminance croissante de -20° à 20° */
    float previous = -1.0F;
    for (int el = -20; el <= 20; el += 5) {
        float rgb[3];
        sky((float)el, 0, 30, 180, rgb);
        const float l = luma(rgb);
        if (el >= -10) {
            EXPECT_GT(l, previous);
        }
        previous = l;
    }
    /* au-dessus de 20°, le ciel reste dans une bande étroite (pas d'effondrement ni d'explosion) */
    for (int el = 20; el <= 80; el += 5) {
        float rgb[3];
        sky((float)el, 0, 30, 180, rgb);
        EXPECT_GT(luma(rgb), 0.35F);
        EXPECT_LT(luma(rgb), 0.70F);
    }
    /* nuit (-20°) : à peine le plancher nocturne, au moins 100 fois plus sombre que le jour */
    float night[3];
    sky(-20, 0, 30, 180, night);
    EXPECT_LT(luma(night) * 100.0F, luma(noon));
    EXPECT_NEAR(night[0], 0.002F, 1e-6F);
    EXPECT_NEAR(night[1], 0.003F, 1e-6F);
    EXPECT_NEAR(night[2], 0.006F, 1e-6F);
}

UTEST(eng_atmosphere, a_rotation_about_the_vertical_does_not_change_the_colour)
{
    for (int k = 0; k < 8; k++) {
        const float turn = (float)k * 47.0F;
        float a[3];
        float b[3];
        sky(25, 10, 15, 140, a);
        sky(25, 10 + turn, 15, 140 + turn, b);
        EXPECT_TRUE(near_rel(a, b, 1e-4F));
    }
}

UTEST(eng_atmosphere, below_the_horizon_is_the_ground_and_the_horizon_is_continuous)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    float s[3];
    dir(30, 0, s);
    /* tout droit vers le bas : albédo x ciel horizontal dans la direction de repli (+X) */
    const float down[3] = {0.0F, -1.0F, 0.0F};
    const float flat[3] = {1.0F, 0.0F, 0.0F};
    float ground[3];
    float horizon[3];
    ASSERT_TRUE(eng_atmosphere_sky(&a, s, down, ground));
    ASSERT_TRUE(eng_atmosphere_sky(&a, s, flat, horizon));
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(ground[c], a.ground_albedo[c] * horizon[c], 1e-4F * horizon[c] + 1e-6F);
    }
    /* en biais (y = -0,5) : même chose avec l'azimut de la vue */
    float v[3];
    dir(-30, 200, v);
    float oblique[3];
    ASSERT_TRUE(eng_atmosphere_sky(&a, s, v, oblique));
    const float flat200[3] = {v[0], 0.0F, v[2]};
    float h200[3];
    ASSERT_TRUE(eng_atmosphere_sky(&a, s, flat200, h200));
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(oblique[c], a.ground_albedo[c] * h200[c], 1e-4F * h200[c] + 1e-6F);
    }
    /* continuité de part et d'autre de l'horizon */
    for (int az = 0; az < 360; az += 30) {
        float up[3];
        float dn[3];
        const float yv[2] = {1e-4F, -1e-4F};
        float vu[3];
        float vd[3];
        dir(0.0F, (float)az, vu);
        vu[1] = yv[0];
        dir(0.0F, (float)az, vd);
        vd[1] = yv[1];
        ASSERT_TRUE(eng_atmosphere_sky(&a, s, vu, up));
        ASSERT_TRUE(eng_atmosphere_sky(&a, s, vd, dn));
        EXPECT_TRUE(near_rel(up, dn, 5e-3F));
    }
    /* le sol est plus sombre que le ciel à l'horizon */
    EXPECT_LT(luma(ground), luma(horizon));
}

UTEST(eng_atmosphere, the_sun_disc_is_there_only_above_the_horizon_and_reddens_at_sunset)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    float s[3];
    dir(45, 0, s);
    float with_disc[3];
    float sky_only[3];
    ASSERT_TRUE(eng_atmosphere_radiance(&a, s, s, with_disc));
    ASSERT_TRUE(eng_atmosphere_sky(&a, s, s, sky_only));
    EXPECT_GT(with_disc[0], sky_only[0] + 40.0F); /* sun_radiance 60 x transmittance ~ 0,6 */
    EXPECT_GT(with_disc[2], sky_only[2] + 30.0F);
    /* hors du disque : deux fois le rayon plus loin, rien */
    float away[3];
    float away_sky[3];
    float v[3];
    dir(45 + (2.0F * a.sun_radius_deg), 0, v);
    ASSERT_TRUE(eng_atmosphere_radiance(&a, s, v, away));
    ASSERT_TRUE(eng_atmosphere_sky(&a, s, v, away_sky));
    EXPECT_EQ(away[0], away_sky[0]);
    EXPECT_EQ(away[1], away_sky[1]);
    EXPECT_EQ(away[2], away_sky[2]);
    /* bord : décroissant du centre vers l'extérieur */
    float previous = 1e30F;
    for (int i = 0; i <= 20; i++) {
        const float off = a.sun_radius_deg * 1.1F * (float)i / 20.0F;
        dir(45 + off, 0, v);
        float rgb[3];
        ASSERT_TRUE(eng_atmosphere_radiance(&a, s, v, rgb));
        EXPECT_LE(rgb[0], previous);
        previous = rgb[0];
    }
    /* coucher (5°) : disque rouge-orangé */
    dir(5, 0, s);
    float sunset[3];
    ASSERT_TRUE(eng_atmosphere_radiance(&a, s, s, sunset));
    EXPECT_GT(sunset[0], 2.0F * sunset[2]);
    /* soleil sous l'horizon : on regarde la direction du soleil, c'est le sol, sans disque */
    dir(-5, 0, s);
    float hidden[3];
    float ground[3];
    ASSERT_TRUE(eng_atmosphere_radiance(&a, s, s, hidden));
    ASSERT_TRUE(eng_atmosphere_sky(&a, s, s, ground));
    EXPECT_EQ(hidden[0], ground[0]);
    EXPECT_LT(hidden[0], 1.0F);
}

UTEST(eng_atmosphere, the_disc_of_a_sun_exactly_on_the_horizon_is_drawn_when_looking_at_it)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    const float s[3] = {0.0F, 0.0F, 1.0F};
    float with_disc[3];
    float sky_only[3];
    ASSERT_TRUE(eng_atmosphere_radiance(&a, s, s, with_disc));
    ASSERT_TRUE(eng_atmosphere_sky(&a, s, s, sky_only));
    /* radiance 60 x transmittance 0,065 (rouge) : au moins 3 de plus que le ciel seul */
    EXPECT_GT(with_disc[0], sky_only[0] + 3.0F);
    /* un cheveu sous l'horizon : le sol, sans disque */
    const float below[3] = {0.0F, -1e-4F, 1.0F};
    float ground[3];
    float ground_sky[3];
    ASSERT_TRUE(eng_atmosphere_radiance(&a, s, below, ground));
    ASSERT_TRUE(eng_atmosphere_sky(&a, s, below, ground_sky));
    EXPECT_EQ(ground[0], ground_sky[0]);
}

UTEST(eng_atmosphere, the_direct_sunlight_gets_redder_and_then_disappears)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    float s[3];
    float color[3];
    float visibility;
    dir(90, 0, s);
    ASSERT_TRUE(eng_atmosphere_sun_light(&a, s, color, &visibility));
    EXPECT_EQ(color[0], 1.0F);
    EXPECT_EQ(color[1], 1.0F);
    EXPECT_EQ(color[2], 1.0F);
    EXPECT_EQ(visibility, 1.0F);
    const struct {
        float el;
        float want[3];
    } pins[] = {{45, {0.97142F, 0.94994F, 0.89828F}},
                {10, {0.72274F, 0.56260F, 0.30078F}},
                {5, {0.52018F, 0.31418F, 0.08908F}},
                {0, {0.06522F, 0.00794F, 4e-05F}}};
    float last_blue_over_red = 1.0F;
    for (size_t i = 0; i < sizeof pins / sizeof pins[0]; i++) {
        dir(pins[i].el, 33, s);
        ASSERT_TRUE(eng_atmosphere_sun_light(&a, s, color, &visibility));
        EXPECT_TRUE(near_rel(color, pins[i].want, 3e-3F));
        for (int c = 0; c < 3; c++) {
            EXPECT_LE(color[c], 1.0F);
        }
        const float ratio = color[2] / color[0];
        EXPECT_LT(ratio, last_blue_over_red);
        last_blue_over_red = ratio;
    }
    /* visibilité : 1 au-dessus de ~1,2°, 0,5 à l'horizon, 0 sous -1,2° */
    dir(2, 0, s);
    ASSERT_TRUE(eng_atmosphere_sun_light(&a, s, color, &visibility));
    EXPECT_EQ(visibility, 1.0F);
    dir(0, 0, s);
    ASSERT_TRUE(eng_atmosphere_sun_light(&a, s, color, &visibility));
    EXPECT_NEAR(visibility, 0.5F, 1e-5F);
    dir(-2, 0, s);
    ASSERT_TRUE(eng_atmosphere_sun_light(&a, s, color, &visibility));
    EXPECT_EQ(visibility, 0.0F);
    /* strictement monotone dans le fondu */
    float previous = -1.0F;
    for (int i = 0; i <= 20; i++) {
        const float sin_el = -0.02F + (0.04F * (float)i / 20.0F);
        const float v3[3] = {sqrtf(1.0F - (sin_el * sin_el)), sin_el, 0.0F};
        ASSERT_TRUE(eng_atmosphere_sun_light(&a, v3, color, &visibility));
        EXPECT_GE(visibility, previous);
        previous = visibility;
    }
}

UTEST(eng_atmosphere, the_ambient_light_is_one_at_noon_dims_with_the_sun_and_reddens)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    float s[3];
    float rgb[3];
    dir(90, 0, s);
    ASSERT_TRUE(eng_atmosphere_ambient(&a, s, rgb));
    EXPECT_EQ(rgb[0], 1.0F);
    EXPECT_EQ(rgb[1], 1.0F);
    EXPECT_EQ(rgb[2], 1.0F);
    const struct {
        float el;
        float want[3];
    } pins[] = {{45, {0.93900F, 0.89562F, 0.82698F}},
                {10, {0.31642F, 0.31270F, 0.20261F}},
                {0, {0.14689F, 0.09396F, 0.02170F}},
                {-6, {0.03769F, 0.02578F, 0.00902F}}};
    for (size_t i = 0; i < sizeof pins / sizeof pins[0]; i++) {
        dir(pins[i].el, 30, s);
        ASSERT_TRUE(eng_atmosphere_ambient(&a, s, rgb));
        EXPECT_TRUE(near_rel(rgb, pins[i].want, 3e-3F));
    }
    /* de plus en plus sombre quand le soleil descend de 30° à -12°, et rouge > bleu au coucher */
    float previous = 10.0F;
    for (int el = 30; el >= -12; el -= 6) {
        dir((float)el, 30, s);
        ASSERT_TRUE(eng_atmosphere_ambient(&a, s, rgb));
        EXPECT_LT(rgb[1], previous);
        previous = rgb[1];
    }
    dir(2, 30, s);
    ASSERT_TRUE(eng_atmosphere_ambient(&a, s, rgb));
    EXPECT_GT(rgb[0], 2.0F * rgb[2]);
    /* nuit : presque rien */
    dir(-30, 30, s);
    ASSERT_TRUE(eng_atmosphere_ambient(&a, s, rgb));
    EXPECT_LT(rgb[1], 0.01F);
}

UTEST(eng_atmosphere, invalid_requests_give_black_and_false)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    const float good[3] = {0.0F, 1.0F, 0.0F};
    const float zero[3] = {0.0F, 0.0F, 0.0F};
    const float nan3[3] = {NAN, 0.0F, 1.0F};
    const float inf3[3] = {0.0F, INFINITY, 1.0F};
    float rgb[3] = {7.0F, 7.0F, 7.0F};
    EXPECT_FALSE(eng_atmosphere_sky(&a, zero, good, rgb));
    EXPECT_EQ(rgb[0] + rgb[1] + rgb[2], 0.0F);
    rgb[0] = 7.0F;
    EXPECT_FALSE(eng_atmosphere_sky(&a, good, zero, rgb));
    EXPECT_EQ(rgb[0], 0.0F);
    rgb[0] = 7.0F;
    EXPECT_FALSE(eng_atmosphere_sky(&a, nan3, good, rgb));
    EXPECT_EQ(rgb[0], 0.0F);
    rgb[0] = 7.0F;
    EXPECT_FALSE(eng_atmosphere_sky(&a, good, inf3, rgb));
    EXPECT_EQ(rgb[0], 0.0F);
    rgb[0] = 7.0F;
    EXPECT_FALSE(eng_atmosphere_radiance(&a, nan3, good, rgb));
    EXPECT_FALSE(eng_atmosphere_sky(NULL, good, good, rgb));
    EngAtmosphere bad = a;
    bad.mie = NAN;
    EXPECT_FALSE(eng_atmosphere_sky(&bad, good, good, rgb));
    EXPECT_FALSE(eng_atmosphere_radiance(&bad, good, good, rgb));
    float color[3] = {7, 7, 7};
    float visibility = 7.0F;
    EXPECT_FALSE(eng_atmosphere_sun_light(&a, zero, color, &visibility));
    EXPECT_EQ(color[0], 0.0F);
    EXPECT_EQ(visibility, 0.0F);
    EXPECT_FALSE(eng_atmosphere_sun_light(&bad, good, color, &visibility));
    rgb[0] = 7.0F;
    EXPECT_FALSE(eng_atmosphere_ambient(&a, nan3, rgb));
    EXPECT_EQ(rgb[0], 0.0F);
    EXPECT_FALSE(eng_atmosphere_ambient(&bad, good, rgb));
    EngSkyDesc desc;
    EXPECT_FALSE(eng_atmosphere_to_sky_desc(&a, zero, &desc));
    EXPECT_FALSE(eng_atmosphere_to_sky_desc(&bad, good, &desc));
}

UTEST(eng_atmosphere, every_output_is_finite_and_bounded_on_a_large_random_sample)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    uint32_t state = 12345U;
    for (int i = 0; i < 60000; i++) {
        float d[6];
        for (int k = 0; k < 6; k++) {
            state = (state * 1664525U) + 1013904223U;
            d[k] = ((float)(state >> 8) / 8388608.0F) - 1.0F;
        }
        /* cas limites exacts : verticales, vue = soleil */
        if (i % 7 == 0) {
            d[0] = d[2] = 0.0F;
            d[1] = i % 2 == 0 ? 1.0F : -1.0F;
        }
        if (i % 11 == 0) {
            memcpy(&d[3], &d[0], 3 * sizeof(float));
        }
        float rgb[3];
        if (!eng_atmosphere_radiance(&a, &d[0], &d[3], rgb)) {
            continue; /* direction quasi nulle */
        }
        for (int c = 0; c < 3; c++) {
            ASSERT_TRUE(isfinite(rgb[c]));
            ASSERT_GE(rgb[c], 0.0F);
            ASSERT_LT(rgb[c], 500.0F);
        }
        float color[3];
        float visibility;
        ASSERT_TRUE(eng_atmosphere_sun_light(&a, &d[0], color, &visibility));
        float amb[3];
        ASSERT_TRUE(eng_atmosphere_ambient(&a, &d[0], amb));
        for (int c = 0; c < 3; c++) {
            ASSERT_TRUE(isfinite(color[c]) && color[c] >= 0.0F && color[c] <= 1.0F);
            ASSERT_TRUE(isfinite(amb[c]) && amb[c] >= 0.0F && amb[c] <= 1.05F);
        }
        ASSERT_TRUE(visibility >= 0.0F && visibility <= 1.0F);
    }
}

UTEST(eng_atmosphere, the_environment_gradient_is_this_sky)
{
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    float s[3];
    dir(35, 70, s);
    EngSkyDesc desc;
    ASSERT_TRUE(eng_atmosphere_to_sky_desc(&a, s, &desc));
    const float up[3] = {0.0F, 1.0F, 0.0F};
    float zenith[3];
    ASSERT_TRUE(eng_atmosphere_sky(&a, s, up, zenith));
    for (int c = 0; c < 3; c++) {
        EXPECT_EQ(desc.zenith[c], zenith[c]);
        EXPECT_NEAR(desc.sun_direction[c], s[c], 1e-6F);
        EXPECT_NEAR(desc.ground[c], a.ground_albedo[c] * desc.horizon[c], 1e-6F);
    }
    /* horizon : à 90° du soleil en azimut */
    float side[3];
    dir(0, 70 + 90, side);
    float horizon[3];
    ASSERT_TRUE(eng_atmosphere_sky(&a, s, side, horizon));
    EXPECT_TRUE(near_rel(desc.horizon, horizon, 1e-4F));
    float sun_light[3];
    float visibility;
    ASSERT_TRUE(eng_atmosphere_sun_light(&a, s, sun_light, &visibility));
    /* radiance du disque = radiance x transmittance absolue (non relative) */
    EXPECT_GT(desc.sun_radiance[0], desc.sun_radiance[2]);
    EXPECT_LT(desc.sun_radiance[0], a.sun_radiance);
    EXPECT_NEAR(desc.sun_angular_radius_deg, a.sun_radius_deg, 1e-6F);
    /* soleil pile au zénith : repli de l'azimut, valeurs finies */
    const float overhead[3] = {0.0F, 1.0F, 0.0F};
    ASSERT_TRUE(eng_atmosphere_to_sky_desc(&a, overhead, &desc));
    for (int c = 0; c < 3; c++) {
        EXPECT_TRUE(isfinite(desc.horizon[c]));
    }
}
