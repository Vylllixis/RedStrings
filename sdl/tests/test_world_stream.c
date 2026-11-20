/* Monde ouvert, partie CPU (M7-01, M7-03, M7-10, M7-12) : cellules, streaming, origine flottante,
 * persistance. */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/world_cell.h"
#include "engine/world_origin.h"
#include "engine/world_state.h"
#include "engine/world_stream.h"
#include "utest.h"

static EngCellData g_a;
static EngCellData g_b;

UTEST(world_cell, neighbours_share_their_edge_exactly_and_generation_is_pure)
{
    const EngWorldGen gen = eng_world_gen_default(7);
    const EngCellCoord ca = {3, -2};
    const EngCellCoord cb = {4, -2}; /* voisine en +X */
    ASSERT_TRUE(eng_cell_generate(&gen, ca, &g_a));
    ASSERT_TRUE(eng_cell_generate(&gen, cb, &g_b));
    for (uint32_t z = 0; z < ENG_CELL_RES; z++) {
        EXPECT_EQ(g_a.heights[(size_t)z * ENG_CELL_RES + ENG_CELL_RES - 1U],
                  g_b.heights[(size_t)z * ENG_CELL_RES]);
    }
    const EngCellCoord cc = {3, -1}; /* voisine en +Z */
    ASSERT_TRUE(eng_cell_generate(&gen, cc, &g_b));
    for (uint32_t x = 0; x < ENG_CELL_RES; x++) {
        EXPECT_EQ(g_a.heights[(size_t)(ENG_CELL_RES - 1U) * ENG_CELL_RES + x], g_b.heights[x]);
    }
    const uint64_t h1 = eng_cell_hash(&g_a);
    ASSERT_TRUE(eng_cell_generate(&gen, ca, &g_a));
    EXPECT_EQ(eng_cell_hash(&g_a), h1);
    const EngWorldGen other = eng_world_gen_default(8);
    ASSERT_TRUE(eng_cell_generate(&other, ca, &g_b));
    EXPECT_NE(eng_cell_hash(&g_b), h1);
    EXPECT_GT(g_a.height_max - g_a.height_min, 1.0F); /* il y a du relief */
    EXPECT_GE(g_a.object_count, gen.rocks_per_cell + gen.trees_per_cell);
    for (uint32_t i = 0; i < g_a.object_count; i++) {
        EXPECT_GE(g_a.objects[i].pos[0], 0.0F);
        EXPECT_LE(g_a.objects[i].pos[0], 256.0F);
        EXPECT_NEAR(g_a.objects[i].y,
                    eng_cell_height(&g_a, g_a.objects[i].pos[0], g_a.objects[i].pos[1]), 1e-4F);
    }
}

UTEST(world_cell, cell_of_handles_negative_coordinates_and_far_positions)
{
    EXPECT_EQ(eng_cell_of(0.0, 0.0).x, 0);
    EXPECT_EQ(eng_cell_of(255.9, 0.0).x, 0);
    EXPECT_EQ(eng_cell_of(256.0, 0.0).x, 1);
    EXPECT_EQ(eng_cell_of(-0.1, 0.0).x, -1);
    EXPECT_EQ(eng_cell_of(-256.0, -256.1).z, -2);
    EXPECT_EQ(eng_cell_of(10240.5, 0.0).x, 40); /* 10 km */
    EXPECT_EQ(eng_cell_of(1.0e6, 0.0).x, 3906);
}

UTEST(world_cell, a_64_cell_world_round_trips_through_the_rscl_format)
{
    const EngWorldGen gen = eng_world_gen_default(3);
    static uint8_t buf[32768];
    uint32_t cells = 0;
    for (int32_t z = -4; z < 4; z++) {
        for (int32_t x = -4; x < 4; x++) {
            const EngCellCoord c = {x, z};
            ASSERT_TRUE(eng_cell_generate(&gen, c, &g_a));
            const size_t n = eng_cell_encode(&g_a, buf, sizeof buf);
            ASSERT_EQ(n, eng_cell_encoded_size(&g_a));
            ASSERT_TRUE(eng_cell_decode(buf, n, &g_b));
            EXPECT_EQ(eng_cell_hash(&g_b), eng_cell_hash(&g_a));
            EXPECT_EQ(g_b.coord.x, x);
            EXPECT_EQ(g_b.coord.z, z);
            cells++;
        }
    }
    EXPECT_EQ(cells, 64U);
    /* corruptions refusées */
    const size_t n = eng_cell_encode(&g_a, buf, sizeof buf);
    EXPECT_FALSE(eng_cell_decode(buf, n - 1U, &g_b));
    buf[0] = 'X';
    EXPECT_FALSE(eng_cell_decode(buf, n, &g_b));
    buf[0] = 'R';
    buf[4] = 2; /* version */
    EXPECT_FALSE(eng_cell_decode(buf, n, &g_b));
    buf[4] = 1;
    EXPECT_TRUE(eng_cell_decode(buf, n, &g_b));
    EXPECT_EQ(eng_cell_encode(&g_a, buf, 10), 0U);
    EXPECT_FALSE(eng_cell_generate(NULL, (EngCellCoord){0, 0}, &g_b));
}

UTEST(world_cell, bilinear_height_matches_samples_and_clamps)
{
    const EngWorldGen gen = eng_world_gen_default(5);
    ASSERT_TRUE(eng_cell_generate(&gen, (EngCellCoord){0, 0}, &g_a));
    EXPECT_NEAR(eng_cell_height(&g_a, 8.0F, 12.0F), g_a.heights[3 * ENG_CELL_RES + 2], 1e-5F);
    const float mid = 0.5F * (g_a.heights[0] + g_a.heights[1]);
    EXPECT_NEAR(eng_cell_height(&g_a, 2.0F, 0.0F), mid, 1e-5F);
    EXPECT_NEAR(eng_cell_height(&g_a, -10.0F, -10.0F), g_a.heights[0], 1e-6F);
    EXPECT_NEAR(eng_cell_height(&g_a, 999.0F, 999.0F),
                g_a.heights[ENG_CELL_RES * ENG_CELL_RES - 1U], 1e-6F);
    EXPECT_NEAR(eng_cell_height(NULL, 1.0F, 1.0F), 0.0F, 1e-9F);
}

/* ── streaming ── */

static EngJobs g_jobs;
static EngWorldStream g_stream;

typedef struct Counts {
    uint32_t ready;
    uint32_t unload;
} Counts;

static void on_ready(void *user, const EngCellData *c)
{
    (void)c;
    ((Counts *)user)->ready++;
}

static void on_unload(void *user, const EngCellData *c)
{
    (void)c;
    ((Counts *)user)->unload++;
}

UTEST(world_stream, a_fast_5km_traversal_never_leaves_a_hole_and_frames_stay_under_budget)
{
    ASSERT_TRUE(eng_jobs_init(&g_jobs, 3));
    const EngWorldGen gen = eng_world_gen_default(11);
    ASSERT_TRUE(eng_stream_init(&g_stream, &gen, &g_jobs, 2, NULL));
    Counts counts = {0};
    g_stream.on_ready = on_ready;
    g_stream.on_unload = on_unload;
    g_stream.user = &counts;
    double x = 100.0;
    double z = 100.0;
    /* préchargement (écran de démarrage) */
    for (int i = 0; i < 2000 && !eng_stream_ready_around(&g_stream, x, z, 2); i++) {
        (void)eng_stream_update(&g_stream, x, z);
        SDL_Delay(1);
    }
    ASSERT_TRUE(eng_stream_ready_around(&g_stream, x, z, 2));
    /* 5 km à 50 m/s (7 x le sprint), en diagonale, 60 images/s */
    const double speed = 50.0 / 60.0;
    uint32_t holes = 0;
    double worst_ms = 0.0;
    const uint32_t frames = (uint32_t)(5000.0 / speed);
    for (uint32_t f = 0; f < frames; f++) {
        x += speed * 0.8;
        z += speed * 0.6;
        const uint64_t t0 = SDL_GetPerformanceCounter();
        (void)eng_stream_update(&g_stream, x, z);
        const double ms = (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 /
                          (double)SDL_GetPerformanceFrequency();
        worst_ms = ms > worst_ms ? ms : worst_ms;
        if (!eng_stream_ready_around(&g_stream, x, z, 1)) { /* le sol sous et autour du joueur */
            holes++;
        }
        float h = 0.0F;
        if (!eng_stream_height(&g_stream, x, z, &h)) {
            holes++;
        }
        SDL_DelayNS(16666667ULL / 8ULL); /* une image « simulée » laisse respirer les threads */
    }
    (void)fprintf(stderr,
                  "stream: frames=%u loads=%llu unloads=%llu peak_slots=%u worst_update=%.3f ms "
                  "holes=%u mem=%zu Ko\n",
                  frames, (unsigned long long)g_stream.loads_done,
                  (unsigned long long)g_stream.unloads, g_stream.peak_slots, worst_ms, holes,
                  eng_stream_memory() / 1024U);
    EXPECT_EQ(holes, 0U);
    EXPECT_LT(worst_ms, 25.0);
    EXPECT_LE(g_stream.peak_slots, ENG_STREAM_SLOTS);
    EXPECT_LE(g_stream.loaded, 49U);  /* (2r+1)² au plus une fois stabilisé, + hystérésis */
    EXPECT_GT(g_stream.unloads, 50U); /* on a bien déchargé derrière soi */
    EXPECT_LE(
        counts.ready,
        (uint32_t)g_stream.loads_done); /* une cellule libérée en vol ne devient jamais prête */
    EXPECT_EQ(g_stream.refused, 0U);
    eng_stream_shutdown(&g_stream);
    EXPECT_EQ(g_stream.loaded, 0U);
    EXPECT_EQ(counts.ready, counts.unload); /* tout ce qui a été prêt a été libéré */
    eng_jobs_shutdown(&g_jobs);
}

UTEST(world_stream, hysteresis_a_back_and_forth_on_a_border_reloads_nothing)
{
    ASSERT_TRUE(eng_jobs_init(&g_jobs, 2));
    const EngWorldGen gen = eng_world_gen_default(1);
    ASSERT_TRUE(eng_stream_init(&g_stream, &gen, &g_jobs, 1, NULL));
    for (int i = 0; i < 2000 && !eng_stream_ready_around(&g_stream, 255.0, 128.0, 1); i++) {
        (void)eng_stream_update(&g_stream, 255.0, 128.0);
        SDL_Delay(1);
    }
    for (int i = 0; i < 2000 && !eng_stream_ready_around(&g_stream, 257.0, 128.0, 1); i++) {
        (void)eng_stream_update(&g_stream, 257.0, 128.0);
        SDL_Delay(1);
    }
    const uint64_t loads = g_stream.loads_started;
    const uint64_t unloads = g_stream.unloads;
    for (int k = 0; k < 50; k++) { /* va-et-vient sur la frontière x = 256 */
        (void)eng_stream_update(&g_stream, (k % 2) != 0 ? 255.0 : 257.0, 128.0);
    }
    EXPECT_EQ(g_stream.loads_started, loads);
    EXPECT_EQ(g_stream.unloads, unloads);
    eng_stream_shutdown(&g_stream);
    eng_jobs_shutdown(&g_jobs);
}

UTEST(world_stream, cells_are_read_from_files_when_present_and_generated_otherwise)
{
    char dir[512];
    (void)snprintf(dir, sizeof dir, "%s", "tests_tmp_world");
    (void)SDL_CreateDirectory(dir);
    const EngWorldGen gen = eng_world_gen_default(4);
    const EngCellCoord c = {0, 0};
    ASSERT_TRUE(eng_cell_generate(&gen, c, &g_a));
    g_a.heights[0] = 1234.0F; /* « édité » : différent de la génération */
    static uint8_t buf[32768];
    const size_t n = eng_cell_encode(&g_a, buf, sizeof buf);
    char path[600];
    (void)snprintf(path, sizeof path, "%s/cell_0_0.rscl", dir);
    ASSERT_TRUE(SDL_SaveFile(path, buf, n));
    ASSERT_TRUE(eng_jobs_init(&g_jobs, 2));
    ASSERT_TRUE(eng_stream_init(&g_stream, &gen, &g_jobs, 1, dir));
    for (int i = 0; i < 2000 && !eng_stream_ready_around(&g_stream, 10.0, 10.0, 1); i++) {
        (void)eng_stream_update(&g_stream, 10.0, 10.0);
        SDL_Delay(1);
    }
    const EngCellData *cell = eng_stream_cell(&g_stream, c);
    ASSERT_NE(cell, NULL);
    EXPECT_NEAR(cell->heights[0], 1234.0F, 1e-6F);
    EXPECT_EQ(g_stream.files_read, 1U); /* les 8 voisines sont générées */
    EXPECT_EQ(g_stream.loaded, 9U);
    eng_stream_shutdown(&g_stream);
    eng_jobs_shutdown(&g_jobs);
    (void)SDL_RemovePath(path);
    (void)SDL_RemovePath(dir);
    EXPECT_FALSE(eng_stream_init(&g_stream, &gen, &g_jobs, 0, NULL));
    EXPECT_FALSE(eng_stream_init(&g_stream, &gen, &g_jobs, 4, NULL));
    EXPECT_EQ(eng_stream_update(NULL, 0.0, 0.0), 0U);
}

/* ── origine flottante ── */

UTEST(world_origin, at_10km_local_floats_keep_sub_millimetre_precision)
{
    EngWorldOrigin o;
    eng_origin_init(&o, 512.0);
    double player[3] = {10000.123456, 3.0, -10000.654321};
    float shift[3];
    EXPECT_TRUE(eng_origin_update(&o, player, shift));
    EXPECT_NEAR(o.x, 9984.0, 1e-9); /* coin de cellule le plus proche (39 x 256) */
    EXPECT_NEAR(o.z, -9984.0, 1e-9);
    float local[3];
    eng_origin_to_local(&o, player, local);
    double back[3];
    eng_origin_to_world(&o, local, back);
    EXPECT_LT(fabs(back[0] - player[0]), 2e-6); /* ~16 m de l'origine : ulp float ~ 2 µm */
    EXPECT_LT(fabs(back[2] - player[2]), 2e-6);
    /* sans origine flottante : le float le plus proche de 10 000,123456 m est à 0,41 mm (pas de
     * 0,98 mm à cette distance) ; avec, l'erreur est de quelques micromètres */
    const double naive_err = fabs((double)(float)player[0] - player[0]);
    EXPECT_GT(naive_err, 1e-4);
    EXPECT_LT(fabs(back[0] - player[0]) * 50.0, naive_err);
    EXPECT_FALSE(eng_origin_update(&o, player, shift)); /* pas de nouveau saut sur place */
    EXPECT_NEAR(shift[0], 0.0F, 1e-9F);
}

UTEST(world_origin, shift_moves_local_positions_consistently_and_height_is_never_rebased)
{
    EngWorldOrigin o;
    eng_origin_init(&o, 100.0);
    const double enemy_world[3] = {250.0, 12.0, 30.0};
    float enemy_local[3];
    eng_origin_to_local(&o, enemy_world, enemy_local);
    const double player[3] = {300.0, 5.0, 0.0};
    float shift[3];
    ASSERT_TRUE(eng_origin_update(&o, player, shift));
    EXPECT_NEAR(o.x, 256.0, 1e-9);
    EXPECT_NEAR(o.y, 0.0, 1e-9);
    EXPECT_NEAR(shift[0], -256.0F, 1e-6F);
    EXPECT_NEAR(shift[1], 0.0F, 1e-9F);
    enemy_local[0] += shift[0];
    enemy_local[2] += shift[2];
    float expect[3];
    eng_origin_to_local(&o, enemy_world, expect);
    EXPECT_NEAR(enemy_local[0], expect[0], 1e-6F);
    EXPECT_NEAR(enemy_local[1], expect[1], 1e-6F);
    EXPECT_NEAR(enemy_local[2], expect[2], 1e-6F);
    EXPECT_EQ(o.rebases, 1U);
    const double nan_p[3] = {NAN, 0.0, 0.0};
    EXPECT_FALSE(eng_origin_update(&o, nan_p, shift));
}

/* ── persistance ── */

static EngWorldState g_state;
static EngWorldState g_state2;

UTEST(world_state, a_cell_remembers_removed_objects_and_defeated_camps_after_unload)
{
    eng_wstate_init(&g_state);
    const EngCellCoord c = {12, -7};
    EXPECT_EQ(eng_wstate_get(&g_state, c), NULL); /* jamais modifiée : rien */
    EXPECT_TRUE(eng_wstate_remove_object(&g_state, c, 5));
    EXPECT_TRUE(eng_wstate_remove_object(&g_state, c, 63));
    EXPECT_FALSE(eng_wstate_remove_object(&g_state, c, 64));
    EXPECT_TRUE(eng_wstate_defeat_camp(&g_state, c));
    EXPECT_TRUE(eng_wstate_object_removed(&g_state, c, 5));
    EXPECT_TRUE(eng_wstate_object_removed(&g_state, c, 63));
    EXPECT_FALSE(eng_wstate_object_removed(&g_state, c, 6));
    EXPECT_EQ(eng_wstate_camps_defeated(&g_state, c), 1U);
    EXPECT_EQ(eng_wstate_camps_defeated(&g_state, (EngCellCoord){12, -6}), 0U);
    /* la cellule est déchargée puis régénérée : son contenu revient identique, son état aussi */
    const EngWorldGen gen = eng_world_gen_default(2);
    ASSERT_TRUE(eng_cell_generate(&gen, c, &g_a));
    uint32_t visible = 0;
    for (uint32_t i = 0; i < g_a.object_count; i++) {
        visible += eng_wstate_object_removed(&g_state, c, i) ? 0U : 1U;
    }
    EXPECT_EQ(visible, g_a.object_count - 1U); /* l'objet 5 reste retiré (63 n'existe pas ici) */
    /* sauvegarde / chargement */
    static char text[65536];
    const size_t n = eng_wstate_save(&g_state, text, sizeof text);
    EXPECT_GT(n, 10U);
    ASSERT_TRUE(eng_wstate_load(&g_state2, text));
    EXPECT_TRUE(eng_wstate_object_removed(&g_state2, c, 5));
    EXPECT_EQ(eng_wstate_camps_defeated(&g_state2, c), 1U);
    EXPECT_EQ(eng_wstate_get(&g_state2, c)->revision, eng_wstate_get(&g_state, c)->revision);
    EXPECT_FALSE(eng_wstate_load(&g_state2, "cell 1 2 zz 0 0\n"));
    EXPECT_EQ(g_state2.count, 0U); /* refusé : vide, pas à moitié chargé */
    EXPECT_FALSE(eng_wstate_load(&g_state2, "nope\n"));
    EXPECT_EQ(eng_wstate_save(&g_state, text, 4), 0U);
}

UTEST(world_state, thousands_of_cells_fit_and_collisions_are_resolved)
{
    eng_wstate_init(&g_state);
    for (int32_t i = 0; i < 3000; i++) {
        const EngCellCoord c = {i % 97 - 48, i / 97 - 15};
        ASSERT_TRUE(eng_wstate_remove_object(&g_state, c, (uint32_t)(i % 64)));
    }
    EXPECT_EQ(g_state.count, 3000U);
    for (int32_t i = 0; i < 3000; i++) {
        const EngCellCoord c = {i % 97 - 48, i / 97 - 15};
        EXPECT_TRUE(eng_wstate_object_removed(&g_state, c, (uint32_t)(i % 64)));
    }
}

/* Grande carte : avec les biomes, deux cellules voisines se raccordent encore exactement, chaque
 * sommet vaut la hauteur du générateur, le relief de la montagne est plus haut que celui de la
 * plaine, et il n'y a d'arbres qu'en plaine. Sans biomes, la hauteur du générateur est le bruit
 * seul (monde d'avant). */
UTEST(world_cell, biomes_keep_seams_exact_scale_the_relief_and_keep_trees_in_the_plain)
{
    EngWorldGen gen = eng_world_gen_default(7);
    gen.biomes = eng_biome_map_default();
    static EngCellData a;
    static EngCellData b;
    /* cellules (3, 0) et (4, 0) : x de 768 à 1280, à cheval sur la frontière plaine / montagne */
    ASSERT_TRUE(eng_cell_generate(&gen, (EngCellCoord){3, 0}, &a));
    ASSERT_TRUE(eng_cell_generate(&gen, (EngCellCoord){4, 0}, &b));
    for (uint32_t z = 0; z < ENG_CELL_RES; z++) {
        const float right = a.heights[((size_t)z * ENG_CELL_RES) + (ENG_CELL_RES - 1U)];
        const float left = b.heights[(size_t)z * ENG_CELL_RES];
        EXPECT_EQ(right, left); /* au bit près */
    }
    for (uint32_t i = 0; i < 5U; i++) { /* quelques sommets : la hauteur du générateur */
        const uint32_t x = 7U + (i * 11U);
        const uint32_t z = 3U + (i * 13U);
        const float h =
            eng_world_gen_height(&gen, (4.0 * 256.0) + ((double)x * 4.0), (double)z * 4.0);
        EXPECT_NEAR(b.heights[((size_t)z * ENG_CELL_RES) + x], h, 1e-3F);
    }
    static EngCellData plain;
    static EngCellData snow;
    static EngCellData desert;
    ASSERT_TRUE(eng_cell_generate(&gen, (EngCellCoord){1, 0}, &plain));  /* x 256 à 512 */
    ASSERT_TRUE(eng_cell_generate(&gen, (EngCellCoord){4, 2}, &snow));   /* x 1024 à 1280 */
    ASSERT_TRUE(eng_cell_generate(&gen, (EngCellCoord){7, 0}, &desert)); /* x 1792 à 2048 */
    uint32_t trees[3] = {0, 0, 0};
    const EngCellData *cells[3] = {&plain, &snow, &desert};
    for (int c = 0; c < 3; c++) {
        for (uint32_t i = 0; i < cells[c]->object_count; i++) {
            trees[c] += cells[c]->objects[i].kind == ENG_CELL_TREE ? 1U : 0U;
        }
    }
    EXPECT_EQ(trees[0], gen.trees_per_cell);
    EXPECT_EQ(trees[1], 0U);
    EXPECT_EQ(trees[2], 0U);
    /* même point de bruit : la montagne (x 1,8) est 4 fois plus haute que la plaine (x 0,45) */
    EngWorldGen flat = gen;
    flat.biomes.enabled = false;
    const float raw = eng_world_gen_height(&flat, 1200.0, 300.0);
    EXPECT_NEAR(eng_world_gen_height(&gen, 1200.0, 300.0), raw * 1.8F, 1e-3F);
    const float raw_plain = eng_world_gen_height(&flat, 400.0, 300.0);
    EXPECT_NEAR(eng_world_gen_height(&gen, 400.0, 300.0), raw_plain * 0.45F, 1e-3F);
    EXPECT_NEAR(eng_world_gen_height(NULL, 0.0, 0.0), 0.0F, 0.0F);
}
