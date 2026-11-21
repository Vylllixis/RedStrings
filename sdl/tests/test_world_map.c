/* Carte du monde dessinée à la main : lecture, relief, régions, chemins, eau, lave, aperçu. */
#include <math.h>
#include <stdbool.h>
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
    EXPECT_EQ(g_map.feature_count, 10U);
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
