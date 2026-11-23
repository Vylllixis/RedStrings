/* Monde ouvert : uploads étalés (M7-04), IA et navigation d'une cellule à l'autre (M7-11). */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/ai_area.h"
#include "engine/ai_group.h"
#include "engine/upload_queue.h"
#include "utest.h"

UTEST(upload_queue, each_frame_respects_the_budget_and_serves_the_nearest_first)
{
    EngUploadQueue q;
    eng_upq_init(&q, 100000U);
    for (uint32_t i = 0; i < 20U; i++) {
        ASSERT_TRUE(eng_upq_push(&q, i, 40000U, (float)(20U - i))); /* id 19 = le plus proche */
    }
    EXPECT_FALSE(eng_upq_push(&q, 3, 10U, 1.0F)); /* doublon */
    uint64_t ids[16];
    uint32_t frames = 0;
    uint64_t first = UINT64_MAX;
    while (q.count > 0U) {
        const uint32_t n = eng_upq_take(&q, ids, 16);
        ASSERT_GT(n, 0U);
        if (frames == 0U) {
            first = ids[0];
            EXPECT_EQ(n, 2U); /* 2 x 40 000 <= 100 000 < 3 x 40 000 */
        }
        frames++;
    }
    EXPECT_EQ(first, 19U);
    EXPECT_EQ(frames, 10U);
    EXPECT_LE(q.max_frame_bytes, 100000U);
    EXPECT_EQ(q.sent, 20U);
    EXPECT_EQ(q.oldest_wait, 9U);
}

UTEST(upload_queue, an_oversized_request_passes_alone_and_cancel_and_priority_updates_work)
{
    EngUploadQueue q;
    eng_upq_init(&q, 1000U);
    ASSERT_TRUE(eng_upq_push(&q, 1, 5000U, 1.0F));
    ASSERT_TRUE(eng_upq_push(&q, 2, 100U, 2.0F));
    ASSERT_TRUE(eng_upq_push(&q, 3, 100U, 3.0F));
    uint64_t ids[8];
    EXPECT_EQ(eng_upq_take(&q, ids, 8), 1U);
    EXPECT_EQ(ids[0], 1U);
    EXPECT_TRUE(eng_upq_set_distance(&q, 3, 0.5F));
    EXPECT_TRUE(eng_upq_cancel(&q, 2));
    EXPECT_FALSE(eng_upq_cancel(&q, 2));
    EXPECT_EQ(eng_upq_take(&q, ids, 8), 1U);
    EXPECT_EQ(ids[0], 3U);
    EXPECT_EQ(eng_upq_take(&q, ids, 8), 0U);
    EXPECT_FALSE(eng_upq_push(&q, 9, 0U, 1.0F));
    EXPECT_FALSE(eng_upq_push(&q, 9, 10U, NAN));
    EXPECT_FALSE(eng_upq_set_distance(&q, 77, 1.0F));
    EXPECT_EQ(eng_upq_take(NULL, ids, 8), 0U);
}

/* ── un ennemi suit le joueur d'une cellule à l'autre ── */

static const char ATTACKS[] =
    "attack light1\n startup 8\n active 4\n recovery 14\n damage 10\n posture 8\n stamina 10\n"
    " hitstop 3\n hitbox hand_r 0 0 0.3 0 0 1.4 0.15\nend\n"
    "attack heavy\n startup 20\n active 5\n recovery 24\n damage 25\n posture 30\n stamina 25\n"
    " hitstop 6\n hitbox hand_r 0 0 0.3 0 0 1.6 0.2\nend\n"
    "attack g_slash\n startup 18\n active 4\n recovery 18\n damage 6\n posture 8\n stamina 5\n"
    " hitstop 3\n hitbox hand_r 0 0 0.3 0 0 1.5 0.15\nend\n"
    "attack g_lunge\n startup 24\n active 6\n recovery 30\n damage 10\n posture 14\n stamina 5\n"
    " hitstop 4\n hitbox hand_r 0 0 0.3 0 0 2.0 0.15\nend\n";
static const char ARCH[] =
    "archetype grunt\n hp 50\n speed 0.9\n sight 30\n fov 360\n hearing 30\n"
    " memory 600\n light g_slash\n heavy g_lunge\n plan light 0 1.9 50 1 1\nend\n";

static EngJobs g_jobs;
static EngWorldStream g_stream;
static EngAiArea g_area;
static EngAttackSet g_atk;
static EngArchSet g_arch;
static EngCombatSim g_sim;
static EngAiGroup g_group;

UTEST(world_ai, an_enemy_follows_the_player_across_a_cell_border_and_an_origin_rebase)
{
    char err[160];
    ASSERT_TRUE(eng_attackset_parse(ATTACKS, &g_atk, err, sizeof err));
    ASSERT_TRUE(eng_arch_parse(ARCH, &g_arch, err, sizeof err));
    ASSERT_TRUE(eng_jobs_init(&g_jobs, 2));
    EngWorldGen gen = eng_world_gen_default(21);
    gen.camp_every = 0;
    ASSERT_TRUE(eng_stream_init(&g_stream, &gen, &g_jobs, 1, NULL));
    EngWorldOrigin origin;
    eng_origin_init(&origin, 160.0);
    /* le joueur part de (200, 100) monde et marche vers +X à 3 m/s, au-delà de x = 256 puis 400 */
    double pw[3] = {200.0, 0.0, 100.0};
    for (int i = 0; i < 3000 && !eng_stream_ready_around(&g_stream, pw[0], pw[2], 1); i++) {
        (void)eng_stream_update(&g_stream, pw[0], pw[2]);
        SDL_Delay(1);
    }
    ASSERT_TRUE(eng_stream_ready_around(&g_stream, pw[0], pw[2], 1));
    eng_ai_area_init(&g_area, 32.0F, 0.5F);
    ASSERT_TRUE(eng_sim_init(&g_sim, &g_atk, 2));
    g_sim.world = &g_area.world;
    g_sim.world_ignore = 1U; /* le joueur scripté traverse tout */
    g_sim.fighters[0].vitals.max_hp = 1e6F;
    g_sim.fighters[0].vitals.hp = 1e6F;
    float pl[3];
    eng_origin_to_local(&origin, pw, pl);
    const float focus[2] = {pl[0], pl[2]};
    ASSERT_TRUE(eng_ai_area_update(&g_area, &g_stream, &origin, focus));
    eng_aig_init(&g_group, &g_arch, &g_area.world, &g_area.nav, 0);
    /* l'ennemi part 10 m derrière le joueur, sur un point praticable */
    float ex = pl[0] - 10.0F;
    float ez = pl[2];
    for (int k = 0; k < 40 && !eng_nav_walkable(&g_area.nav, (const float[2]){ex, ez}); k++) {
        ez += 0.5F;
    }
    ASSERT_TRUE(eng_aig_add(&g_group, &g_sim, 1, 0, ex, ez, -1.5707963F, NULL, 0));
    uint32_t rebuilds_seen = 0;
    uint32_t inside = 0;
    float worst_gap = 0.0F;
    for (uint32_t t = 0; t < 60U * 75U; t++) { /* 75 s : 225 m, 2 frontières de cellule */
        pw[0] += 3.0 / 60.0;
        (void)eng_stream_update(&g_stream, pw[0], pw[2]);
        float shift[3];
        if (eng_origin_update(&origin, pw, shift)) {
            for (uint32_t i = 0; i < g_sim.count; i++) { /* tout le monde se décale */
                g_sim.bodies[i].pos[0] += shift[0];
                g_sim.bodies[i].pos[1] += shift[2];
                g_sim.bodies[i].prev_pos[0] += shift[0];
                g_sim.bodies[i].prev_pos[1] += shift[2];
            }
            eng_ai_area_shift(&g_area, shift);
            g_group.agents[0].home[0] += shift[0];
            g_group.agents[0].home[1] += shift[2];
            g_group.agents[0].perc.last_known[0] += shift[0];
            g_group.agents[0].perc.last_known[1] += shift[2];
            eng_aig_invalidate_paths(&g_group);
        }
        eng_origin_to_local(&origin, pw, pl);
        (void)eng_sim_place(&g_sim, 0, pl[0], pl[2], -1.5707963F);
        const float f2[2] = {pl[0], pl[2]};
        if (eng_ai_area_update(&g_area, &g_stream, &origin, f2)) {
            eng_aig_invalidate_paths(&g_group);
            rebuilds_seen++;
        }
        EngFighterInput in[2];
        memset(in, 0, sizeof in);
        eng_aig_tick(&g_group, &g_sim, 6.0F, in);
        eng_sim_step(&g_sim, in);
        if (eng_aiw_distance(&g_area.world, g_sim.bodies[1].pos) < 0.4F) {
            inside++;
        }
        if (t > 600U) {
            const float gap =
                hypotf(g_sim.bodies[1].pos[0] - pl[0], g_sim.bodies[1].pos[1] - pl[2]);
            worst_gap = gap > worst_gap ? gap : worst_gap;
        }
    }
    double ew[3];
    const float el[3] = {g_sim.bodies[1].pos[0], 0.0F, g_sim.bodies[1].pos[1]};
    eng_origin_to_world(&origin, el, ew);
    (void)fprintf(
        stderr, "world_ai: player x=%.1f enemy x=%.1f gap<=%.1f rebuilds=%u rebases=%u inside=%u\n",
        pw[0], ew[0], (double)worst_gap, rebuilds_seen, origin.rebases, inside);
    EXPECT_GT(pw[0], 400.0);
    EXPECT_GT(ew[0], 400.0 - 8.0); /* l'ennemi a passé x = 256 avec lui */
    EXPECT_LT(worst_gap, 8.0F);    /* il ne décroche jamais */
    EXPECT_GE(rebuilds_seen, 3U);
    EXPECT_GE(origin.rebases, 1U);
    EXPECT_EQ(inside, 0U);
    eng_stream_shutdown(&g_stream);
    eng_jobs_shutdown(&g_jobs);
}
