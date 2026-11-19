/* Rejeu déterministe (M5-18). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/combat_replay.h"
#include "utest.h"

#define TICKS 600U

static const char SET[] =
    "attack light1\n startup 8\n active 4\n recovery 14\n damage 10\n posture 8\n stamina 10\n"
    " hitstop 3\n cancel_from 6\n next light2\n hitbox hand_r 0 0 0 0 0 0.6 0.12\nend\n"
    "attack light2\n startup 7\n active 4\n recovery 14\n damage 10\n posture 8\n stamina 10\n"
    " hitstop 3\n cancel_from 6\n hitbox hand_r 0 0 0 0 0 0.6 0.12\nend\n"
    "attack heavy\n startup 20\n active 5\n recovery 24\n damage 25\n posture 30\n stamina 25\n"
    " hitstop 6\n hitbox hand_r 0 0 0 0 0 0.7 0.14\nend\n";

static EngAttackSet g_set;
static EngPackedInput g_store[TICKS * 2U];
static EngReplaySetup g_setup;

static void init_common(void)
{
    char err[128];
    if (!eng_attackset_parse(SET, &g_set, err, sizeof err)) {
        (void)fprintf(stderr, "parse: %s\n", err);
        abort();
    }
    memset(&g_setup, 0, sizeof g_setup);
    g_setup.count = 2;
    g_setup.x[0] = 0.0F;
    g_setup.z[0] = 0.0F;
    g_setup.yaw[0] = 0.0F;
    g_setup.x[1] = 0.0F;
    g_setup.z[1] = -0.8F;
    g_setup.yaw[1] = 3.14159265F;
}

/* Joue en direct (entrées pseudo-aléatoires graines) en enregistrant ; renvoie l'empreinte. */
static uint64_t record_live(EngReplay *r, unsigned seed)
{
    EngCombatSim s;
    if (!eng_replay_setup_sim(&s, &g_set, &g_setup) || !eng_replay_init(r, g_store, TICKS, 2)) {
        abort();
    }
    for (uint32_t t = 0; t < TICKS; t++) {
        EngPackedInput p[2];
        EngFighterInput in[2];
        for (int k = 0; k < 2; k++) {
            seed = seed * 1664525U + 1013904223U;
            const unsigned v = (seed >> 16) % 16U;
            EngFighterInput raw;
            memset(&raw, 0, sizeof raw);
            raw.light = v <= 2U;
            raw.heavy = v == 3U;
            raw.dodge = v == 4U;
            raw.block_pressed = v == 5U;
            raw.block_held = v == 5U || v == 6U;
            raw.moving = v >= 7U;
            raw.move_dir[0] = cosf((float)seed * 0.001F);
            raw.move_dir[1] = sinf((float)seed * 0.001F);
            const float dx = s.bodies[1 - k].pos[0] - s.bodies[k].pos[0];
            const float dz = s.bodies[1 - k].pos[1] - s.bodies[k].pos[1];
            if (v >= 7U && sqrtf(dx * dx + dz * dz) > 0.7F) { /* se rapproche de l'adversaire */
                raw.move_dir[0] = dx;
                raw.move_dir[1] = dz;
            }
            p[k] = eng_input_pack(&raw);
            in[k] = eng_input_unpack(p[k]); /* le direct utilise l'entrée quantifiée */
        }
        if (!eng_replay_record(r, p)) {
            abort();
        }
        eng_sim_step(&s, in);
    }
    return eng_sim_hash(&s);
}

UTEST(combat_replay, replaying_the_recorded_inputs_gives_the_exact_final_hash)
{
    init_common();
    EngReplay r;
    const uint64_t live = record_live(&r, 777U);
    uint64_t again = 0;
    ASSERT_TRUE(eng_replay_run(&r, &g_set, &g_setup, &again, NULL, 0));
    EXPECT_TRUE(live == again);
    uint64_t twice = 0;
    ASSERT_TRUE(eng_replay_run(&r, &g_set, &g_setup, &twice, NULL, 0));
    EXPECT_TRUE(again == twice);
    /* la partie a vraiment contenu du combat (sinon le test ne prouverait rien) */
    EngCombatSim s;
    ASSERT_TRUE(eng_replay_setup_sim(&s, &g_set, &g_setup));
    for (uint32_t t = 0; t < r.ticks; t++) {
        const EngPackedInput *p = eng_replay_tick(&r, t);
        EngFighterInput in[2] = {eng_input_unpack(p[0]), eng_input_unpack(p[1])};
        eng_sim_step(&s, in);
    }
    EXPECT_GE(s.total_hits, 3U);
}

UTEST(combat_replay, different_seeds_give_different_hashes)
{
    init_common();
    EngReplay r;
    const uint64_t a = record_live(&r, 1U);
    const uint64_t b = record_live(&r, 2U);
    EXPECT_TRUE(a != b);
}

UTEST(combat_replay, the_first_divergence_is_found_at_or_after_the_changed_tick)
{
    init_common();
    EngReplay r;
    (void)record_live(&r, 4242U);
    static uint64_t ta[TICKS];
    static uint64_t tb[TICKS];
    uint64_t ha = 0;
    uint64_t hb = 0;
    ASSERT_TRUE(eng_replay_run(&r, &g_set, &g_setup, &ha, ta, TICKS));
    EXPECT_EQ(eng_replay_first_divergence(ta, ta, TICKS), TICKS);
    /* change l'entrée du combattant 0 au tick 300 : un déplacement à la place d'un appui */
    EngPackedInput *p = &r.data[(size_t)300U * 2U];
    p->flags = (uint8_t)(p->flags ^ ENG_PIN_LIGHT);
    p->dir = 1;
    p->flags |= ENG_PIN_MOVING;
    ASSERT_TRUE(eng_replay_run(&r, &g_set, &g_setup, &hb, tb, TICKS));
    const uint32_t d = eng_replay_first_divergence(ta, tb, TICKS);
    EXPECT_GE(d, 300U);
    EXPECT_LT(d, TICKS);
    EXPECT_EQ(eng_replay_first_divergence(NULL, tb, 5), 5U);
}

UTEST(combat_replay, quantisation_keeps_the_flags_and_the_direction_within_one_step)
{
    EngFighterInput in;
    memset(&in, 0, sizeof in);
    in.light = true;
    in.block_held = true;
    in.moving = true;
    float worst = 0.0F;
    for (int deg = 0; deg < 360; deg++) {
        const float a = (float)deg * 0.01745329F;
        in.move_dir[0] = cosf(a);
        in.move_dir[1] = sinf(a);
        const EngFighterInput out = eng_input_unpack(eng_input_pack(&in));
        EXPECT_TRUE(out.light && out.block_held && out.moving);
        EXPECT_FALSE(out.heavy || out.dodge || out.block_pressed);
        const float dot = out.move_dir[0] * in.move_dir[0] + out.move_dir[1] * in.move_dir[1];
        const float err = acosf(dot > 1.0F ? 1.0F : dot);
        if (err > worst) {
            worst = err;
        }
        EXPECT_NEAR(sqrtf(out.move_dir[0] * out.move_dir[0] + out.move_dir[1] * out.move_dir[1]),
                    1.0F, 1e-5F);
    }
    EXPECT_LT(worst, 0.0125F); /* ≤ π/254 ≈ 0,0124 rad */
    memset(&in, 0, sizeof in);
    EXPECT_EQ(eng_input_pack(&in).dir, 0U);
    in.move_dir[0] = NAN;
    EXPECT_EQ(eng_input_pack(&in).dir, 0U);
    in.move_dir[0] = 0.2F; /* sous le seuil */
    EXPECT_EQ(eng_input_pack(&in).dir, 0U);
    const EngFighterInput zero = eng_input_unpack((EngPackedInput){0, 0});
    EXPECT_NEAR(zero.move_dir[0], 0.0F, 1e-9F);
    EXPECT_EQ(eng_input_pack(NULL).flags, 0U);
}

UTEST(combat_replay, the_binary_format_round_trips_and_replays_to_the_same_hash)
{
    init_common();
    EngReplay r;
    const uint64_t live = record_live(&r, 99U);
    static uint8_t buf[200000];
    const size_t n = eng_replay_write(&r, &g_setup, live, buf, sizeof buf);
    ASSERT_EQ(n, eng_replay_size(&r));
    EXPECT_EQ(n, 24U + 24U + (size_t)TICKS * 4U);
    static EngPackedInput store2[TICKS * 2U];
    EngReplay r2;
    EngReplaySetup su2;
    uint64_t h2 = 0;
    ASSERT_TRUE(eng_replay_read(buf, n, store2, TICKS, &r2, &su2, &h2));
    EXPECT_TRUE(h2 == live);
    EXPECT_EQ(r2.ticks, TICKS);
    EXPECT_EQ(r2.fighters, 2U);
    EXPECT_TRUE(su2.z[1] == g_setup.z[1]);
    uint64_t replayed = 0;
    ASSERT_TRUE(eng_replay_run(&r2, &g_set, &su2, &replayed, NULL, 0));
    EXPECT_TRUE(replayed == live);
}

UTEST(combat_replay, malformed_files_are_refused)
{
    init_common();
    EngReplay r;
    const uint64_t live = record_live(&r, 5U);
    static uint8_t buf[200000];
    const size_t n = eng_replay_write(&r, &g_setup, live, buf, sizeof buf);
    ASSERT_GT(n, 0U);
    static EngPackedInput st[TICKS * 2U];
    EngReplay r2;
    EngReplaySetup su;
    uint64_t h;
    EXPECT_FALSE(eng_replay_write(&r, &g_setup, live, buf, n - 1U)); /* trop petit */
    for (size_t cut = 0; cut < n; cut += 37U) {                      /* toute troncature */
        EXPECT_FALSE(eng_replay_read(buf, cut, st, TICKS, &r2, &su, &h));
    }
    EXPECT_FALSE(eng_replay_read(buf, n - 1U, st, TICKS, &r2, &su, &h));
    buf[n] = 0;
    EXPECT_FALSE(eng_replay_read(buf, n + 1U, st, TICKS, &r2, &su, &h)); /* octet en trop */
    uint8_t save = buf[0];
    buf[0] = 'X';
    EXPECT_FALSE(eng_replay_read(buf, n, st, TICKS, &r2, &su, &h));
    buf[0] = save;
    save = buf[4];
    buf[4] = 2;
    EXPECT_FALSE(eng_replay_read(buf, n, st, TICKS, &r2, &su, &h)); /* version */
    buf[4] = save;
    save = buf[8];
    buf[8] = 0;
    EXPECT_FALSE(eng_replay_read(buf, n, st, TICKS, &r2, &su, &h)); /* 0 combattant */
    buf[8] = 9;
    EXPECT_FALSE(eng_replay_read(buf, n, st, TICKS, &r2, &su, &h)); /* trop de combattants */
    buf[8] = save;
    EXPECT_FALSE(eng_replay_read(buf, n, st, TICKS - 1U, &r2, &su, &h)); /* capacité */
    save = buf[24 + 24 + 0]; /* premier octet de drapeaux */
    buf[24 + 24] = 0x80;
    EXPECT_FALSE(eng_replay_read(buf, n, st, TICKS, &r2, &su, &h)); /* drapeau inconnu */
    buf[24 + 24] = save;
    save = buf[24 + 3]; /* octet de poids fort du premier flottant */
    buf[24 + 3] = 0x7F;
    buf[24 + 2] = 0xC0; /* NaN */
    EXPECT_FALSE(eng_replay_read(buf, n, st, TICKS, &r2, &su, &h));
    EXPECT_TRUE(eng_replay_read(buf, 0, st, TICKS, &r2, &su, &h) == false);
    EXPECT_FALSE(eng_replay_read(NULL, n, st, TICKS, &r2, &su, &h));
    (void)save;
}

UTEST(combat_replay, recording_limits_and_invalid_arguments)
{
    init_common();
    EngPackedInput st[3];
    EngReplay r;
    EXPECT_FALSE(eng_replay_init(NULL, st, 3, 1));
    EXPECT_FALSE(eng_replay_init(&r, NULL, 3, 1));
    EXPECT_FALSE(eng_replay_init(&r, st, 0, 1));
    EXPECT_FALSE(eng_replay_init(&r, st, 3, 0));
    EXPECT_FALSE(eng_replay_init(&r, st, 3, ENG_SIM_MAX + 1U));
    EXPECT_FALSE(eng_replay_init(&r, st, ENG_REPLAY_MAX_TICKS + 1U, 1));
    ASSERT_TRUE(eng_replay_init(&r, st, 3, 1));
    const EngPackedInput in = {ENG_PIN_LIGHT, 0};
    EXPECT_TRUE(eng_replay_record(&r, &in));
    EXPECT_TRUE(eng_replay_record(&r, &in));
    EXPECT_TRUE(eng_replay_record(&r, &in));
    EXPECT_FALSE(eng_replay_record(&r, &in)); /* plein */
    EXPECT_FALSE(eng_replay_record(&r, NULL));
    EXPECT_FALSE(eng_replay_record(NULL, &in));
    EXPECT_TRUE(eng_replay_tick(&r, 2) != NULL);
    EXPECT_TRUE(eng_replay_tick(&r, 3) == NULL);
    EXPECT_TRUE(eng_replay_tick(NULL, 0) == NULL);
    uint64_t h = 0;
    EXPECT_FALSE(eng_replay_run(&r, &g_set, &g_setup, &h, NULL, 0)); /* 1 combattant ≠ setup à 2 */
    EXPECT_FALSE(eng_replay_run(NULL, &g_set, &g_setup, &h, NULL, 0));
    EXPECT_FALSE(eng_replay_run(&r, &g_set, &g_setup, NULL, NULL, 0));
    EXPECT_EQ(eng_replay_size(NULL), 0U);
    EngReplaySetup bad = g_setup;
    bad.x[0] = NAN;
    EngCombatSim s;
    EXPECT_FALSE(eng_replay_setup_sim(&s, &g_set, &bad));
    EXPECT_FALSE(eng_replay_setup_sim(&s, &g_set, NULL));
}

UTEST(combat_replay, pack_threshold_and_init_limits_are_exact)
{
    EngFighterInput in;
    memset(&in, 0, sizeof in);
    in.move_dir[0] = 0.5F; /* norme exactement 0,5 : pas de direction */
    EXPECT_EQ(eng_input_pack(&in).dir, 0U);
    in.move_dir[0] = 0.51F;
    EXPECT_NE(eng_input_pack(&in).dir, 0U);
    in.move_dir[0] = -1.0F; /* π : tout en haut de l'échelle, sans dépasser */
    EXPECT_EQ(eng_input_pack(&in).dir, 255U);
    in.move_dir[0] = 1.0F; /* 0 rad : milieu */
    EXPECT_EQ(eng_input_pack(&in).dir, 128U);
    static EngPackedInput big[ENG_REPLAY_MAX_TICKS];
    EngReplay r;
    EXPECT_TRUE(eng_replay_init(&r, big, ENG_REPLAY_MAX_TICKS, 1));
    EXPECT_TRUE(eng_replay_init(&r, big, 4, ENG_SIM_MAX));
    EXPECT_TRUE(eng_replay_init(&r, big, 1, 1));
}
