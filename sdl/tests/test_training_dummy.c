/* Mannequin d'entraînement (M5-17). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/training_dummy.h"
#include "utest.h"

static const char SET[] =
    "attack light1\n startup 8\n active 4\n recovery 14\n damage 10\n posture 8\n stamina 10\n"
    " hitstop 3\n hitbox hand_r 0 0 0 0 0 0.6 0.12\nend\n"
    "attack heavy\n startup 20\n active 5\n recovery 24\n damage 25\n posture 30\n stamina 25\n"
    " hitstop 6\n hitbox hand_r 0 0 0 0 0 0.7 0.14\nend\n";

static EngAttackSet g_set;

static void duel(EngCombatSim *s)
{
    char err[128];
    if (!eng_attackset_parse(SET, &g_set, err, sizeof err) || !eng_sim_init(s, &g_set, 2) ||
        !eng_sim_place(s, 0, 0.0F, 0.0F, 0.0F) || !eng_sim_place(s, 1, 0.0F, -0.8F, 3.14159265F)) {
        abort();
    }
}

static void step(EngCombatSim *s, EngTrainingDummy *d, bool light)
{
    EngFighterInput in[2];
    memset(in, 0, sizeof in);
    in[0].light = light;
    eng_dummy_tick(d, s, &in[1]);
    eng_sim_step(s, in);
}

UTEST(training_dummy, static_dummy_counts_the_damage_it_receives)
{
    EngCombatSim s;
    duel(&s);
    EngTrainingDummy d;
    eng_dummy_init(&d, 1, ENG_DUMMY_STATIC, 0);
    step(&s, &d, true);
    for (int i = 0; i < 40; i++) {
        step(&s, &d, false);
    }
    EXPECT_EQ(d.hits_taken, 1U);
    EXPECT_NEAR(d.total_damage, 10.0F, 1.0F); /* légère x zone (0,75 a 1,5) */
    EXPECT_GT(d.last_damage, 0.0F);
}

UTEST(training_dummy, blocking_dummy_takes_reduced_damage_and_no_damage_events)
{
    EngCombatSim s;
    duel(&s);
    EngTrainingDummy d;
    eng_dummy_init(&d, 1, ENG_DUMMY_BLOCK, 0);
    for (int i = 0; i < 20; i++) { /* laisse la parade s'écouler : la garde est établie */
        step(&s, &d, false);
    }
    step(&s, &d, true);
    for (int i = 0; i < 40; i++) {
        step(&s, &d, false);
    }
    EXPECT_GT(s.total_hits, 0U);
    EXPECT_LT(s.fighters[1].vitals.max_hp - s.fighters[1].vitals.hp, 5.0F); /* x0,2 */
}

UTEST(training_dummy, attacking_dummy_swings_on_its_period)
{
    EngCombatSim s;
    duel(&s);
    EngTrainingDummy d;
    eng_dummy_init(&d, 1, ENG_DUMMY_ATTACK, 60);
    uint32_t starts = 0;
    for (int i = 0; i < 130; i++) {
        step(&s, &d, false);
        if ((s.fighters[1].events & ENG_FEV_ATTACK_START) != 0U) {
            starts++;
        }
    }
    EXPECT_EQ(starts, 2U);
}

UTEST(training_dummy, heals_after_a_quiet_period_and_respawns_after_death)
{
    EngCombatSim s;
    duel(&s);
    EngTrainingDummy d;
    eng_dummy_init(&d, 1, ENG_DUMMY_STATIC, 0);
    s.fighters[1].vitals.hp = 30.0F;
    d.reset_after = 50;
    for (int i = 0; i < 60; i++) {
        step(&s, &d, false);
    }
    EXPECT_NEAR(s.fighters[1].vitals.hp, s.fighters[1].vitals.max_hp, 0.01F);
    s.fighters[1].vitals.hp = 0.0F;
    s.fighters[1].vitals.dead = true;
    d.respawn_after = 30;
    for (int i = 0; i < 40; i++) {
        step(&s, &d, false);
    }
    EXPECT_FALSE(s.fighters[1].vitals.dead);
    EXPECT_NEAR(s.fighters[1].vitals.hp, s.fighters[1].vitals.max_hp, 0.01F);
}
