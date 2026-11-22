#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "engine/lighting.h"
#include "utest.h"

/* Éclairage (M1-13, PBR depuis M1-15), côté CPU : structure, orientation du soleil et correction
 * des valeurs. Les valeurs attendues sont calculées À LA MAIN (angles remarquables), pas avec le
 * code testé. Le calcul de la couleur d'une surface est testé dans test_pbr.c. */

#define EPS 1e-4F

/* Égalité champ à champ (EngLighting n'est que des float). */
static bool same_lighting(const EngLighting *a, const EngLighting *b)
{
    const float *x = (const float *)a;
    const float *y = (const float *)b;
    for (size_t i = 0; i < sizeof *a / sizeof(float); i++) {
        if (fabsf(x[i] - y[i]) > 0.0F) {
            return false;
        }
    }
    return true;
}

static float length3(const float v[3])
{
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* Éclairage sans soleil ni ambiante : on n'allume que ce que le test veut voir. */
static EngLighting dark(void)
{
    EngLighting l = {0};
    l.sun_direction[2] = 1.0F;
    l.sun_color[0] = l.sun_color[1] = l.sun_color[2] = 1.0F;
    l.ambient_color[0] = l.ambient_color[1] = l.ambient_color[2] = 1.0F;
    l.camera_position[2] = 5.0F;
    return l;
}

/* ── Valeurs par défaut et orientation du soleil ── */

UTEST(eng_lighting, defaults_are_valid_and_unit_length)
{
    EngLighting l;
    memset(&l, 0xAB, sizeof l);
    eng_lighting_default(&l);
    EXPECT_NEAR(length3(l.sun_direction), 1.0F, EPS);
    /* azimut 30°, hauteur 45° : (cos45 sin30, sin45, cos45 cos30) */
    EXPECT_NEAR(l.sun_direction[0], 0.35355339F, EPS);
    EXPECT_NEAR(l.sun_direction[1], 0.70710678F, EPS);
    EXPECT_NEAR(l.sun_direction[2], 0.61237244F, EPS);
    EXPECT_NEAR(l.sun_intensity, 1.0F, EPS);
    EXPECT_NEAR(l.ambient_intensity, 0.25F, EPS);
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(l.sun_color[c], 1.0F, EPS);
        EXPECT_NEAR(l.ambient_color[c], 1.0F, EPS);
        EXPECT_NEAR(l.camera_position[c], 0.0F, EPS);
    }
    EXPECT_TRUE(eng_lighting_sanitize(&l)); /* le défaut n'a rien à corriger */
}

UTEST(eng_lighting, sun_angles_point_toward_the_documented_directions)
{
    EngLighting l;
    eng_lighting_default(&l);
    ASSERT_TRUE(eng_lighting_set_sun_angles(&l, 0.0F, 0.0F)); /* horizon, vers +Z */
    EXPECT_NEAR(l.sun_direction[0], 0.0F, EPS);
    EXPECT_NEAR(l.sun_direction[1], 0.0F, EPS);
    EXPECT_NEAR(l.sun_direction[2], 1.0F, EPS);
    ASSERT_TRUE(eng_lighting_set_sun_angles(&l, 90.0F, 0.0F)); /* vers +X */
    EXPECT_NEAR(l.sun_direction[0], 1.0F, EPS);
    EXPECT_NEAR(l.sun_direction[2], 0.0F, EPS);
    ASSERT_TRUE(eng_lighting_set_sun_angles(&l, -90.0F, 0.0F)); /* vers -X */
    EXPECT_NEAR(l.sun_direction[0], -1.0F, EPS);
    ASSERT_TRUE(eng_lighting_set_sun_angles(&l, 180.0F, 0.0F)); /* vers -Z */
    EXPECT_NEAR(l.sun_direction[2], -1.0F, EPS);
    ASSERT_TRUE(
        eng_lighting_set_sun_angles(&l, 123.0F, 90.0F)); /* zénith, quel que soit l'azimut */
    EXPECT_NEAR(l.sun_direction[0], 0.0F, EPS);
    EXPECT_NEAR(l.sun_direction[1], 1.0F, EPS);
    EXPECT_NEAR(l.sun_direction[2], 0.0F, EPS);
    ASSERT_TRUE(
        eng_lighting_set_sun_angles(&l, 0.0F, -90.0F)); /* nadir : la lumière vient du sol */
    EXPECT_NEAR(l.sun_direction[1], -1.0F, EPS);
    ASSERT_TRUE(eng_lighting_set_sun_angles(&l, 0.0F, 30.0F));
    EXPECT_NEAR(l.sun_direction[1], 0.5F, EPS); /* sin 30° */
    EXPECT_NEAR(l.sun_direction[2], 0.8660254F, EPS);
}

UTEST(eng_lighting, sun_direction_stays_unit_length_for_any_angle)
{
    EngLighting l;
    eng_lighting_default(&l);
    for (int az_step = -10; az_step <= 10; az_step++) {
        for (int el_step = -8; el_step <= 8; el_step++) {
            const float az = (float)az_step * 40.0F - 3.0F;
            const float el = (float)el_step * 12.5F - 1.0F;
            ASSERT_TRUE(eng_lighting_set_sun_angles(&l, az, el));
            EXPECT_NEAR(length3(l.sun_direction), 1.0F, EPS);
        }
    }
}

UTEST(eng_lighting, elevation_is_clamped_and_non_numeric_angles_are_refused)
{
    EngLighting l;
    eng_lighting_default(&l);
    ASSERT_TRUE(eng_lighting_set_sun_angles(&l, 0.0F, 500.0F));
    EXPECT_NEAR(l.sun_direction[1], 1.0F, EPS);
    ASSERT_TRUE(eng_lighting_set_sun_angles(&l, 0.0F, -500.0F));
    EXPECT_NEAR(l.sun_direction[1], -1.0F, EPS);

    EngLighting before;
    eng_lighting_default(&before);
    l = before;
    EXPECT_FALSE(eng_lighting_set_sun_angles(&l, NAN, 10.0F));
    EXPECT_FALSE(eng_lighting_set_sun_angles(&l, 10.0F, NAN));
    EXPECT_FALSE(eng_lighting_set_sun_angles(&l, INFINITY, 10.0F));
    EXPECT_FALSE(eng_lighting_set_sun_angles(&l, 10.0F, -INFINITY));
    EXPECT_TRUE(same_lighting(&l, &before)); /* inchangé */
}

/* ── eng_lighting_sanitize ── */

UTEST(eng_lighting_sanitize, a_valid_lighting_is_left_untouched)
{
    EngLighting l = dark();
    l.sun_intensity = 2.0F;
    EngLighting copy = l;
    EXPECT_TRUE(eng_lighting_sanitize(&l));
    EXPECT_TRUE(same_lighting(&l, &copy));
    l.sun_intensity = 0.0F; /* l'intensité nulle est valide */
    copy = l;
    EXPECT_TRUE(eng_lighting_sanitize(&l));
    EXPECT_TRUE(same_lighting(&l, &copy));
}

UTEST(eng_lighting_sanitize, the_sun_direction_is_normalised_or_replaced)
{
    EngLighting l = dark();
    l.sun_direction[0] = 0.0F;
    l.sun_direction[1] = 3.0F;
    l.sun_direction[2] = 4.0F;
    EXPECT_FALSE(eng_lighting_sanitize(&l));
    EXPECT_NEAR(l.sun_direction[1], 0.6F, EPS);
    EXPECT_NEAR(l.sun_direction[2], 0.8F, EPS);
    EXPECT_NEAR(length3(l.sun_direction), 1.0F, EPS);

    memset(l.sun_direction, 0, sizeof l.sun_direction); /* nulle : +Y */
    EXPECT_FALSE(eng_lighting_sanitize(&l));
    EXPECT_NEAR(l.sun_direction[1], 1.0F, EPS);
    EXPECT_NEAR(l.sun_direction[0], 0.0F, EPS);

    l.sun_direction[0] = NAN;
    l.sun_direction[1] = 0.0F;
    l.sun_direction[2] = 1.0F;
    EXPECT_FALSE(eng_lighting_sanitize(&l));
    EXPECT_NEAR(l.sun_direction[1], 1.0F, EPS);
    EXPECT_NEAR(length3(l.sun_direction), 1.0F, EPS);

    l.sun_direction[0] = INFINITY;
    EXPECT_FALSE(eng_lighting_sanitize(&l));
    EXPECT_NEAR(l.sun_direction[1], 1.0F, EPS);
}

UTEST(eng_lighting_sanitize, negative_or_non_numeric_values_become_zero)
{
    EngLighting l = dark();
    l.sun_color[0] = -0.5F;
    l.sun_color[1] = NAN;
    l.sun_color[2] = INFINITY;
    l.ambient_color[0] = -1.0F;
    l.ambient_color[1] = NAN;
    l.ambient_color[2] = 0.75F;
    l.sun_intensity = -2.0F;
    l.ambient_intensity = NAN;
    EXPECT_FALSE(eng_lighting_sanitize(&l));
    for (int c = 0; c < 3; c++) {
        EXPECT_EQ(l.sun_color[c], 0.0F);
    }
    EXPECT_EQ(l.ambient_color[0], 0.0F);
    EXPECT_EQ(l.ambient_color[1], 0.0F);
    EXPECT_NEAR(l.ambient_color[2], 0.75F, EPS); /* une valeur correcte voisine est gardée */
    EXPECT_EQ(l.sun_intensity, 0.0F);
    EXPECT_EQ(l.ambient_intensity, 0.0F);
}

UTEST(eng_lighting_sanitize, each_field_alone_is_reported_and_fixed)
{
    /* un champ fautif à la fois : chacun doit être détecté (retour false) et corrigé */
    EngLighting l = dark();
    l.sun_intensity = -1.0F;
    EXPECT_FALSE(eng_lighting_sanitize(&l));
    l = dark();
    l.ambient_intensity = -1.0F;
    EXPECT_FALSE(eng_lighting_sanitize(&l));
    l = dark();
    l.ambient_color[2] = -1.0F;
    EXPECT_FALSE(eng_lighting_sanitize(&l));
    l = dark();
    l.sun_color[2] = -1.0F;
    EXPECT_FALSE(eng_lighting_sanitize(&l));
    l = dark();
    l.camera_position[1] = NAN;
    EXPECT_FALSE(eng_lighting_sanitize(&l));
    EXPECT_EQ(l.camera_position[1], 0.0F);
    l = dark();
    l.camera_position[2] = INFINITY;
    EXPECT_FALSE(eng_lighting_sanitize(&l));
    EXPECT_EQ(l.camera_position[2], 0.0F);
    l = dark();
    l.camera_position[0] = 12.5F; /* une position finie, même négative, est valide */
    l.camera_position[1] = -3.0F;
    EXPECT_TRUE(eng_lighting_sanitize(&l));
    EXPECT_NEAR(l.camera_position[0], 12.5F, EPS);
    EXPECT_NEAR(l.camera_position[1], -3.0F, EPS);
}
