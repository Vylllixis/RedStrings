#include <math.h>
#include <stdbool.h>

#include "engine/tonemap.h"
#include "utest.h"

/* Référence CPU du tonemapping (M1-14). Valeurs de référence : à la main pour NONE, ACES (formule
 * de Narkowicz) et sRGB ; pour AgX, calculées par une implémentation Python (numpy) écrite
 * séparément à partir du code de three.js (le même calcul que le C testé, mais sans partager une
 * ligne), et recoupées par les propriétés (monotonie, bornes, gris moyen ~ 0,5 en sRGB). */

#define EPS 1e-5F
#define EPS_AGX 2e-4F /* constantes à 4 chiffres pour les matrices Rec.2020 : marge de 2e-4 */

static EngTonemap make(EngTonemapOperator op, float ev)
{
    EngTonemap t = {.op = op, .exposure_ev = ev};
    return t;
}

static void apply3(const EngTonemap *t, float r, float g, float b, float out[3])
{
    const float in[3] = {r, g, b};
    eng_tonemap_apply(t, in, out);
}

/* ── réglages ── */

UTEST(eng_tonemap, default_is_aces_at_zero_ev)
{
    EngTonemap t = make(ENG_TONEMAP_AGX, 3.0F);
    eng_tonemap_default(&t);
    EXPECT_EQ((int)t.op, (int)ENG_TONEMAP_ACES);
    EXPECT_NEAR(t.exposure_ev, 0.0F, 0.0F);
}

UTEST(eng_tonemap, sanitize_repairs_each_invalid_field_and_reports_it)
{
    EngTonemap t = make(ENG_TONEMAP_AGX, -2.5F);
    EXPECT_TRUE(eng_tonemap_sanitize(&t)); /* valide : intact */
    EXPECT_EQ((int)t.op, (int)ENG_TONEMAP_AGX);
    EXPECT_NEAR(t.exposure_ev, -2.5F, 0.0F);

    t = make(ENG_TONEMAP_NONE, ENG_EXPOSURE_EV_MAX); /* les bornes sont valides */
    EXPECT_TRUE(eng_tonemap_sanitize(&t));
    t = make(ENG_TONEMAP_NONE, ENG_EXPOSURE_EV_MIN);
    EXPECT_TRUE(eng_tonemap_sanitize(&t));

    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    t = make((EngTonemapOperator)99, 1.0F);
    EXPECT_FALSE(eng_tonemap_sanitize(&t));
    EXPECT_EQ((int)t.op, (int)ENG_TONEMAP_ACES);
    EXPECT_NEAR(t.exposure_ev, 1.0F, 0.0F); /* l'exposition valide est conservée */

    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    t = make((EngTonemapOperator)-1, 0.0F);
    EXPECT_FALSE(eng_tonemap_sanitize(&t));
    EXPECT_EQ((int)t.op, (int)ENG_TONEMAP_ACES);
    t = make(ENG_TONEMAP_COUNT, 0.0F); /* COUNT n'est pas un opérateur */
    EXPECT_FALSE(eng_tonemap_sanitize(&t));
    EXPECT_EQ((int)t.op, (int)ENG_TONEMAP_ACES);

    t = make(ENG_TONEMAP_AGX, NAN);
    EXPECT_FALSE(eng_tonemap_sanitize(&t));
    EXPECT_NEAR(t.exposure_ev, 0.0F, 0.0F);
    EXPECT_EQ((int)t.op, (int)ENG_TONEMAP_AGX); /* l'opérateur valide est conservé */
    t = make(ENG_TONEMAP_AGX, INFINITY);
    EXPECT_FALSE(eng_tonemap_sanitize(&t));
    EXPECT_NEAR(t.exposure_ev, 0.0F, 0.0F);
    t = make(ENG_TONEMAP_AGX, -INFINITY);
    EXPECT_FALSE(eng_tonemap_sanitize(&t));
    EXPECT_NEAR(t.exposure_ev, 0.0F, 0.0F);

    t = make(ENG_TONEMAP_NONE, 50.0F); /* trop grand : borné, pas remis à zéro */
    EXPECT_FALSE(eng_tonemap_sanitize(&t));
    EXPECT_NEAR(t.exposure_ev, ENG_EXPOSURE_EV_MAX, 0.0F);
    t = make(ENG_TONEMAP_NONE, -50.0F);
    EXPECT_FALSE(eng_tonemap_sanitize(&t));
    EXPECT_NEAR(t.exposure_ev, ENG_EXPOSURE_EV_MIN, 0.0F);
}

UTEST(eng_tonemap, exposure_scale_is_two_to_the_ev)
{
    EngTonemap t = make(ENG_TONEMAP_NONE, 0.0F);
    EXPECT_NEAR(eng_tonemap_exposure_scale(&t), 1.0F, EPS);
    t.exposure_ev = 1.0F;
    EXPECT_NEAR(eng_tonemap_exposure_scale(&t), 2.0F, EPS);
    t.exposure_ev = -1.0F;
    EXPECT_NEAR(eng_tonemap_exposure_scale(&t), 0.5F, EPS);
    t.exposure_ev = 3.0F;
    EXPECT_NEAR(eng_tonemap_exposure_scale(&t), 8.0F, EPS);
    t.exposure_ev = 0.5F; /* demi-stop : racine de 2 */
    EXPECT_NEAR(eng_tonemap_exposure_scale(&t), 1.4142135F, EPS);
}

UTEST(eng_tonemap, operator_names)
{
    EXPECT_STREQ(eng_tonemap_operator_name(ENG_TONEMAP_NONE), "aucun");
    EXPECT_STREQ(eng_tonemap_operator_name(ENG_TONEMAP_ACES), "ACES");
    EXPECT_STREQ(eng_tonemap_operator_name(ENG_TONEMAP_AGX), "AgX");
    EXPECT_STREQ(eng_tonemap_operator_name(ENG_TONEMAP_COUNT), "?");
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    EXPECT_STREQ(eng_tonemap_operator_name((EngTonemapOperator)-3), "?");
}

/* ── opérateurs ── */

UTEST(eng_tonemap, none_is_a_plain_clamp_after_the_exposure)
{
    EngTonemap t = make(ENG_TONEMAP_NONE, 0.0F);
    float out[3];
    apply3(&t, 0.5F, 2.0F, 0.0F, out);
    EXPECT_NEAR(out[0], 0.5F, EPS);
    EXPECT_NEAR(out[1], 1.0F, EPS); /* écrasé */
    EXPECT_NEAR(out[2], 0.0F, EPS);
    t.exposure_ev = 1.0F; /* 0,25 -> 0,5 ; 0,5 -> 1,0 ; 2 -> 4 -> 1 */
    apply3(&t, 0.25F, 0.5F, 2.0F, out);
    EXPECT_NEAR(out[0], 0.5F, EPS);
    EXPECT_NEAR(out[1], 1.0F, EPS);
    EXPECT_NEAR(out[2], 1.0F, EPS);
    t.exposure_ev = -2.0F; /* ÷4 */
    apply3(&t, 0.8F, 0.4F, 4.0F, out);
    EXPECT_NEAR(out[0], 0.2F, EPS);
    EXPECT_NEAR(out[1], 0.1F, EPS);
    EXPECT_NEAR(out[2], 1.0F, EPS);
}

UTEST(eng_tonemap, aces_matches_the_narkowicz_formula_by_hand)
{
    const EngTonemap t = make(ENG_TONEMAP_ACES, 0.0F);
    float out[3];
    apply3(&t, 0.0F, 1.0F, 0.18F, out);
    EXPECT_NEAR(out[0], 0.0F, EPS);          /* 0 / 0,14 */
    EXPECT_NEAR(out[1], 2.54F / 3.16F, EPS); /* x = 1 : 2,54 / 3,16 = 0,803797 */
    EXPECT_NEAR(out[2], 0.266899F, EPS);     /* x = 0,18 : 0,086724 / 0,324932 */
    apply3(&t, 0.5F, 2.0F, 8.0F, out);
    EXPECT_NEAR(out[0], 0.616307F, EPS);
    EXPECT_NEAR(out[1], 0.914855F, EPS);
    EXPECT_NEAR(out[2], 1.0F, EPS); /* la courbe dépasse 1 pour les grandes valeurs : bornée */
}

UTEST(eng_tonemap, aces_applies_the_exposure_before_the_curve)
{
    EngTonemap t = make(ENG_TONEMAP_ACES, 1.0F); /* 0,5 +1 EV = 1,0 */
    float out[3];
    apply3(&t, 0.5F, 0.5F, 0.5F, out);
    EXPECT_NEAR(out[0], 2.54F / 3.16F, EPS);
    t.exposure_ev = -1.0F; /* 2,0 -1 EV = 1,0 */
    apply3(&t, 2.0F, 2.0F, 2.0F, out);
    EXPECT_NEAR(out[1], 2.54F / 3.16F, EPS);
}

UTEST(eng_tonemap, agx_matches_the_independent_reference)
{
    const EngTonemap t = make(ENG_TONEMAP_AGX, 0.0F);
    float out[3];
    apply3(&t, 0.0F, 0.0F, 0.0F, out);
    EXPECT_NEAR(out[0], 0.0F, EPS_AGX);
    EXPECT_NEAR(out[1], 0.0F, EPS_AGX);
    EXPECT_NEAR(out[2], 0.0F, EPS_AGX);
    apply3(&t, 0.18F, 0.18F, 0.18F, out);
    EXPECT_NEAR(out[0], 0.214549F, EPS_AGX);
    EXPECT_NEAR(out[1], 0.214502F, EPS_AGX);
    EXPECT_NEAR(out[2], 0.214499F, EPS_AGX);
    apply3(&t, 1.0F, 1.0F, 1.0F, out);
    EXPECT_NEAR(out[0], 0.590229F, EPS_AGX);
    EXPECT_NEAR(out[1], 0.590136F, EPS_AGX);
    EXPECT_NEAR(out[2], 0.590102F, EPS_AGX);
    apply3(&t, 0.5F, 0.2F, 0.1F, out); /* une vraie couleur : teste les matrices canal par canal */
    EXPECT_NEAR(out[0], 0.441477F, EPS_AGX);
    EXPECT_NEAR(out[1], 0.236864F, EPS_AGX);
    EXPECT_NEAR(out[2], 0.150757F, EPS_AGX);
    apply3(&t, 4.0F, 0.05F, 0.5F, out); /* le rouge sature, les autres non */
    EXPECT_NEAR(out[0], 1.0F, EPS_AGX);
    EXPECT_NEAR(out[1], 0.409759F, EPS_AGX);
    EXPECT_NEAR(out[2], 0.484562F, EPS_AGX);
    apply3(&t, 16.0F, 16.0F, 16.0F, out); /* très lumineux : proche de 1 sans l'atteindre */
    EXPECT_NEAR(out[0], 0.995212F, EPS_AGX);
    apply3(&t, 100.0F, 100.0F, 100.0F, out);
    EXPECT_NEAR(out[1], 0.996879F, EPS_AGX);
}

UTEST(eng_tonemap, agx_middle_grey_is_about_half_after_srgb_encoding)
{
    const EngTonemap t = make(ENG_TONEMAP_AGX, 0.0F);
    float out[3];
    apply3(&t, 0.18F, 0.18F, 0.18F, out);
    EXPECT_NEAR(eng_srgb_encode(out[1]), 0.5F, 0.01F);
}

UTEST(eng_tonemap, curves_are_monotonic_bounded_and_start_at_black)
{
    for (int op = 0; op < ENG_TONEMAP_COUNT; op++) {
        const EngTonemap t = make((EngTonemapOperator)op, 0.0F);
        float previous = -1.0F;
        for (int i = 0; i <= 400; i++) {
            const float x = (float)i * 0.05F; /* 0 .. 20 */
            float out[3];
            apply3(&t, x, x, x, out);
            ASSERT_TRUE(out[1] >= 0.0F && out[1] <= 1.0F);
            ASSERT_TRUE(out[1] >= previous - 1e-6F); /* jamais décroissante */
            previous = out[1];
        }
        float black[3];
        apply3(&t, 0.0F, 0.0F, 0.0F, black);
        EXPECT_NEAR(black[0], 0.0F, EPS_AGX);
    }
}

UTEST(eng_tonemap, negative_and_non_numeric_input_count_as_black)
{
    for (int op = 0; op < ENG_TONEMAP_COUNT; op++) {
        const EngTonemap t = make((EngTonemapOperator)op, 2.0F);
        float out[3];
        apply3(&t, -5.0F, NAN, -INFINITY, out);
        for (int c = 0; c < 3; c++) {
            EXPECT_TRUE(isfinite(out[c]));
            EXPECT_NEAR(out[c], 0.0F, EPS_AGX);
        }
        apply3(&t, INFINITY, 1.0F, 1.0F, out); /* +inf : sature, sans NaN */
        EXPECT_TRUE(isfinite(out[0]));
        EXPECT_TRUE(out[0] > 0.98F);
    }
}

UTEST(eng_tonemap, apply_works_in_place)
{
    const EngTonemap t = make(ENG_TONEMAP_AGX, 0.0F);
    float rgb[3] = {0.5F, 0.2F, 0.1F};
    eng_tonemap_apply(&t, rgb, rgb);
    EXPECT_NEAR(rgb[0], 0.441477F, EPS_AGX);
    EXPECT_NEAR(rgb[1], 0.236864F, EPS_AGX);
    EXPECT_NEAR(rgb[2], 0.150757F, EPS_AGX);
}

/* ── sRGB ── */

UTEST(eng_srgb, encode_matches_known_values)
{
    EXPECT_NEAR(eng_srgb_encode(0.0F), 0.0F, EPS);
    EXPECT_NEAR(eng_srgb_encode(1.0F), 1.0F, EPS);
    EXPECT_NEAR(eng_srgb_encode(0.002F), 0.02584F, EPS); /* segment linéaire : 12,92 x */
    EXPECT_NEAR(eng_srgb_encode(0.18F), 0.461356F, EPS);
    EXPECT_NEAR(eng_srgb_encode(0.5F), 0.735357F, EPS);
    EXPECT_NEAR(eng_srgb_encode(0.0031308F), 0.04045F, 1e-4F); /* raccord des deux segments */
}

UTEST(eng_srgb, encode_clamps_and_survives_non_numbers)
{
    EXPECT_NEAR(eng_srgb_encode(2.0F), 1.0F, EPS);
    EXPECT_NEAR(eng_srgb_encode(-1.0F), 0.0F, EPS);
    EXPECT_NEAR(eng_srgb_encode(NAN), 0.0F, EPS);
    EXPECT_NEAR(eng_srgb_encode(INFINITY), 1.0F, EPS);
    EXPECT_NEAR(eng_srgb_decode(NAN), 0.0F, EPS);
}

UTEST(eng_srgb, decode_inverts_encode)
{
    for (int i = 0; i <= 100; i++) {
        const float x = (float)i / 100.0F;
        EXPECT_NEAR(eng_srgb_decode(eng_srgb_encode(x)), x, 1e-5F);
    }
    EXPECT_NEAR(eng_srgb_decode(0.5F), 0.214041F, 1e-5F); /* valeur connue */
    EXPECT_NEAR(eng_srgb_decode(0.02F), 0.02F / 12.92F, EPS);
}
