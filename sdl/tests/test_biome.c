/* Biomes de la grande carte : bandes le long de X, frontières fondues et ondulées, échelle de
 * relief et règle de matières par biome. */
#include <math.h>
#include <stdbool.h>

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
    eng_biome_splat(plain, base, 1.0F, out);
    EXPECT_NEAR(out[ENG_SPLAT_SNOW], 0.0F, 1e-6F);
    EXPECT_NEAR(out[ENG_SPLAT_GRASS], 0.6F, 1e-6F);
    EXPECT_NEAR(sum4(out), 1.0F, 1e-6F);
    const float snow[4] = {0, 1, 0, 0};
    eng_biome_splat(snow, base, 1.0F, out);
    EXPECT_NEAR(out[ENG_SPLAT_SNOW], 0.8F, 1e-6F);
    EXPECT_NEAR(out[ENG_SPLAT_ROCK], 0.2F, 1e-6F);
    EXPECT_NEAR(out[ENG_SPLAT_GRASS], 0.0F, 1e-6F);
    const float desert[4] = {0, 0, 1, 0};
    eng_biome_splat(desert, base, 1.0F, out);
    EXPECT_NEAR(out[ENG_SPLAT_GRASS], 0.8F, 1e-6F); /* canal 0 : du sable dans le désert */
    EXPECT_NEAR(out[ENG_SPLAT_DIRT] + out[ENG_SPLAT_SNOW], 0.0F, 1e-6F);
    EXPECT_NEAR(sum4(out), 1.0F, 1e-6F);
    const float volcano[4] = {0, 0, 0, 1};
    eng_biome_splat(volcano, base, 0.0F, out);
    EXPECT_NEAR(out[ENG_SPLAT_ROCK], 0.2F + (0.8F * 0.8F), 1e-6F); /* herbe 0,5 : pas de lave */
    EXPECT_NEAR(out[ENG_SPLAT_DIRT], 0.16F, 1e-6F);
    EXPECT_NEAR(out[ENG_SPLAT_SNOW], 0.0F, 1e-6F);
    EXPECT_NEAR(sum4(out), 1.0F, 1e-6F);
    eng_biome_splat(volcano, base, 1.0F, out); /* au cœur d'une coulée : lave (canal 3) */
    EXPECT_NEAR(out[ENG_SPLAT_SNOW], 0.8F, 1e-6F);
    EXPECT_NEAR(out[ENG_SPLAT_ROCK], 0.2F, 1e-6F); /* la roche des pentes reste */
    EXPECT_NEAR(out[ENG_SPLAT_DIRT], 0.0F, 1e-6F);
    EXPECT_NEAR(sum4(out), 1.0F, 1e-6F);
    eng_biome_splat(volcano, base, 0.5F, out); /* au bord : à moitié */
    EXPECT_NEAR(out[ENG_SPLAT_SNOW], 0.4F, 1e-6F);
    EXPECT_NEAR(sum4(out), 1.0F, 1e-6F);
    eng_biome_splat(volcano, base, NAN, out); /* non fini : pas de lave */
    EXPECT_NEAR(out[ENG_SPLAT_SNOW], 0.0F, 1e-6F);
    eng_biome_splat(plain, base, 1.0F, out); /* hors du volcan, la coulée ne change rien */
    EXPECT_NEAR(out[ENG_SPLAT_SNOW], 0.0F, 1e-6F);
    const float mixed[4] = {0.25F, 0.25F, 0.25F, 0.25F};
    eng_biome_splat(mixed, base, 1.0F, out);
    EXPECT_NEAR(sum4(out), 1.0F, 1e-6F);
}

UTEST(biome, the_volcano_ground_is_darker_and_the_shade_blends_at_the_border)
{
    EXPECT_NEAR(eng_biome_shade((float[4]){1, 0, 0, 0}), 1.0F, 1e-6F);
    EXPECT_NEAR(eng_biome_shade((float[4]){0, 1, 0, 0}), 1.0F, 1e-6F);
    EXPECT_NEAR(eng_biome_shade((float[4]){0, 0, 1, 0}), 1.0F, 1e-6F);
    EXPECT_NEAR(eng_biome_shade((float[4]){0, 0, 0, 1}), 0.45F, 1e-6F);
    EXPECT_NEAR(eng_biome_shade((float[4]){0, 0, 0.5F, 0.5F}), 0.725F, 1e-6F); /* frontière */
    EXPECT_NEAR(eng_biome_shade((float[4]){0, 0, 0, 0}), 1.0F, 1e-6F);
    EXPECT_NEAR(eng_biome_shade((float[4]){NAN, 0, 0, 1}), 0.45F, 1e-6F);
    EXPECT_NEAR(eng_biome_shade(NULL), 1.0F, 1e-6F);
}

/* Le canal de l'herbe sert au sable, celui de la neige à la lave : jamais les deux sens dans une
 * même cellule de 256 m, pour toute la carte par défaut. */
UTEST(biome, sand_and_lava_reuse_a_channel_only_in_cells_their_left_biome_never_reaches)
{
    const EngBiomeMap map = eng_biome_map_default();
    bool sand = true;
    bool lava = true;
    eng_biome_cell_layers(&map, 0.0, &sand, &lava);
    EXPECT_FALSE(sand);
    EXPECT_FALSE(lava);
    eng_biome_cell_layers(&map, 1024.0, &sand, &lava);
    EXPECT_TRUE(sand);
    EXPECT_FALSE(lava);
    eng_biome_cell_layers(&map, 1792.0, &sand, &lava);
    EXPECT_TRUE(sand);
    EXPECT_TRUE(lava);
    for (int ix = 0; ix < 514; ix++) {
        const double x = -512.0 + (8.0 * (double)ix);
        const double cell_min = floor(x / 256.0) * 256.0;
        eng_biome_cell_layers(&map, cell_min, &sand, &lava);
        for (int iz = 0; iz < 33; iz++) {
            const double z = 0.0 + (37.0 * (double)iz);
            float w[ENG_BIOME_COUNT];
            eng_biome_weights(&map, x, z, w);
            if (w[ENG_BIOME_PLAIN] > 0.0F) {
                ASSERT_FALSE(sand); /* de l'herbe ici : le canal 0 reste de l'herbe */
            }
            if (w[ENG_BIOME_DESERT] > 0.0F) {
                ASSERT_TRUE(sand);
            }
            if (w[ENG_BIOME_SNOW] > 0.0F) {
                ASSERT_FALSE(lava); /* de la neige ici : le canal 3 reste de la neige */
            }
            if (w[ENG_BIOME_VOLCANO] > 0.0F) {
                ASSERT_TRUE(lava);
            }
        }
    }
    EngBiomeMap off = map;
    off.enabled = false;
    eng_biome_cell_layers(&off, 3000.0, &sand, &lava);
    EXPECT_FALSE(sand);
    EXPECT_FALSE(lava);
    eng_biome_cell_layers(NULL, 3000.0, &sand, &lava);
    EXPECT_FALSE(sand);
    eng_biome_cell_layers(&map, 3000.0, NULL, NULL); /* sorties absentes : sans effet */
}

UTEST(biome, lava_rivers_cover_a_small_part_of_the_ground_in_thin_bands)
{
    uint32_t core = 0;
    uint32_t any = 0;
    uint32_t total = 0;
    for (int ix = 0; ix < 400; ix++) {
        const double x = 2400.0 + (2.0 * (double)ix);
        for (int iz = 0; iz < 400; iz++) {
            const double z = 0.0 + (2.0 * (double)iz);
            const float r = eng_biome_lava_river(x, z);
            ASSERT_GE(r, 0.0F);
            ASSERT_LE(r, 1.0F);
            core += r >= 1.0F ? 1U : 0U;
            any += r > 0.0F ? 1U : 0U;
            total++;
        }
    }
    const float covered = (float)any / (float)total;
    EXPECT_GT(covered, 0.05F); /* il y a des coulées */
    EXPECT_LT(covered, 0.15F); /* mais le sol reste surtout du basalte */
    EXPECT_GT(core, 0U);
    EXPECT_LT(core, any);
    /* des coulées étroites, pas des lacs : aucun carré de 16 m entièrement en lave */
    uint32_t lakes = 0;
    for (int ix = 0; ix < 100; ix++) {
        const double x = 2400.0 + (8.0 * (double)ix);
        for (int iz = 0; iz < 100; iz++) {
            const double z = 0.0 + (8.0 * (double)iz);
            bool all = true;
            for (int idx = 0; idx < 5 && all; idx++) {
                const double dx = 0.0 + (4.0 * (double)idx);
                for (int idz = 0; idz < 5 && all; idz++) {
                    const double dz = 0.0 + (4.0 * (double)idz);
                    all = eng_biome_lava_river(x + dx, z + dz) > 0.0F;
                }
            }
            lakes += all ? 1U : 0U;
        }
    }
    EXPECT_EQ(lakes, 0U);
    EXPECT_EQ(eng_biome_lava_river(2500.0, 300.0), eng_biome_lava_river(2500.0, 300.0));
    EXPECT_EQ(eng_biome_lava_river(NAN, 0.0), 0.0F);
    EXPECT_EQ(eng_biome_lava_river(0.0, INFINITY), 0.0F);
}
