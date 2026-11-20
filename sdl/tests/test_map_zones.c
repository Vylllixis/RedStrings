/* Zones de la grande carte : trois par biome (farm facile, farm dur, boss), dans l'ordre du chemin,
 * de difficulté croissante, chacune dans la bande de son biome. */
#include <math.h>
#include <string.h>

#include "engine/map_zones.h"
#include "utest.h"

UTEST(eng_map_zones, twelve_zones_follow_the_path_three_per_biome_in_their_band)
{
    const EngBiomeMap map = eng_biome_map_default();
    EngMapZone z[ENG_MAPZONE_COUNT];
    ASSERT_TRUE(eng_mapzones_build(&map, z));
    for (uint32_t i = 0; i < ENG_MAPZONE_COUNT; i++) {
        EXPECT_EQ((uint32_t)z[i].biome, i / ENG_MAPZONES_PER_BIOME);
        EXPECT_EQ((uint32_t)z[i].kind, i % ENG_MAPZONES_PER_BIOME);
        /* au centre de la zone, le biome de la carte est bien le sien, pur à 95 % au moins */
        float w[ENG_BIOME_COUNT];
        eng_biome_weights(&map, z[i].x, z[i].z, w);
        EXPECT_GT(w[z[i].biome], 0.95F);
        EXPECT_TRUE(z[i].name != NULL && strlen(z[i].name) > 5U);
        EXPECT_GE(z[i].enemy_count, 1U);
        EXPECT_LE(z[i].enemy_count, ENG_MAPZONE_MAX_ENEMIES);
        for (uint32_t e = 0; e < z[i].enemy_count; e++) {
            EXPECT_TRUE(z[i].arch[e] != NULL);
        }
        if (i > 0U) {
            EXPECT_GT(z[i].x, z[i - 1U].x + 150.0); /* dans l'ordre, espacées */
        }
    }
    EXPECT_FALSE(eng_mapzones_build(&map, NULL));
    EngBiomeMap bad = map;
    bad.blend_m = 0.0F;
    EXPECT_FALSE(eng_mapzones_build(&bad, z));
    ASSERT_TRUE(eng_mapzones_build(NULL, z)); /* carte par défaut */
    EXPECT_NEAR((float)z[0].x, 240.0F, 1e-3F);
}

UTEST(eng_map_zones, the_plain_matches_the_requested_hit_counts_and_difficulty_only_rises)
{
    EngMapZone z[ENG_MAPZONE_COUNT];
    ASSERT_TRUE(eng_mapzones_build(NULL, z));
    /* plaine, niveau 1, une attaque légère = 10 dégâts : 3 coups, 6 coups, 20 coups */
    EXPECT_NEAR(z[0].hp / 10.0F, 3.0F, 1e-6F);
    EXPECT_NEAR(z[1].hp / 10.0F, 6.0F, 1e-6F);
    EXPECT_NEAR(z[2].hp / 10.0F, 20.0F, 1e-6F);
    EXPECT_NEAR(z[1].size_scale, 2.0F, 1e-6F); /* les durs : deux fois plus grands */
    EXPECT_STREQ(z[0].arch[0], "grunt");
    EXPECT_STREQ(z[1].arch[0], "brute");
    EXPECT_STREQ(z[2].arch[0], "boss");
    for (uint32_t b = 1; b < (uint32_t)ENG_BIOME_COUNT; b++) {
        for (uint32_t k = 0; k < ENG_MAPZONES_PER_BIOME; k++) {
            const EngMapZone *now = &z[((size_t)b * ENG_MAPZONES_PER_BIOME) + k];
            const EngMapZone *before = &z[(((size_t)b - 1U) * ENG_MAPZONES_PER_BIOME) + k];
            EXPECT_GT(now->hp, before->hp);
            EXPECT_GT(now->damage_scale, before->damage_scale);
        }
    }
    for (uint32_t b = 0; b < (uint32_t)ENG_BIOME_COUNT;
         b++) { /* dans un biome : facile < dur < boss */
        const EngMapZone *e = &z[(size_t)b * ENG_MAPZONES_PER_BIOME];
        EXPECT_LT(e[0].hp, e[1].hp);
        EXPECT_LT(e[1].hp, e[2].hp);
        EXPECT_EQ(e[0].reward, 0U);
        EXPECT_EQ(e[1].reward, 0U);
        EXPECT_EQ(e[2].reward, b + 1U); /* chaque boss donne le palier de son biome */
    }
    EXPECT_STREQ(eng_mapzone_kind_name(ENG_MAPZONE_BOSS), "boss");
}

UTEST(eng_map_zones, the_nearest_zone_is_found_within_the_distance_only)
{
    EngMapZone z[ENG_MAPZONE_COUNT];
    ASSERT_TRUE(eng_mapzones_build(NULL, z));
    EXPECT_EQ(eng_mapzones_nearest(z, z[4].x + 10.0, z[4].z - 20.0, 100.0), 4);
    EXPECT_EQ(eng_mapzones_nearest(z, z[4].x + 104.0, z[4].z, 100.0), -1); /* à 104 et 131 m */
    EXPECT_EQ(eng_mapzones_nearest(z, z[0].x, z[0].z, 0.0), 0);            /* pile dessus */
    EXPECT_EQ(eng_mapzones_nearest(z, -5000.0, 0.0, 100.0), -1);
    EXPECT_EQ(eng_mapzones_nearest(NULL, 0.0, 0.0, 100.0), -1);
    EXPECT_EQ(eng_mapzones_nearest(z, NAN, 0.0, 100.0), -1);
    /* entre deux zones : la plus proche des deux */
    const double mid = 0.5 * (z[0].x + z[1].x);
    EXPECT_EQ(eng_mapzones_nearest(z, mid - 5.0, 128.0, 1000.0), 0);
    EXPECT_EQ(eng_mapzones_nearest(z, mid + 30.0, 128.0, 1000.0), 1);
}
