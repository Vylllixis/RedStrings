/* Biomes de la grande carte : bandes le long de X, frontières fondues et ondulées, échelle de
 * relief et règle de matières par biome. */
#include <math.h>

#include "engine/biome.h"
#include "utest.h"

static float sum4(const float w[4])
{
    return w[0] + w[1] + w[2] + w[3];
}

UTEST(eng_biome, each_band_centre_is_one_pure_biome_in_order)
{
    const EngBiomeMap map = eng_biome_map_default();
    ASSERT_TRUE(eng_biome_map_valid(&map));
    const double centres[4] = {400.0, 1200.0, 2000.0, 2800.0};
    for (int k = 0; k < 4; k++) {
        float w[4];
        eng_biome_weights(&map, centres[k], 0.0, w);
        EXPECT_NEAR(w[k], 1.0F, 1e-6F);
        EXPECT_NEAR(sum4(w), 1.0F, 1e-6F);
        EXPECT_EQ((int)eng_biome_at(&map, centres[k], 0.0), k);
    }
    EXPECT_EQ((int)eng_biome_at(&map, -5000.0, 0.0), (int)ENG_BIOME_PLAIN); /* derrière le départ */
    EXPECT_EQ((int)eng_biome_at(&map, 90000.0, 0.0), (int)ENG_BIOME_VOLCANO); /* au-delà */
    EXPECT_STREQ(eng_biome_name(ENG_BIOME_SNOW), "montagne enneigée");
}

UTEST(eng_biome, weights_are_positive_sum_to_one_and_blend_monotonically_across_a_border)
{
    const EngBiomeMap map = eng_biome_map_default();
    float previous = 1.0F;
    for (int step = 0; step <= 80; step++) { /* frontière plaine -> montagne à x = 800 */
        const double x = 600.0 + (5.0 * (double)step);
        float w[4];
        eng_biome_weights(&map, x, 0.0, w);
        EXPECT_NEAR(sum4(w), 1.0F, 1e-5F);
        for (int k = 0; k < 4; k++) {
            EXPECT_GE(w[k], 0.0F);
        }
        EXPECT_LE(w[ENG_BIOME_PLAIN], previous + 1e-6F); /* la plaine ne revient pas */
        previous = w[ENG_BIOME_PLAIN];
        EXPECT_NEAR(w[ENG_BIOME_DESERT] + w[ENG_BIOME_VOLCANO], 0.0F, 1e-6F);
    }
    float mid[4];
    eng_biome_weights(&map, 800.0, 0.0, mid); /* sin(0) = 0 : pas d'ondulation à z = 0 */
    EXPECT_NEAR(mid[ENG_BIOME_PLAIN], 0.5F, 1e-5F);
    EXPECT_NEAR(mid[ENG_BIOME_SNOW], 0.5F, 1e-5F);
}

UTEST(eng_biome, the_border_wobbles_with_z_by_the_requested_amplitude)
{
    const EngBiomeMap map = eng_biome_map_default();
    float a[4];
    float b[4];
    /* z = quart de longueur d'onde : la frontière est décalée de 70 m (le point « avance ») */
    eng_biome_weights(&map, 800.0 - 70.0, 225.0, a);
    EXPECT_NEAR(a[ENG_BIOME_PLAIN], 0.5F, 1e-4F);
    eng_biome_weights(&map, 800.0 + 70.0, 675.0, b); /* trois quarts : décalée dans l'autre sens */
    EXPECT_NEAR(b[ENG_BIOME_PLAIN], 0.5F, 1e-4F);
}

UTEST(eng_biome, a_disabled_or_invalid_map_is_plain_everywhere_with_scale_one)
{
    EngBiomeMap map = eng_biome_map_default();
    map.enabled = false;
    float w[4];
    eng_biome_weights(&map, 2000.0, 0.0, w);
    EXPECT_NEAR(w[ENG_BIOME_PLAIN], 1.0F, 0.0F);
    eng_biome_weights(NULL, 2000.0, 0.0, w);
    EXPECT_NEAR(w[ENG_BIOME_PLAIN], 1.0F, 0.0F);
    EXPECT_NEAR(eng_biome_altitude_scale(NULL), 1.0F, 0.0F);
    map = eng_biome_map_default();
    map.blend_m = 0.0F;
    EXPECT_FALSE(eng_biome_map_valid(&map));
    map = eng_biome_map_default();
    map.band_m = 100.0F; /* plus court que le fondu */
    EXPECT_FALSE(eng_biome_map_valid(&map));
    map = eng_biome_map_default();
    map.wobble_wavelength_m = NAN;
    EXPECT_FALSE(eng_biome_map_valid(&map));
    eng_biome_weights(&map, 2000.0, 0.0, w); /* invalide : plaine */
    EXPECT_NEAR(w[ENG_BIOME_PLAIN], 1.0F, 0.0F);
}

UTEST(eng_biome, the_relief_scale_is_the_weighted_mix_of_the_four_biomes)
{
    const float plain[4] = {1, 0, 0, 0};
    const float snow[4] = {0, 1, 0, 0};
    const float desert[4] = {0, 0, 1, 0};
    const float volcano[4] = {0, 0, 0, 1};
    const float half[4] = {0.5F, 0.5F, 0, 0};
    EXPECT_NEAR(eng_biome_altitude_scale(plain), 0.45F, 1e-6F);
    EXPECT_NEAR(eng_biome_altitude_scale(snow), 1.8F, 1e-6F);
    EXPECT_NEAR(eng_biome_altitude_scale(desert), 0.35F, 1e-6F);
    EXPECT_NEAR(eng_biome_altitude_scale(volcano), 1.2F, 1e-6F);
    EXPECT_NEAR(eng_biome_altitude_scale(half), 1.125F, 1e-6F);
}

UTEST(eng_biome, each_biome_has_its_materials_and_the_total_is_kept)
{
    const float base[4] = {0.5F, 0.2F, 0.2F, 0.1F}; /* herbe, roche, terre, neige */
    float out[4];
    const float plain[4] = {1, 0, 0, 0};
    eng_biome_splat(plain, base, out);
    EXPECT_NEAR(out[ENG_SPLAT_SNOW], 0.0F, 1e-6F);
    EXPECT_NEAR(out[ENG_SPLAT_GRASS], 0.6F, 1e-6F);
    EXPECT_NEAR(sum4(out), 1.0F, 1e-6F);
    const float snow[4] = {0, 1, 0, 0};
    eng_biome_splat(snow, base, out);
    EXPECT_NEAR(out[ENG_SPLAT_SNOW], 0.8F, 1e-6F);
    EXPECT_NEAR(out[ENG_SPLAT_ROCK], 0.2F, 1e-6F);
    EXPECT_NEAR(out[ENG_SPLAT_GRASS], 0.0F, 1e-6F);
    const float desert[4] = {0, 0, 1, 0};
    eng_biome_splat(desert, base, out);
    EXPECT_NEAR(out[ENG_SPLAT_DIRT], 0.8F, 1e-6F);
    EXPECT_NEAR(out[ENG_SPLAT_GRASS] + out[ENG_SPLAT_SNOW], 0.0F, 1e-6F);
    const float volcano[4] = {0, 0, 0, 1};
    eng_biome_splat(volcano, base, out);
    EXPECT_NEAR(out[ENG_SPLAT_ROCK], 0.2F + (0.8F * 0.8F), 1e-6F);
    EXPECT_NEAR(out[ENG_SPLAT_DIRT], 0.16F, 1e-6F);
    EXPECT_NEAR(sum4(out), 1.0F, 1e-6F);
    const float mixed[4] = {0.25F, 0.25F, 0.25F, 0.25F};
    eng_biome_splat(mixed, base, out);
    EXPECT_NEAR(sum4(out), 1.0F, 1e-6F);
}
