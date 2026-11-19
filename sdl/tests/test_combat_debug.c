/* Débogueur de combat (M5-16). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/combat_debug.h"
#include "utest.h"

#define TICKS 300U

static const char SET[] =
    "attack light1\n startup 8\n active 4\n recovery 14\n damage 10\n posture 8\n stamina 10\n"
    " hitstop 3\n cancel_from 6\n hitbox hand_r 0 0 0 0 0 0.6 0.12\nend\n";

static EngAttackSet g_set;
static EngPackedInput g_store[TICKS * 2U];
static EngReplay g_replay;
static EngReplaySetup g_setup;

static void build(void)
{
    char err[128];
    if (!eng_attackset_parse(SET, &g_set, err, sizeof err)) {
        abort();
    }
    memset(&g_setup, 0, sizeof g_setup);
    g_setup.count = 2;
    g_setup.z[1] = -0.8F;
    g_setup.yaw[1] = 3.14159265F;
    if (!eng_replay_init(&g_replay, g_store, TICKS, 2)) {
        abort();
    }
    for (uint32_t t = 0; t < TICKS; t++) {
        EngPackedInput p[2] = {{0, 0}, {0, 0}};
        if (t == 0U) {
            p[0].flags = ENG_PIN_LIGHT;
        }
        if (!eng_replay_record(&g_replay, p)) {
            abort();
        }
    }
}

UTEST(combat_debug, the_timeline_shows_startup_active_freeze_and_the_hit_marks)
{
    build();
    EngCombatDebugger d;
    ASSERT_TRUE(eng_cdbg_init(&d, &g_replay, &g_set, &g_setup));
    EXPECT_EQ(eng_cdbg_length(&d), TICKS);
    ASSERT_TRUE(eng_cdbg_seek(&d, 13));
    char buf[64];
    EXPECT_EQ(eng_cdbg_timeline(&d, 0, 13, buf, sizeof buf), 13U);
    EXPECT_STREQ(buf, "ssssssssA###A");
    EXPECT_EQ(eng_cdbg_timeline(&d, 1, 13, buf, sizeof buf), 13U);
    EXPECT_STREQ(buf, "........h###h");
    EXPECT_EQ(eng_cdbg_hitline(&d, 1, 13, buf, sizeof buf), 13U);
    EXPECT_STREQ(buf, "        *    ");
    EXPECT_EQ(eng_cdbg_hitline(&d, 0, 13, buf, sizeof buf), 13U);
    EXPECT_STREQ(buf, "        >    ");
    EXPECT_EQ(eng_cdbg_timeline(&d, 0, 4, buf, sizeof buf), 4U); /* les 4 derniers */
    EXPECT_STREQ(buf, "###A");
}

UTEST(combat_debug, stepping_back_and_forth_lands_on_the_same_state)
{
    build();
    EngCombatDebugger d;
    ASSERT_TRUE(eng_cdbg_init(&d, &g_replay, &g_set, &g_setup));
    EXPECT_FALSE(eng_cdbg_step_back(&d)); /* déjà au tick 0 */
    ASSERT_TRUE(eng_cdbg_seek(&d, 20));
    const uint64_t at20 = eng_sim_hash(&d.sim);
    ASSERT_TRUE(eng_cdbg_seek(&d, 9));
    EXPECT_EQ(eng_cdbg_tick(&d), 9U);
    EXPECT_EQ(d.sim.total_hits, 1U);
    ASSERT_TRUE(eng_cdbg_step_back(&d));
    EXPECT_EQ(eng_cdbg_tick(&d), 8U);
    EXPECT_EQ(d.sim.total_hits, 0U); /* la touche est « dé-faite » */
    EXPECT_NEAR(d.sim.fighters[1].vitals.hp, 100.0F, 1e-6F);
    char buf[16];
    EXPECT_EQ(eng_cdbg_timeline(&d, 0, 8, buf, sizeof buf), 8U);
    EXPECT_STREQ(buf, "ssssssss");
    ASSERT_TRUE(eng_cdbg_seek(&d, 20));
    EXPECT_TRUE(eng_sim_hash(&d.sim) == at20);
    /* sauter directement en arrière puis avancer pas à pas */
    ASSERT_TRUE(eng_cdbg_seek(&d, 0));
    EXPECT_EQ(eng_cdbg_tick(&d), 0U);
    for (int i = 0; i < 20; i++) {
        ASSERT_TRUE(eng_cdbg_step(&d));
    }
    EXPECT_TRUE(eng_sim_hash(&d.sim) == at20);
}

UTEST(combat_debug, the_end_of_the_replay_and_invalid_seeks_are_refused)
{
    build();
    EngCombatDebugger d;
    ASSERT_TRUE(eng_cdbg_init(&d, &g_replay, &g_set, &g_setup));
    EXPECT_FALSE(eng_cdbg_seek(&d, TICKS + 1U));
    ASSERT_TRUE(eng_cdbg_seek(&d, TICKS));
    EXPECT_FALSE(eng_cdbg_step(&d));
    EXPECT_EQ(eng_cdbg_tick(&d), TICKS);
    /* l'historique ne garde que ENG_CDBG_HISTORY ticks */
    char buf[400];
    EXPECT_EQ(eng_cdbg_timeline(&d, 0, 400, buf, sizeof buf), ENG_CDBG_HISTORY);
    EXPECT_EQ(buf[ENG_CDBG_HISTORY - 1U], '.');
    EXPECT_EQ(eng_cdbg_timeline(&d, 0, 400, buf, 10), 9U); /* tronquée à la capacité */
    EXPECT_EQ(strlen(buf), 9U);
    EXPECT_EQ(eng_cdbg_timeline(&d, 5, 10, buf, sizeof buf), 0U); /* combattant inconnu */
    EXPECT_EQ(eng_cdbg_timeline(&d, 0, 10, NULL, 10), 0U);
    EXPECT_EQ(eng_cdbg_timeline(&d, 0, 10, buf, 0), 0U);
    EXPECT_EQ(eng_cdbg_timeline(NULL, 0, 10, buf, sizeof buf), 0U);
    EXPECT_EQ(eng_cdbg_hitline(NULL, 0, 10, buf, sizeof buf), 0U);
    EXPECT_FALSE(eng_cdbg_step(NULL));
    EXPECT_FALSE(eng_cdbg_step_back(NULL));
    EXPECT_FALSE(eng_cdbg_seek(NULL, 0));
    EXPECT_EQ(eng_cdbg_tick(NULL), 0U);
    EXPECT_EQ(eng_cdbg_length(NULL), 0U);
    EngReplaySetup one = g_setup;
    one.count = 1;
    EXPECT_FALSE(eng_cdbg_init(&d, &g_replay, &g_set, &one)); /* combattants ≠ replay */
    EXPECT_FALSE(eng_cdbg_init(NULL, &g_replay, &g_set, &g_setup));
    EXPECT_FALSE(eng_cdbg_init(&d, NULL, &g_set, &g_setup));
    EXPECT_FALSE(eng_cdbg_init(&d, &g_replay, NULL, &g_setup));
    EXPECT_FALSE(eng_cdbg_init(&d, &g_replay, &g_set, NULL));
}

UTEST(combat_debug, state_letters_cover_every_state)
{
    build();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), '.');
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, true), '#');
    EXPECT_EQ(eng_cdbg_state_char(NULL, &g_set, false), '?');
    (void)eng_fsm_request(&f.fsm, ENG_CS_MOVE);
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'm');
    (void)eng_fsm_request(&f.fsm, ENG_CS_IDLE);
    (void)eng_fsm_request(&f.fsm, ENG_CS_PARRY);
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'P');
    (void)eng_fsm_request(&f.fsm, ENG_CS_BLOCK);
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'b');
    (void)eng_fsm_request(&f.fsm, ENG_CS_IDLE);
    (void)eng_fsm_request(&f.fsm, ENG_CS_DODGE);
    f.state_tick = 2;
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'd');
    f.state_tick = 3;
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'D');
    f.state_tick = 14;
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'D');
    f.state_tick = 15;
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'd');
    (void)eng_fsm_request(&f.fsm, ENG_CS_IDLE);
    (void)eng_fsm_request(&f.fsm, ENG_CS_ATTACK);
    f.attack = 0;
    f.attack_tick = 7;
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 's');
    f.attack_tick = 8;
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'A');
    f.attack_tick = 11;
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'A');
    f.attack_tick = 12;
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'r');
    f.attack = 9; /* indice invalide */
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), '?');
    (void)eng_fsm_request(&f.fsm, ENG_CS_HITSTUN);
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'h');
    (void)eng_fsm_request(&f.fsm, ENG_CS_STAGGER);
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'S');
    (void)eng_fsm_request(&f.fsm, ENG_CS_DEAD);
    EXPECT_EQ(eng_cdbg_state_char(&f, &g_set, false), 'X');
}

UTEST(combat_debug, the_drawing_follows_the_toggles_and_the_active_weapon)
{
    build();
    EngCombatDebugger d;
    ASSERT_TRUE(eng_cdbg_init(&d, &g_replay, &g_set, &g_setup));
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "cdbg", (size_t)1 << 20));
    EngDebugLines lines;
    ASSERT_TRUE(eng_debug_lines_init(&lines, &arena, 4000));
    ASSERT_TRUE(eng_cdbg_seek(&d, 1));
    EXPECT_EQ(eng_cdbg_draw(&d, &lines), 6U); /* 2 × 3 hurtboxes, arme pas encore active */
    EXPECT_EQ(eng_debug_lines_count(&lines), 6U * 76U);
    eng_debug_lines_clear(&lines);
    ASSERT_TRUE(eng_cdbg_seek(&d, 9));
    EXPECT_EQ(eng_cdbg_draw(&d, &lines), 7U);                 /* + l'arme active */
    EXPECT_EQ(eng_debug_lines_count(&lines), 7U * 76U + 36U); /* + 1 point de contact */
    eng_debug_lines_clear(&lines);
    d.show_contacts = false;
    EXPECT_EQ(eng_cdbg_draw(&d, &lines), 7U);
    EXPECT_EQ(eng_debug_lines_count(&lines), 7U * 76U);
    eng_debug_lines_clear(&lines);
    d.show_hurtboxes = false;
    EXPECT_EQ(eng_cdbg_draw(&d, &lines), 1U); /* l'arme seule */
    eng_debug_lines_clear(&lines);
    d.show_weapons = false;
    EXPECT_EQ(eng_cdbg_draw(&d, &lines), 0U);
    EXPECT_EQ(eng_debug_lines_count(&lines), 0U);
    EXPECT_EQ(eng_cdbg_draw(NULL, &lines), 0U);
    EXPECT_EQ(eng_cdbg_draw(&d, NULL), 0U);
    eng_arena_shutdown(&arena);
}
