/* Carte du monde dessinée à la main : lecture, relief, régions, chemins, eau, lave, aperçu. */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/world_map.h"
#include "utest.h"

static EngWorldMap g_map;
static EngWorldMap g_again;

static const char MAP[] = "# petite carte d'essai\n"
                          "monde 1024 4\n"
                          "eau 3\n"
                          "bordure 420 200\n"
                          "depart 0 0\n"
                          "region plaine 0 0 300\n"
                          "region montagne -300 -300 250\n"
                          "region desert -300 300 250\n"
                          "region volcan 300 -300 250\n"
                          "colline 0 0 60 9 20\n"
                          "aplanir 0 0 20\n"
                          "montagne -300 -300 150 200\n"
                          "volcan 300 -300 200 150 50 20\n"
                          "lac 150 150 40 5\n"
                          "riviere 16 4 -200 0 0 -100 200 0\n"
                          "chemin 8 0 0 -100 -100 -230 -230\n"
                          "lave 12 300 -380 300 -480\n"
                          "lave 12 -190 150 -110 150\n"
                          "chemin 8 -150 100 -150 200\n"
                          "foret chene -30 230 80 8\n"
                          "foret chene -60 30 90 8\n"
                          "chemin 8 -60 160 0 300\n"
                          "foret sapin -300 -200 90 8\n"
                          "arbres_epars acacia desert 30\n"
                          "zone plaine facile 100 0\n"
                          "zone volcan boss 300 -300\n";

static EngArena g_arena;

static bool build(EngWorldMap *m, const char *text)
{
    char err[160];
    if (!eng_worldmap_parse(m, text, strlen(text), err, sizeof err)) {
        return false;
    }
    if (g_arena.base == NULL) {
        (void)eng_arena_init(&g_arena, "test-carte", (size_t)64 << 20);
    }
    return eng_worldmap_build(m, &g_arena);
}

UTEST(world_map, it_reads_the_description_and_rejects_mistakes_with_the_line_number)
{
    char err[160];
    ASSERT_TRUE(eng_worldmap_parse(&g_map, MAP, strlen(MAP), err, sizeof err));
    EXPECT_EQ(g_map.side, 1024.0F);
    EXPECT_EQ(g_map.region_count, 4U);
    EXPECT_EQ(g_map.feature_count, 15U);
    EXPECT_EQ(g_map.features[2].kind, (uint8_t)ENG_MAPF_MOUNTAIN);
    EXPECT_EQ(g_map.features[6].point_count, 3U);
    EXPECT_TRUE(g_map.zones[ENG_BIOME_VOLCANO * 3 + ENG_MAPZONE_BOSS].used);
    EXPECT_FALSE(g_map.zones[ENG_BIOME_DESERT * 3 + ENG_MAPZONE_BOSS].used);
    static const char *const bad[] = {
        "monde 1024 4\nregion plaine 0 0 300\ncolline 0 0\n",  /* trop peu de nombres */
        "monde 1024 4\nregion plaine 0 0 300\nforet 1 2 3\n",  /* mot inconnu */
        "monde 1024 4\nregion marais 0 0 300\n",               /* biome inconnu */
        "monde 1024 4\nregion plaine 0 0 300\nchemin 8 0 0\n", /* un seul point */
        "monde 1000 3\nregion plaine 0 0 300\n",               /* côté non multiple */
        "region plaine 0 0 300\n",                             /* monde manquant */
        "monde 1024 4\n",                                      /* aucune région */
        "monde 1024 4\nregion plaine 0 0 300\nzone plaine boss 0 0\nzone plaine boss 1 1\n",
        "monde 1024 4\nregion plaine 0 0 300\ncolline 0 0 10 5 20\n", /* plateau > rayon */
        "monde 1024 4\nregion plaine 0 0 300\nlac 0 0 x 5\n",         /* nombre illisible */
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        err[0] = '\0';
        EXPECT_FALSE(eng_worldmap_parse(&g_again, bad[i], strlen(bad[i]), err, sizeof err));
        EXPECT_TRUE(strncmp(err, "ligne ", 6) == 0);
    }
    EXPECT_TRUE(strstr(err, "ligne 3") != NULL); /* le dernier : erreur à la ligne 3 */
}

UTEST(world_map, the_same_description_always_gives_the_same_map)
{
    ASSERT_TRUE(build(&g_map, MAP));
    ASSERT_TRUE(build(&g_again, MAP));
    ASSERT_EQ(g_map.size, 257U);
    const size_t n = (size_t)g_map.size * g_map.size;
    EXPECT_EQ(memcmp(g_map.heights, g_again.heights, n * sizeof(float)), 0);
    EXPECT_EQ(memcmp(g_map.weights, g_again.weights, n * 4U), 0);
    EXPECT_EQ(memcmp(g_map.lava, g_again.lava, n), 0);
}

UTEST(world_map, the_spawn_hill_is_flat_on_top_and_overlooks_the_plain)
{
    ASSERT_TRUE(build(&g_map, MAP));
    const float top = eng_worldmap_height(&g_map, 0.0, 0.0);
    /* sommet aplani : moins de 30 cm d'écart sur 8 m, du côté opposé au chemin qui descend */
    for (int i = 0; i <= 2; i++) {
        for (int j = 0; j <= 2; j++) {
            EXPECT_NEAR(eng_worldmap_height(&g_map, 4.0 * i, 4.0 * j), top, 0.3F);
        }
    }
    const float plain = eng_worldmap_height(&g_map, 100.0, 60.0);
    EXPECT_GT(top - plain, 5.0F);
    EXPECT_LT(top - plain, 15.0F);
}

UTEST(world_map, regions_mountains_volcano_border_water_and_lava_are_where_they_were_drawn)
{
    ASSERT_TRUE(build(&g_map, MAP));
    float w[ENG_BIOME_COUNT];
    float sum = 0.0F;
    eng_worldmap_weights(&g_map, 0.0, 0.0, w, NULL);
    EXPECT_GT(w[ENG_BIOME_PLAIN], 0.95F);
    eng_worldmap_weights(&g_map, -300.0, -300.0, w, NULL);
    EXPECT_GT(w[ENG_BIOME_SNOW], 0.95F);
    eng_worldmap_weights(&g_map, -300.0, 300.0, w, NULL);
    EXPECT_GT(w[ENG_BIOME_DESERT], 0.95F);
    eng_worldmap_weights(&g_map, 160.0, -160.0, w, NULL); /* entre plaine et volcan : mélange */
    for (int k = 0; k < ENG_BIOME_COUNT; k++) {
        sum += w[k];
    }
    EXPECT_NEAR(sum, 1.0F, 0.01F);
    EXPECT_GT(eng_worldmap_height(&g_map, -300.0, -300.0), 180.0F); /* le pic */
    /* volcan : bord du cratère plus haut que son fond plat */
    const float floor_h = eng_worldmap_height(&g_map, 300.0, -300.0);
    EXPECT_GT(eng_worldmap_height(&g_map, 350.0, -300.0) - floor_h, 12.0F);
    EXPECT_NEAR(eng_worldmap_height(&g_map, 310.0, -300.0), floor_h, 2.0F); /* détail de surface */
    EXPECT_GT(eng_worldmap_height(&g_map, 510.0, 0.0), 150.0F);             /* falaises du bord */
    EXPECT_GT(eng_worldmap_height(&g_map, 2000.0, 2000.0), 150.0F); /* hors carte : le bord */
    EXPECT_LT(eng_worldmap_height(&g_map, 150.0, 150.0), g_map.water - 3.0F); /* lac */
    EXPECT_LT(eng_worldmap_height(&g_map, 0.0, -100.0), g_map.water);         /* rivière */
    EXPECT_GT(eng_worldmap_lava(&g_map, 300.0, -430.0), 0.9F);
    EXPECT_EQ(eng_worldmap_lava(&g_map, 0.0, 0.0), 0.0F);
    /* la lave ne coule jamais sur un chemin (le chemin x = -150 croise la coulée en z = 150) */
    EXPECT_LT(eng_worldmap_lava(&g_map, -150.0, 150.0), 0.05F);
    EXPECT_GT(eng_worldmap_lava(&g_map, -186.0, 150.0), 0.5F);
}

UTEST(world_map, paths_are_walkable_and_marked)
{
    ASSERT_TRUE(build(&g_map, MAP));
    float path = 0.0F;
    float w[ENG_BIOME_COUNT];
    eng_worldmap_weights(&g_map, -100.0, -100.0, w, &path);
    EXPECT_GT(path, 0.9F);
    eng_worldmap_weights(&g_map, 100.0, -60.0, w, &path);
    EXPECT_EQ(path, 0.0F);
    /* le chemin monte au pied du pic à 30 % au plus, mesuré tous les 4 m le long de son axe */
    float worst = 0.0F;
    for (int i = 0; i < 24; i++) { /* de -160 à -228 : loin des berges de la rivière */
        const double a = -160.0 - (2.83 * i);
        const double b = a - 2.83;
        const float dh = eng_worldmap_height(&g_map, b, b) - eng_worldmap_height(&g_map, a, a);
        worst = fmaxf(worst, fabsf(dh) / 4.0F);
    }
    EXPECT_LE(worst, 0.31F);
    EXPECT_GT(worst, 0.1F); /* il monte vraiment */
}

UTEST(world_map, the_preview_shows_water_lava_zones_and_the_spawn)
{
    ASSERT_TRUE(build(&g_map, MAP));
    const uint32_t size = 256;
    uint8_t *rgb = malloc((size_t)size * size * 3U);
    ASSERT_TRUE(rgb != NULL);
    ASSERT_TRUE(eng_worldmap_render(&g_map, size, rgb));
    const uint8_t *spawn = &rgb[(((size_t)128 * size) + 128) * 3U];
    EXPECT_EQ(spawn[0], 255); /* départ : blanc */
    EXPECT_EQ(spawn[2], 255);
    const uint8_t *lake = &rgb[(((size_t)165 * size) + 165) * 3U]; /* (150, 150) */
    EXPECT_GT(lake[2], lake[0] + 40);
    const uint8_t *lava = &rgb[(((size_t)20 * size) + 203) * 3U]; /* (300, -430) */
    EXPECT_GT(lava[0], 200);
    EXPECT_LT(lava[2], 60);
    const uint8_t *boss = &rgb[(((size_t)53 * size) + 203) * 3U]; /* (300, -300) : rouge */
    EXPECT_GT(boss[0], 200);
    EXPECT_LT(boss[1], 60);
    EXPECT_FALSE(eng_worldmap_render(&g_map, 8U, rgb));
    EXPECT_FALSE(eng_worldmap_render(NULL, size, rgb));
    free(rgb);
    EXPECT_EQ(eng_worldmap_height(NULL, 0.0, 0.0), 0.0F);
    EXPECT_EQ(eng_worldmap_height(&g_map, NAN, 0.0), 0.0F);
}

static EngMapTree g_trees[8192];
static EngMapTree g_parts[8192];

UTEST(world_map, trees_grow_where_drawn_and_do_not_depend_on_the_cell_grid)
{
    ASSERT_TRUE(build(&g_map, MAP));
    const uint32_t n = eng_worldmap_trees(&g_map, -512.0, -512.0, 1024.0, g_trees, 8192U);
    uint32_t by_kind[ENG_TREE_KINDS] = {0};
    for (uint32_t i = 0; i < n; i++) {
        const EngMapTree *t = &g_trees[i];
        by_kind[t->kind]++;
        float w[ENG_BIOME_COUNT];
        float path = 0.0F;
        eng_worldmap_weights(&g_map, t->x, t->z, w, &path);
        EXPECT_LE(path, 0.05F);                        /* jamais sur un chemin */
        EXPECT_LE(w[ENG_BIOME_VOLCANO], 0.25F);        /* ni dans le volcan */
        EXPECT_GE(t->y, g_map.water + 0.8F);           /* ni dans l'eau */
        EXPECT_GE(hypotf(t->x, t->z), 70.0F);          /* ni contre le départ */
        EXPECT_GE(hypotf(t->x - 100.0F, t->z), 32.0F); /* ni dans une zone */
        EXPECT_NEAR(t->y, eng_worldmap_height(&g_map, t->x, t->z), 1e-3F); /* au sol */
        EXPECT_GT(t->scale, 4.0F);
        if (t->kind == ENG_TREE_ACACIA) {
            EXPECT_GE(w[ENG_BIOME_DESERT], 0.7F); /* les acacias restent dans le désert */
        }
    }
    EXPECT_GT(by_kind[ENG_TREE_OAK], 20U);
    EXPECT_GT(by_kind[ENG_TREE_PINE], 10U);
    EXPECT_GT(by_kind[ENG_TREE_ACACIA], 5U);
    /* les chênes restent dans leur forêt (disque de 80 m autour de (-30, 230)) */
    for (uint32_t i = 0; i < n; i++) {
        if (g_trees[i].kind == ENG_TREE_OAK) {
            EXPECT_TRUE(hypotf(g_trees[i].x + 30.0F, g_trees[i].z - 230.0F) < 80.0F ||
                        hypotf(g_trees[i].x + 60.0F, g_trees[i].z - 30.0F) < 90.0F);
        }
    }
    /* même placement découpé en quatre carrés de 512 m : les mêmes arbres, dans le même ordre par
     * carré */
    uint32_t total = 0;
    for (int q = 0; q < 4; q++) {
        total += eng_worldmap_trees(&g_map, -512.0 + (512.0 * (q & 1)), -512.0 + (256.0 * (q & 2)),
                                    512.0, &g_parts[total], 8192U - total);
    }
    EXPECT_EQ(total, n);
    uint32_t found = 0;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t k = 0; k < total; k++) {
            if (g_parts[k].x == g_trees[i].x && g_parts[k].z == g_trees[i].z &&
                g_parts[k].kind == g_trees[i].kind && g_parts[k].scale == g_trees[i].scale) {
                found++;
                break;
            }
        }
    }
    EXPECT_EQ(found, n);
    EXPECT_EQ(eng_worldmap_trees(&g_map, 0.0, 0.0, 512.0, g_trees, 3U) <= 3U, true); /* borne */
    EXPECT_EQ(eng_worldmap_trees(NULL, 0.0, 0.0, 512.0, g_trees, 10U), 0U);
    char err[160];
    static const char bad[] = "monde 1024 4\nregion plaine 0 0 300\nforet palmier 0 0 50 8\n";
    EXPECT_FALSE(eng_worldmap_parse(&g_again, bad, sizeof bad - 1U, err, sizeof err));
    EXPECT_TRUE(strstr(err, "ligne 3") != NULL);
}

static const char DECOR[] = "arbres_epars chene plaine 60 3\n"
                            "arbres_epars buisson plaine 24 3\n"
                            "arbres_epars rocher plaine 50 2\n"
                            "arbres_epars buisson_neige montagne 30 2\n"
                            "arbres_epars buisson_sec desert 30\n"
                            "arbres_epars rocher volcan 30 3\n";

UTEST(world_map, scattered_trees_bushes_and_rocks_fill_the_land_between_the_forests)
{
    static char text[4096];
    (void)snprintf(text, sizeof text, "%s%s", MAP, DECOR);
    ASSERT_TRUE(build(&g_map, MAP));
    const uint32_t before = eng_worldmap_trees(&g_map, -512.0, -512.0, 1024.0, g_parts, 8192U);
    ASSERT_TRUE(build(&g_again, text));
    const uint32_t n = eng_worldmap_trees(&g_again, -512.0, -512.0, 1024.0, g_trees, 8192U);
    ASSERT_GT(n, before + 60U);
    /* les lignes ajoutées APRÈS les autres ne déplacent aucun arbre déjà dessiné */
    for (uint32_t i = 0; i < before; i++) {
        ASSERT_EQ(g_trees[i].x, g_parts[i].x);
        ASSERT_EQ(g_trees[i].z, g_parts[i].z);
        ASSERT_EQ(g_trees[i].scale, g_parts[i].scale);
        ASSERT_EQ(g_trees[i].kind, g_parts[i].kind);
    }
    uint32_t by_kind[ENG_TREE_KINDS] = {0};
    uint32_t volcano_rocks = 0;
    uint32_t grouped_oaks = 0;
    for (uint32_t i = before; i < n; i++) {
        const EngMapTree *t = &g_trees[i];
        by_kind[t->kind]++;
        float w[ENG_BIOME_COUNT];
        float path = 0.0F;
        eng_worldmap_weights(&g_again, t->x, t->z, w, &path);
        EXPECT_LE(path, 0.05F);                                   /* jamais sur un chemin */
        EXPECT_GE(t->y, g_again.water + 0.8F);                    /* ni dans l'eau */
        EXPECT_EQ(eng_worldmap_lava(&g_again, t->x, t->z), 0.0F); /* ni sur la lave */
        EXPECT_GE(hypotf(t->x - 100.0F, t->z), 26.0F);            /* ni dans une zone */
        const bool rock = t->kind == ENG_TREE_ROCK;
        const bool bush = t->kind == ENG_TREE_BUSH || t->kind == ENG_TREE_BUSH_SNOW ||
                          t->kind == ENG_TREE_BUSH_DRY;
        EXPECT_GE(hypotf(t->x, t->z), rock ? 40.0F : (bush ? 34.0F : 70.0F)); /* ni au départ */
        EXPECT_EQ(eng_tree_is_solid((EngTreeKind)t->kind), !bush);
        if (!rock) {
            EXPECT_LE(w[ENG_BIOME_VOLCANO], 0.25F); /* rien ne pousse dans le volcan */
        }
        if (bush) {
            EXPECT_GE(t->scale, 0.79F); /* de 1 à 1,3 m, à 20 % près en moins, 25 % en plus */
            EXPECT_LE(t->scale, 1.63F);
        }
        if (rock) {
            EXPECT_GE(t->scale, 0.8F); /* de 0,55 à 1,75 fois 1,5 m */
            EXPECT_LE(t->scale, 2.65F);
            if (w[ENG_BIOME_VOLCANO] > 0.95F) { /* basalte sombre dans le volcan */
                volcano_rocks++;
                EXPECT_LT(t->tint[0], 120);
            }
            if (w[ENG_BIOME_PLAIN] > 0.95F) { /* roche grise, non teintée, dans la plaine */
                EXPECT_GT(t->tint[0], 220);
            }
        } else {
            EXPECT_GE(t->tint[0], 205); /* le reste varie un peu d'un pied à l'autre */
            EXPECT_EQ(t->tint[0], t->tint[1]);
        }
        /* par groupes : un autre chêne épars à moins de 7,5 m ? (sans groupes, deux emplacements
         * voisins de la grille de 60 m sont à 12 m au moins l'un de l'autre) */
        if (t->kind == ENG_TREE_OAK) {
            for (uint32_t k = before; k < n; k++) {
                if (k != i && g_trees[k].kind == ENG_TREE_OAK &&
                    hypotf(g_trees[k].x - t->x, g_trees[k].z - t->z) < 7.5F) {
                    grouped_oaks++;
                    break;
                }
            }
        }
    }
    EXPECT_GT(by_kind[ENG_TREE_OAK], 5U);
    EXPECT_GT(grouped_oaks, 1U);                    /* des groupes de deux ou trois */
    EXPECT_LT(grouped_oaks, by_kind[ENG_TREE_OAK]); /* et des arbres seuls */
    EXPECT_GT(by_kind[ENG_TREE_BUSH], 30U);
    EXPECT_GT(by_kind[ENG_TREE_BUSH_SNOW], 3U);
    EXPECT_GT(by_kind[ENG_TREE_BUSH_DRY], 3U);
    EXPECT_GT(by_kind[ENG_TREE_ROCK], 6U);
    EXPECT_GT(volcano_rocks, 2U); /* les rochers, eux, vont aussi dans le volcan */
    /* peu de rochers : bien moins que de buissons */
    EXPECT_LT(by_kind[ENG_TREE_ROCK], by_kind[ENG_TREE_BUSH]);
    /* même décor découpé en quatre carrés de 512 m (un groupe peut chevaucher deux carrés) */
    uint32_t total = 0;
    for (int q = 0; q < 4; q++) {
        total +=
            eng_worldmap_trees(&g_again, -512.0 + (512.0 * (q & 1)), -512.0 + (256.0 * (q & 2)),
                               512.0, &g_parts[total], 8192U - total);
    }
    EXPECT_EQ(total, n);
    uint32_t found = 0;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t k = 0; k < total; k++) {
            if (g_parts[k].x == g_trees[i].x && g_parts[k].z == g_trees[i].z &&
                g_parts[k].kind == g_trees[i].kind && g_parts[k].scale == g_trees[i].scale) {
                found++;
                break;
            }
        }
    }
    EXPECT_EQ(found, n);
    EXPECT_STREQ(eng_tree_name(ENG_TREE_ROCK), "rocher");
    EXPECT_STREQ(eng_tree_name(ENG_TREE_KINDS), "?");
    char err[160];
    static const char *const bad[] = {
        "monde 1024 4\nregion plaine 0 0 300\narbres_epars chene plaine 60 9\n", /* groupe > 4 */
        "monde 1024 4\nregion plaine 0 0 300\narbres_epars chene plaine 60 0\n",
        "monde 1024 4\nregion plaine 0 0 300\narbres_epars chene plaine 60 2 1\n",
        "monde 1024 4\nregion plaine 0 0 300\narbres_epars caillou plaine 60\n"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        err[0] = '\0';
        EXPECT_FALSE(eng_worldmap_parse(&g_map, bad[i], strlen(bad[i]), err, sizeof err));
        EXPECT_TRUE(strstr(err, "ligne 3") != NULL);
    }
}

static EngMapTuft g_tufts[8192];
static EngMapTuft g_tufts_b[8192];

UTEST(world_map, grass_covers_the_plain_but_not_paths_water_lava_or_other_biomes)
{
    ASSERT_TRUE(build(&g_map, MAP));
    EXPECT_EQ(eng_worldmap_grass_cover(&g_map, 60.0, 80.0), 1.0F);   /* pleine plaine */
    EXPECT_EQ(eng_worldmap_grass_cover(&g_map, -50.0, -50.0), 0.0F); /* sur le chemin */
    /* le chemin qui descend de la colline du départ vers (-100, -100) : rien sur son axe, alors
     * que la même pente de la colline, hors du chemin, est en herbe (c'est donc bien le chemin
     * qui l'écarte, pas l'eau ni la pente) */
    EXPECT_EQ(eng_worldmap_grass_cover(&g_map, -30.0, -30.0), 0.0F);
    EXPECT_EQ(eng_worldmap_grass_cover(&g_map, 30.0, 30.0), 1.0F);
    EXPECT_EQ(eng_worldmap_grass_cover(&g_map, -30.0, 30.0), 1.0F);
    EXPECT_EQ(eng_worldmap_grass_cover(&g_map, 150.0, 150.0), 0.0F);   /* dans le lac */
    EXPECT_EQ(eng_worldmap_grass_cover(&g_map, -150.0, 150.0), 0.0F);  /* sur la lave */
    EXPECT_EQ(eng_worldmap_grass_cover(&g_map, -300.0, -300.0), 0.0F); /* montagne */
    EXPECT_EQ(eng_worldmap_grass_cover(&g_map, -300.0, 300.0), 0.0F);  /* désert */
    EXPECT_EQ(eng_worldmap_grass_cover(&g_map, 300.0, -300.0), 0.0F);  /* volcan */
    EXPECT_EQ(eng_worldmap_grass_cover(&g_map, 600.0, 0.0), 0.0F);     /* hors de la carte */
    EXPECT_EQ(eng_worldmap_grass_cover(&g_map, NAN, 0.0), 0.0F);
    EXPECT_EQ(eng_worldmap_grass_cover(NULL, 60.0, 80.0), 0.0F);
    /* de la plaine vers le désert (-300, 300) : sur terrain plat, sec, sans chemin ni lave, la
     * couverture suit la part de plaine (1 au-dessus de 80 %, 0 sous 50 %) : pas de bord franc */
    bool partial = false;
    float last = 1.0F;
    for (int i = 0; i <= 120; i++) {
        const double x = 60.0 - (3.0 * i);
        const double z = 80.0 + ((220.0 / 120.0) * i);
        float w[ENG_BIOME_COUNT];
        float path = 0.0F;
        eng_worldmap_weights(&g_map, x, z, w, &path);
        const float h = eng_worldmap_height(&g_map, x, z);
        const float slope = 0.25F * hypotf(eng_worldmap_height(&g_map, x + 2.0, z) -
                                               eng_worldmap_height(&g_map, x - 2.0, z),
                                           eng_worldmap_height(&g_map, x, z + 2.0) -
                                               eng_worldmap_height(&g_map, x, z - 2.0));
        last = eng_worldmap_grass_cover(&g_map, x, z);
        if (path > 0.0F || eng_worldmap_lava(&g_map, x, z) > 0.0F || h < g_map.water + 0.3F ||
            slope > 0.4F) {
            continue;
        }
        const float expected = (w[ENG_BIOME_PLAIN] - 0.5F) / 0.3F;
        EXPECT_NEAR(last, expected < 0.0F ? 0.0F : (expected > 1.0F ? 1.0F : expected), 1e-4F);
        partial = partial || (last > 0.05F && last < 0.95F);
    }
    EXPECT_TRUE(partial);
    EXPECT_EQ(last, 0.0F); /* arrivé dans le désert */
}

UTEST(world_map, grass_tufts_are_always_at_the_same_places_whatever_the_centre)
{
    ASSERT_TRUE(build(&g_map, MAP));
    const uint32_t n =
        eng_worldmap_grass(&g_map, 60.0, 80.0, 20.0F, 0.5F, 0.4F, 1U, g_tufts, 8192U);
    /* disque de 20 m, une touffe par maille de 0,5 m : 5027 nœuds, tous gardés en pleine plaine */
    EXPECT_GT(n, 4850U);
    EXPECT_LT(n, 5200U);
    float lo = INFINITY;
    float hi = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        const EngMapTuft *t = &g_tufts[i];
        ASSERT_LE(hypotf(t->x - 60.0F, t->z - 80.0F), 20.001F);
        ASSERT_NEAR(t->y, eng_worldmap_height(&g_map, t->x, t->z), 1e-3F);
        ASSERT_GE(t->scale, 0.4F * 0.55F - 1e-5F);
        ASSERT_LE(t->scale, 0.4F * 1.4F + 1e-5F);
        ASSERT_GE(t->tint[1], 203); /* de 80 à 100 % */
        ASSERT_LE(t->tint[0], t->tint[1]);
        ASSERT_LT(t->tint[2], t->tint[1]);
        lo = fminf(lo, t->scale);
        hi = fmaxf(hi, t->scale);
    }
    EXPECT_LT(lo, 0.4F * 0.75F); /* des taches d'herbe rase et d'herbe haute */
    EXPECT_GT(hi, 0.4F * 1.2F);
    /* un autre centre, 10 m plus loin : dans la partie commune, exactement les mêmes touffes */
    const uint32_t m =
        eng_worldmap_grass(&g_map, 70.0, 80.0, 20.0F, 0.5F, 0.4F, 1U, g_tufts_b, 8192U);
    uint32_t shared = 0;
    uint32_t found = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (hypot((double)g_tufts[i].x - 70.0, (double)g_tufts[i].z - 80.0) > 19.99) {
            continue;
        }
        shared++;
        for (uint32_t k = 0; k < m; k++) {
            if (g_tufts_b[k].x == g_tufts[i].x && g_tufts_b[k].z == g_tufts[i].z &&
                g_tufts_b[k].scale == g_tufts[i].scale && g_tufts_b[k].yaw == g_tufts[i].yaw &&
                g_tufts_b[k].tint[0] == g_tufts[i].tint[0]) {
                found++;
                break;
            }
        }
    }
    EXPECT_GT(shared, 2500U);
    EXPECT_EQ(found, shared);
    /* un autre semis (salt) : d'autres places */
    const uint32_t other =
        eng_worldmap_grass(&g_map, 60.0, 80.0, 20.0F, 0.5F, 0.4F, 2U, g_tufts_b, 8192U);
    EXPECT_GT(other, 4850U);
    EXPECT_TRUE(g_tufts_b[0].x != g_tufts[0].x || g_tufts_b[0].z != g_tufts[0].z);
    /* un pas deux fois plus grand : quatre fois moins de touffes */
    const uint32_t sparse =
        eng_worldmap_grass(&g_map, 60.0, 80.0, 20.0F, 1.0F, 0.4F, 1U, g_tufts_b, 8192U);
    EXPECT_NEAR((float)sparse, (float)n / 4.0F, 80.0F);
    /* aucune touffe hors de la plaine, sur le lac ; la borne `max` est respectée */
    EXPECT_EQ(eng_worldmap_grass(&g_map, -300.0, -300.0, 20.0F, 0.5F, 0.4F, 1U, g_tufts_b, 8192U),
              0U);
    EXPECT_EQ(eng_worldmap_grass(&g_map, 150.0, 150.0, 10.0F, 0.5F, 0.4F, 1U, g_tufts_b, 8192U),
              0U);
    EXPECT_EQ(eng_worldmap_grass(&g_map, 60.0, 80.0, 20.0F, 0.5F, 0.4F, 1U, g_tufts_b, 100U), 100U);
    /* arguments invalides */
    EXPECT_EQ(eng_worldmap_grass(NULL, 60.0, 80.0, 20.0F, 0.5F, 0.4F, 1U, g_tufts_b, 8192U), 0U);
    EXPECT_EQ(eng_worldmap_grass(&g_map, 60.0, 80.0, 20.0F, 0.5F, 0.4F, 1U, NULL, 8192U), 0U);
    EXPECT_EQ(eng_worldmap_grass(&g_map, NAN, 80.0, 20.0F, 0.5F, 0.4F, 1U, g_tufts_b, 8192U), 0U);
    EXPECT_EQ(eng_worldmap_grass(&g_map, 60.0, 80.0, 0.0F, 0.5F, 0.4F, 1U, g_tufts_b, 8192U), 0U);
    EXPECT_EQ(eng_worldmap_grass(&g_map, 60.0, 80.0, 500.0F, 0.5F, 0.4F, 1U, g_tufts_b, 8192U), 0U);
    EXPECT_EQ(eng_worldmap_grass(&g_map, 60.0, 80.0, 20.0F, 0.1F, 0.4F, 1U, g_tufts_b, 8192U), 0U);
    EXPECT_EQ(eng_worldmap_grass(&g_map, 60.0, 80.0, 20.0F, 0.5F, 0.0F, 1U, g_tufts_b, 8192U), 0U);
}

UTEST(world_map, zones_take_their_place_from_the_map)
{
    ASSERT_TRUE(build(&g_map, MAP));
    EngMapZone zones[ENG_MAPZONE_COUNT];
    ASSERT_TRUE(eng_worldmap_zones(&g_map, zones));
    EXPECT_NEAR(zones[0].x, 100.0, 1e-9); /* plaine facile, placée par la carte */
    EXPECT_NEAR(zones[0].z, 0.0, 1e-9);
    EXPECT_NEAR(zones[11].x, 300.0, 1e-9);                /* volcan boss */
    EXPECT_STREQ(zones[11].boss_title, "Dragon de lave"); /* le reste vient de map_zones */
    EXPECT_FALSE(eng_worldmap_zones(NULL, zones));
}

UTEST(world_map, the_water_surface_covers_the_lake_and_the_river_and_nothing_else)
{
    ASSERT_TRUE(build(&g_map, MAP));
    EngModel water;
    ASSERT_TRUE(eng_worldmap_water(&g_map, &g_arena, &water));
    const uint32_t wet = eng_worldmap_wet_cells(&g_map);
    ASSERT_GT(wet, 200U); /* lac de 40 m de rayon (~310 cellules de 4 m) et rivière */
    EXPECT_EQ(water.index_count, wet * 6U);
    EXPECT_LT(water.vertex_count, wet * 4U); /* sommets partagés entre cellules voisines */
    EXPECT_GT(water.vertex_count, wet);
    float deepest = 0.0F;
    bool lake_center = false;
    for (uint32_t i = 0; i < water.vertex_count; i++) {
        const EngModelVertex *v = &water.vertices[i];
        EXPECT_EQ(v->position[1], g_map.water);
        EXPECT_EQ(v->normal[1], 1.0F);
        const float depth =
            g_map.water - eng_worldmap_height(&g_map, v->position[0], v->position[2]);
        EXPECT_NEAR(v->uv[0], depth, 1e-4F); /* signée : négative où le relief dépasse */
        deepest = fmaxf(deepest, v->uv[0]);
        lake_center = lake_center || (fabsf(v->position[0] - 150.0F) < 2.1F &&
                                      fabsf(v->position[2] - 150.0F) < 2.1F);
    }
    bool above = false; /* des sommets de rive au-dessus de l'eau (profondeur négative) */
    for (uint32_t i = 0; i < water.vertex_count; i++) {
        above = above || water.vertices[i].uv[0] < -0.05F;
    }
    EXPECT_TRUE(above);
    EXPECT_TRUE(lake_center);         /* le milieu du lac (150, 150) est couvert */
    EXPECT_NEAR(deepest, 5.0F, 0.6F); /* lac de 5 m de fond */
    /* chaque triangle a un sommet sous l'eau à moins d'une cellule : pas d'eau sur la terre ferme
     */
    for (uint32_t k = 0; k < water.index_count; k += 3U) {
        const EngModelVertex *v = &water.vertices[water.indices[k]];
        ASSERT_TRUE(eng_worldmap_near_water(&g_map, v->position[0], v->position[2], 4.01));
        /* orientation : normale géométrique vers le haut */
        const float *a = water.vertices[water.indices[k]].position;
        const float *b = water.vertices[water.indices[k + 1U]].position;
        const float *c = water.vertices[water.indices[k + 2U]].position;
        ASSERT_GT(((b[2] - a[2]) * (c[0] - a[0])) - ((b[0] - a[0]) * (c[2] - a[2])), 0.0F);
    }
    EXPECT_TRUE(eng_worldmap_near_water(&g_map, 150.0, 150.0, 0.0));   /* dans le lac */
    EXPECT_TRUE(eng_worldmap_near_water(&g_map, 150.0, 200.0, 16.0));  /* à 10 m de sa rive */
    EXPECT_FALSE(eng_worldmap_near_water(&g_map, 150.0, 260.0, 16.0)); /* loin */
    EXPECT_FALSE(eng_worldmap_near_water(NULL, 0.0, 0.0, 10.0));
    /* carte sans eau : modèle vide */
    static const char dry[] = "monde 256 4\neau -50\nregion plaine 0 0 300\n";
    char err[160];
    ASSERT_TRUE(eng_worldmap_parse(&g_again, dry, sizeof dry - 1U, err, sizeof err));
    ASSERT_TRUE(eng_worldmap_build(&g_again, &g_arena));
    ASSERT_TRUE(eng_worldmap_water(&g_again, &g_arena, &water));
    EXPECT_EQ(water.vertex_count, 0U);
    EXPECT_FALSE(eng_worldmap_water(NULL, &g_arena, &water));
}
