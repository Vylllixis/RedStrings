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
    EXPECT_NEAR(z[1].size_scale * 1.2F, 2.0F, 0.01F); /* brute x 1,2 : deux fois un soldat */
    EXPECT_STREQ(z[2].boss_title, "Chevalier noir");
    EXPECT_STREQ(z[11].boss_title, "Dragon de lave");
    EXPECT_TRUE(z[0].boss_title == NULL);
    /* boss spéciaux : le cavalier en plaine, le dragon au volcan ; les autres ordinaires */
    EXPECT_EQ(z[2].boss_kind, ENG_BOSSKIND_RIDER);
    EXPECT_EQ(z[11].boss_kind, ENG_BOSSKIND_DRAGON);
    EXPECT_STREQ(z[11].arch[0], "dragon");
    EXPECT_STREQ(z[2].arch[0], "boss");
    EXPECT_EQ(z[5].boss_kind, ENG_BOSSKIND_NONE);
    EXPECT_EQ(z[8].boss_kind, ENG_BOSSKIND_NONE);
    EXPECT_EQ(z[0].boss_kind, ENG_BOSSKIND_NONE);
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

/* Meutes (demande de Théo, 2026-11-22) : cinq petits monstres bien plus fragiles que l'ennemi
 * facile du biome, sans butin, de plus en plus solides d'un biome à l'autre. */
UTEST(eng_map_zones, a_pack_is_five_small_weak_enemies_of_its_biome)
{
    EngMapZone z[ENG_MAPZONE_COUNT];
    ASSERT_TRUE(eng_mapzones_build(NULL, z));
    EXPECT_EQ((uint32_t)ENG_MAPZONE_KINDS, 3U); /* les zones fixes : toujours trois par biome */
    EXPECT_EQ((uint32_t)ENG_MAPZONE_COUNT, 12U);
    float before = 0.0F;
    for (uint32_t b = 0; b < (uint32_t)ENG_BIOME_COUNT; b++) {
        const EngMapZone *easy = &z[(size_t)b * ENG_MAPZONES_PER_BIOME];
        EngMapZone p;
        memset(&p, 0x5A, sizeof p);
        ASSERT_TRUE(eng_mapzones_pack((EngBiome)b, 100.0 + (double)b, -50.0, &p));
        EXPECT_EQ(p.kind, ENG_MAPZONE_PACK);
        EXPECT_EQ((uint32_t)p.biome, b);
        EXPECT_NEAR(p.x, 100.0 + (double)b, 1e-9);
        EXPECT_NEAR(p.z, -50.0, 1e-9);
        EXPECT_NEAR(p.reach, 85.0, 1e-9);
        ASSERT_EQ(p.enemy_count, 5U);
        EXPECT_LE(p.enemy_count, ENG_MAPZONE_MAX_ENEMIES);
        for (uint32_t e = 0; e < p.enemy_count; e++) {
            EXPECT_STREQ(p.arch[e], "imp");
        }
        EXPECT_NEAR(p.hp, 0.4F * easy->hp, 0.01F * easy->hp); /* 12, 22, 38 et 60 PV */
        EXPECT_GT(p.hp, before);
        before = p.hp;
        EXPECT_NEAR(p.damage_scale, easy->damage_scale, 1e-6F);
        EXPECT_NEAR(p.size_scale, 1.0F, 1e-6F);
        EXPECT_EQ(p.reward, 0U);
        EXPECT_EQ(p.boss_kind, ENG_BOSSKIND_NONE);
        EXPECT_TRUE(p.boss_title == NULL);
        EXPECT_TRUE(p.name != NULL && strstr(p.name, "meute") != NULL);
    }
    EngMapZone plain;
    ASSERT_TRUE(eng_mapzones_pack(ENG_BIOME_PLAIN, 0.0, 0.0, &plain));
    EXPECT_NEAR(plain.hp, 12.0F, 1e-6F); /* deux attaques légères de 10 au niveau 1 */
    EXPECT_FALSE(eng_mapzones_pack(ENG_BIOME_PLAIN, 0.0, 0.0, NULL));
    EXPECT_FALSE(eng_mapzones_pack(ENG_BIOME_COUNT, 0.0, 0.0, &plain));
    EXPECT_FALSE(eng_mapzones_pack((EngBiome)-1, 0.0, 0.0, &plain));
    EXPECT_FALSE(eng_mapzones_pack(ENG_BIOME_PLAIN, NAN, 0.0, &plain));
    EXPECT_FALSE(eng_mapzones_pack(ENG_BIOME_PLAIN, 0.0, INFINITY, &plain));
    EXPECT_STREQ(eng_mapzone_kind_name(ENG_MAPZONE_PACK), "meute");
}

/* Chaque zone a sa portée : 110 m pour une zone fixe, 85 m pour une meute, 420 m pour l'antre du
 * dragon (dix fois plus grand, il vole : on le voit de loin). La zone active est la plus proche de
 * celles dont le joueur est à portée. */
UTEST(eng_map_zones, the_active_zone_is_the_nearest_one_within_its_own_reach)
{
    EngMapZone built[ENG_MAPZONE_COUNT];
    ASSERT_TRUE(eng_mapzones_build(NULL, built));
    for (uint32_t i = 0; i < ENG_MAPZONE_COUNT; i++) {
        EXPECT_NEAR(built[i].reach, i == 11U ? 420.0 : 110.0, 1e-9);
    }
    EngMapZone z[3];
    z[0] = built[0]; /* zone fixe en (0, 0) */
    z[0].x = 0.0;
    z[0].z = 0.0;
    ASSERT_TRUE(eng_mapzones_pack(ENG_BIOME_PLAIN, 150.0, 0.0, &z[1])); /* meute à 150 m */
    z[2] = built[11]; /* antre du dragon à 1000 m */
    z[2].x = 1000.0;
    z[2].z = 0.0;
    EXPECT_EQ(eng_mapzones_active(z, 3U, 0.0, 0.0), 0);
    EXPECT_EQ(eng_mapzones_active(z, 3U, 60.0, 0.0), 0); /* meute à 90 m : hors de sa portée */
    EXPECT_EQ(eng_mapzones_active(z, 3U, 70.0, 0.0), 0); /* les deux à portée : la zone à 70 m */
    EXPECT_EQ(eng_mapzones_active(z, 3U, 80.0, 0.0), 1); /* les deux à portée : la meute à 70 m */
    EXPECT_EQ(eng_mapzones_active(z, 3U, 120.0, 30.0), 1); /* zone fixe à 124 m : hors de portée */
    EXPECT_EQ(eng_mapzones_active(z, 3U, 235.0, 0.0), 1);  /* pile à 85 m de la meute */
    EXPECT_EQ(eng_mapzones_active(z, 3U, 236.0, 0.0), -1);
    EXPECT_EQ(eng_mapzones_active(z, 3U, 579.0, 0.0), -1); /* à 421 m du dragon */
    EXPECT_EQ(eng_mapzones_active(z, 3U, 580.0, 0.0), 2);  /* à 420 m : on le voit */
    EXPECT_EQ(eng_mapzones_active(z, 3U, 1000.0, 300.0), 2);
    EXPECT_EQ(eng_mapzones_active(z, 2U, 1000.0, 0.0), -1); /* le dragon n'est pas dans la liste */
    EXPECT_EQ(eng_mapzones_active(z, 0U, 0.0, 0.0), -1);
    EXPECT_EQ(eng_mapzones_active(NULL, 3U, 0.0, 0.0), -1);
    EXPECT_EQ(eng_mapzones_active(z, 3U, NAN, 0.0), -1);
    EXPECT_EQ(eng_mapzones_active(z, 3U, 0.0, INFINITY), -1);
    z[0].reach = NAN; /* portée absurde : la zone ne s'active jamais */
    EXPECT_EQ(eng_mapzones_active(z, 3U, 0.0, 0.0), -1);
    z[0].reach = -1.0;
    EXPECT_EQ(eng_mapzones_active(z, 3U, 0.0, 0.0), -1);
}
