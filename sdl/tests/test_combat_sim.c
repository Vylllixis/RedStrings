/* Simulation de combat déterministe (M5-17). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/combat_sim.h"
#include "utest.h"

static const char SET[] =
    "attack light1\n startup 8\n active 4\n recovery 14\n damage 10\n posture 8\n stamina 10\n"
    " hitstop 3\n cancel_from 6\n next light2\n hitbox hand_r 0 0 0 0 0 0.6 0.12\nend\n"
    "attack light2\n startup 7\n active 4\n recovery 14\n damage 10\n posture 8\n stamina 10\n"
    " hitstop 3\n cancel_from 6\n hitbox hand_r 0 0 0 0 0 0.6 0.12\nend\n"
    "attack heavy\n startup 20\n active 5\n recovery 24\n damage 25\n posture 30\n stamina 25\n"
    " hitstop 6\n hitbox hand_r 0 0 0 0 0 0.7 0.14\nend\n";

static EngAttackSet g_set;

static void load(void)
{
    char err[128];
    if (!eng_attackset_parse(SET, &g_set, err, sizeof err)) {
        (void)fprintf(stderr, "parse: %s\n", err);
        abort();
    }
}

static EngFighterInput none(void)
{
    EngFighterInput in;
    memset(&in, 0, sizeof in);
    return in;
}

/* A (0) en (0,0) regarde -Z ; B (1) en (0,-0.8) regarde +Z. */
static void duel(EngCombatSim *s, float dist)
{
    load();
    if (!eng_sim_init(s, &g_set, 2) || !eng_sim_place(s, 0, 0.0F, 0.0F, 0.0F) ||
        !eng_sim_place(s, 1, 0.0F, -dist, 3.14159265F)) {
        abort();
    }
}

static void run(EngCombatSim *s, uint32_t n)
{
    const EngFighterInput in[2] = {none(), none()};
    for (uint32_t i = 0; i < n; i++) {
        eng_sim_step(s, in);
    }
}

UTEST(combat_sim, a_light_attack_hits_once_at_the_first_active_tick)
{
    EngCombatSim s;
    duel(&s, 0.8F);
    EngFighterInput in[2] = {none(), none()};
    in[0].light = true;
    eng_sim_step(&s, in); /* tick 1 : départ */
    in[0] = none();
    uint32_t hit_tick = 0;
    uint32_t events = 0;
    for (int i = 0; i < 40; i++) {
        eng_sim_step(&s, in);
        for (uint32_t k = 0; k < s.event_count; k++) {
            hit_tick = s.events[k].tick;
            events++;
            EXPECT_EQ(s.events[k].attacker, 0U);
            EXPECT_EQ(s.events[k].defender, 1U);
            EXPECT_EQ(s.events[k].kind, ENG_HITKIND_DAMAGE);
            EXPECT_EQ(s.events[k].zone, ENG_ZONE_TORSO);
            EXPECT_NEAR(s.events[k].dealt, 10.0F, 1e-4F);
            EXPECT_EQ(s.events[k].hitstop, 3U);
        }
    }
    EXPECT_EQ(events, 1U); /* 4 ticks actifs, une seule touche */
    EXPECT_EQ(hit_tick, 9U);
    EXPECT_NEAR(s.fighters[1].vitals.hp, 90.0F, 1e-4F);
    EXPECT_LT(s.bodies[1].pos[1], -0.8F); /* repoussé, en s'éloignant de A */
    EXPECT_EQ(s.total_hits, 1U);
}

UTEST(combat_sim, nothing_is_hit_out_of_range_or_when_facing_away)
{
    EngCombatSim s;
    duel(&s, 3.0F);
    EngFighterInput in[2] = {none(), none()};
    in[0].light = true;
    eng_sim_step(&s, in);
    in[0] = none();
    run(&s, 40);
    EXPECT_EQ(s.total_hits, 0U);

    duel(&s, 0.8F);
    ASSERT_TRUE(eng_sim_place(&s, 0, 0.0F, 0.0F, 3.14159265F)); /* A tourné vers +Z */
    in[0].light = true;
    eng_sim_step(&s, in);
    in[0] = none();
    run(&s, 40);
    EXPECT_EQ(s.total_hits, 0U);
}

UTEST(combat_sim, a_dodge_started_early_enough_makes_the_target_untouchable)
{
    EngCombatSim s;
    duel(&s, 0.8F);
    EngFighterInput in[2] = {none(), none()};
    in[0].light = true;
    eng_sim_step(&s, in); /* tick 1 */
    in[0] = none();
    for (int i = 0; i < 3; i++) {
        eng_sim_step(&s, in); /* ticks 2 à 4 */
    }
    in[1].dodge = true;
    in[1].move_dir[0] = 0.0F;
    in[1].move_dir[1] = 1.0F; /* vers A : reste dans la zone */
    eng_sim_step(&s, in);     /* tick 5 : début de l'esquive, invulnérable des ticks 8 à 19 */
    in[1] = none();
    run(&s, 40);
    EXPECT_EQ(s.total_hits, 0U);
    EXPECT_NEAR(s.fighters[1].vitals.hp, 100.0F, 1e-6F);
}

UTEST(combat_sim, a_timely_parry_staggers_the_attacker_and_costs_nothing)
{
    EngCombatSim s;
    duel(&s, 0.8F);
    EngFighterInput in[2] = {none(), none()};
    in[0].light = true;
    for (int i = 0; i < 4; i++) {
        eng_sim_step(&s, in);
        in[0] = none();
    }
    in[1].block_pressed = true;
    in[1].block_held = true;
    eng_sim_step(&s, in); /* tick 5 : fenêtre de parade ticks 5 à 12 */
    in[1].block_pressed = false;
    uint32_t parried = 0;
    for (int i = 0; i < 20; i++) {
        eng_sim_step(&s, in);
        for (uint32_t k = 0; k < s.event_count; k++) {
            EXPECT_EQ(s.events[k].kind, ENG_HITKIND_PARRIED);
            parried++;
        }
    }
    EXPECT_EQ(parried, 1U);
    EXPECT_EQ(s.fighters[0].fsm.state, ENG_CS_STAGGER);
    EXPECT_NEAR(s.fighters[1].vitals.hp, 100.0F, 1e-6F);
}

UTEST(combat_sim, impact_freeze_pauses_fighters_but_keeps_the_inputs)
{
    EngCombatSim s;
    duel(&s, 0.8F);
    EngFighterInput in[2] = {none(), none()};
    in[0].light = true;
    eng_sim_step(&s, in);
    in[0] = none();
    for (int i = 0; i < 8; i++) {
        eng_sim_step(&s, in); /* tick 9 : touche */
    }
    ASSERT_EQ(s.total_hits, 1U);
    EXPECT_EQ(s.hitstop, 3U);
    const uint32_t frozen_attack_tick = s.fighters[0].attack_tick;
    const uint32_t frozen_ticks = s.fighters[0].tick;
    in[0].light = true; /* appui pendant le gel */
    eng_sim_step(&s, in);
    in[0] = none();
    eng_sim_step(&s, in);
    eng_sim_step(&s, in);
    EXPECT_EQ(s.hitstop, 0U);
    EXPECT_EQ(s.fighters[0].attack_tick, frozen_attack_tick);
    EXPECT_EQ(s.fighters[0].tick, frozen_ticks);
    EXPECT_EQ(s.fighters[0].buffer.count, 1U); /* l'appui n'est pas perdu */
    eng_sim_step(&s, in);
    EXPECT_EQ(s.fighters[0].attack_tick, frozen_attack_tick + 1U);
    EXPECT_EQ(s.tick, 13U); /* le tick de la simulation, lui, avance toujours */
}

UTEST(combat_sim, two_fighters_can_trade_blows_on_the_same_tick)
{
    EngCombatSim s;
    duel(&s, 0.8F);
    EngFighterInput in[2] = {none(), none()};
    in[0].light = true;
    in[1].light = true;
    eng_sim_step(&s, in);
    in[0] = none();
    in[1] = none();
    run(&s, 10);
    EXPECT_EQ(s.total_hits, 2U);
    EXPECT_NEAR(s.fighters[0].vitals.hp, 90.0F, 1e-4F);
    EXPECT_NEAR(s.fighters[1].vitals.hp, 90.0F, 1e-4F);
}

UTEST(combat_sim, walking_moves_at_four_metres_per_second_and_turns_the_body)
{
    EngCombatSim s;
    duel(&s, 10.0F);
    EngFighterInput in[2] = {none(), none()};
    in[0].moving = true;
    in[0].move_dir[0] = 1.0F; /* vers +X */
    for (int i = 0; i < 60; i++) {
        eng_sim_step(&s, in);
    }
    EXPECT_NEAR(s.bodies[0].pos[0], 4.0F, 1e-3F);
    EXPECT_NEAR(s.bodies[0].pos[1], 0.0F, 1e-4F);
    EXPECT_NEAR(s.bodies[0].yaw, -1.57079633F, 1e-4F); /* avant = (-sin yaw, -cos yaw) = +X */
    EXPECT_EQ(s.fighters[0].fsm.state, ENG_CS_MOVE);
}

UTEST(combat_sim, a_fatal_hit_kills_and_the_dead_ignore_further_inputs)
{
    EngCombatSim s;
    duel(&s, 0.8F);
    s.fighters[1].vitals.hp = 10.0F;
    EngFighterInput in[2] = {none(), none()};
    in[0].light = true;
    eng_sim_step(&s, in);
    in[0] = none();
    run(&s, 10);
    EXPECT_EQ(s.fighters[1].fsm.state, ENG_CS_DEAD);
    EXPECT_EQ(s.fighters[1].vitals.died_tick, 9U);
    in[1].light = true;
    in[1].moving = true;
    in[1].move_dir[0] = 1.0F;
    const float x = s.bodies[1].pos[0];
    run(&s, 1);
    eng_sim_step(&s, in);
    EXPECT_EQ(s.fighters[1].fsm.state, ENG_CS_DEAD);
    EXPECT_NEAR(s.bodies[1].pos[0], x, 1e-6F);
}

UTEST(combat_sim, the_same_inputs_give_the_same_hash_and_a_different_input_changes_it)
{
    EngCombatSim a;
    EngCombatSim b;
    EngCombatSim c;
    duel(&a, 0.8F);
    duel(&b, 0.8F);
    duel(&c, 0.8F);
    unsigned seed = 12345U;
    for (int t = 0; t < 400; t++) {
        EngFighterInput in[2] = {none(), none()};
        for (int k = 0; k < 2; k++) {
            seed = seed * 1664525U + 1013904223U;
            const unsigned r = (seed >> 16) % 16U;
            in[k].light = r == 0U;
            in[k].heavy = r == 1U;
            in[k].dodge = r == 2U;
            in[k].block_pressed = r == 3U;
            in[k].block_held = r >= 3U && r <= 5U;
            in[k].moving = r >= 6U && r <= 8U;
            in[k].move_dir[0] = (r % 2U) != 0U ? 1.0F : -1.0F;
        }
        eng_sim_step(&a, in);
        eng_sim_step(&b, in);
        if (t == 200) {
            in[0].light = !in[0].light; /* une seule entrée change */
        }
        eng_sim_step(&c, in);
        EXPECT_EQ(eng_sim_hash(&a), eng_sim_hash(&b));
    }
    EXPECT_NE(eng_sim_hash(&a), eng_sim_hash(&c));
}

UTEST(combat_sim, hurtbox_and_weapon_geometry_follows_position_and_yaw)
{
    EngSimBody b;
    memset(&b, 0, sizeof b);
    b.pos[0] = 1.0F;
    b.pos[1] = 2.0F;
    b.yaw = 0.0F;
    EngCapsule box[ENG_SIM_HURTBOXES];
    EngHitZone zones[ENG_SIM_HURTBOXES];
    eng_sim_hurtboxes(&b, false, box, zones);
    EXPECT_EQ(zones[0], ENG_ZONE_HEAD);
    EXPECT_EQ(zones[1], ENG_ZONE_TORSO);
    EXPECT_EQ(zones[2], ENG_ZONE_LIMB);
    EXPECT_NEAR(box[1].a[0], 1.0F, 1e-6F);
    EXPECT_NEAR(box[1].a[1], 0.9F, 1e-6F);
    EXPECT_NEAR(box[1].a[2], 2.0F, 1e-6F);
    load();
    EngCapsule w[ENG_ATTACK_MAX_HITBOXES];
    EXPECT_EQ(eng_sim_weapons(&b, false, &g_set.attacks[0], w), 1U);
    EXPECT_NEAR(w[0].b[2], 2.0F - 0.6F, 1e-5F); /* +Z local = avant = -Z monde */
    EXPECT_NEAR(w[0].b[1], 1.1F, 1e-6F);
    b.yaw = 1.57079633F; /* avant = -X */
    EXPECT_EQ(eng_sim_weapons(&b, false, &g_set.attacks[0], w), 1U);
    EXPECT_NEAR(w[0].b[0], 1.0F - 0.6F, 1e-5F);
    EXPECT_NEAR(w[0].b[2], 2.0F, 1e-5F);
    b.prev_pos[0] = 5.0F;
    EXPECT_EQ(eng_sim_weapons(&b, true, &g_set.attacks[0], w), 1U);
    EXPECT_NEAR(w[0].a[0], 5.0F, 1e-5F); /* pose précédente */
}

UTEST(combat_sim, invalid_arguments_are_refused)
{
    load();
    EngCombatSim s;
    EXPECT_FALSE(eng_sim_init(NULL, &g_set, 2));
    EXPECT_FALSE(eng_sim_init(&s, NULL, 2));
    EXPECT_FALSE(eng_sim_init(&s, &g_set, 0));
    EXPECT_FALSE(eng_sim_init(&s, &g_set, ENG_SIM_MAX + 1U));
    EXPECT_TRUE(eng_sim_init(&s, &g_set, ENG_SIM_MAX));
    EXPECT_FALSE(eng_sim_place(&s, ENG_SIM_MAX, 0, 0, 0));
    EXPECT_FALSE(eng_sim_place(&s, 0, NAN, 0, 0));
    EXPECT_FALSE(eng_sim_place(NULL, 0, 0, 0, 0));
    eng_sim_step(NULL, NULL);
    eng_sim_step(&s, NULL);
    EXPECT_EQ(s.tick, 0U);
    EXPECT_EQ(eng_sim_weapons(NULL, false, &g_set.attacks[0], NULL), 0U);
}

UTEST(combat_sim, non_unit_move_vectors_are_normalised)
{
    EngCombatSim s;
    duel(&s, 10.0F);
    EngFighterInput in[2] = {none(), none()};
    in[0].moving = true;
    in[0].move_dir[0] = 3.0F;
    in[0].move_dir[1] = 4.0F; /* norme 5 */
    for (int i = 0; i < 60; i++) {
        eng_sim_step(&s, in);
    }
    EXPECT_NEAR(s.bodies[0].pos[0], 2.4F, 1e-3F); /* 4 m × 0,6 */
    EXPECT_NEAR(s.bodies[0].pos[1], 3.2F, 1e-3F); /* 4 m × 0,8 */
    EngCombatSim t;
    duel(&t, 10.0F);
    in[0].move_dir[0] = 0.3F; /* sous le seuil de 0,5 : pas de déplacement */
    in[0].move_dir[1] = 0.0F;
    for (int i = 0; i < 10; i++) {
        eng_sim_step(&t, in);
    }
    EXPECT_NEAR(t.bodies[0].pos[0], 0.0F, 1e-6F);
}

UTEST(combat_sim, the_knockback_direction_goes_from_the_attacker_towards_the_defender)
{
    EngCombatSim s;
    duel(&s, 0.75F);
    ASSERT_TRUE(eng_sim_place(&s, 1, 0.2F, -0.75F, 3.14159265F));
    EngFighterInput in[2] = {none(), none()};
    in[0].light = true;
    eng_sim_step(&s, in);
    in[0] = none();
    for (int i = 0; i < 8; i++) {
        eng_sim_step(&s, in);
    }
    ASSERT_EQ(s.total_hits, 1U);
    const float len = sqrtf(0.2F * 0.2F + 0.75F * 0.75F);
    EXPECT_NEAR(s.bodies[1].pos[0], 0.2F + 0.16F * 0.2F / len, 1e-4F);
    EXPECT_NEAR(s.bodies[1].pos[1], -0.75F - 0.16F * 0.75F / len, 1e-4F);
}

UTEST(combat_sim, weapon_offsets_follow_the_lateral_axis_and_unused_slots_stay_untouched)
{
    EngAttack atk;
    memset(&atk, 0, sizeof atk);
    atk.hitbox_count = 1;
    atk.hitboxes[0].a[0] = 0.3F;
    atk.hitboxes[0].b[0] = 0.3F;
    atk.hitboxes[0].b[2] = 0.6F;
    atk.hitboxes[0].radius = 0.1F;
    EngSimBody b;
    memset(&b, 0, sizeof b);
    b.pos[0] = 1.0F;
    b.pos[1] = 2.0F;
    b.yaw = 1.57079633F; /* avant = -X, droite = -Z */
    EngCapsule w[ENG_ATTACK_MAX_HITBOXES];
    for (uint32_t i = 0; i < ENG_ATTACK_MAX_HITBOXES; i++) {
        w[i].radius = -5.0F;
    }
    EXPECT_EQ(eng_sim_weapons(&b, false, &atk, w), 1U);
    EXPECT_NEAR(w[0].a[0], 1.0F, 1e-5F);
    EXPECT_NEAR(w[0].a[2], 1.7F, 1e-5F);
    EXPECT_NEAR(w[0].b[0], 0.4F, 1e-5F);
    EXPECT_NEAR(w[0].b[2], 1.7F, 1e-5F);
    EXPECT_NEAR(w[1].radius, -5.0F, 1e-9F);
}
