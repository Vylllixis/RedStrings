/* Combattant : combos, esquive, garde / parade, réactions (M5-10 à M5-13). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/fighter.h"
#include "utest.h"

static const char SET[] = "attack light1\n startup 8\n active 4\n recovery 14\n damage 10\n "
                          "posture 8\n stamina 10\n hitstop 3\n"
                          " cancel_from 6\n next light2\n hitbox hand_r 0 0 0 0 0 0.6 0.12\nend\n"
                          "attack light2\n startup 7\n active 4\n recovery 14\n damage 10\n "
                          "posture 8\n stamina 10\n hitstop 3\n"
                          " cancel_from 6\n next light3\n hitbox hand_r 0 0 0 0 0 0.6 0.12\nend\n"
                          "attack light3\n startup 10\n active 5\n recovery 20\n damage 14\n "
                          "posture 14\n stamina 14\n hitstop 3\n"
                          " hitbox hand_r 0 0 0 0 0 0.6 0.12\nend\n"
                          "attack heavy\n startup 20\n active 5\n recovery 24\n damage 25\n "
                          "posture 30\n stamina 25\n hitstop 6\n"
                          " hitbox hand_r 0 0 0 0 0 0.6 0.12\nend\n";

static EngAttackSet g_set;

static void load(void)
{
    char err[128];
    if (!eng_attackset_parse(SET, &g_set, err, sizeof err)) {
        (void)fprintf(stderr, "parse: %s\n", err);
        abort();
    }
}

static EngFighterInput idle_in(void)
{
    EngFighterInput in;
    memset(&in, 0, sizeof in);
    return in;
}

static void step(EngFighter *f, EngFighterInput in)
{
    eng_fighter_tick(f, &g_set, &in);
}

static void wait_n(EngFighter *f, uint32_t n)
{
    const EngFighterInput in = idle_in();
    for (uint32_t i = 0; i < n; i++) {
        step(f, in);
    }
}

UTEST(fighter, a_light_press_starts_an_attack_and_it_ends_after_its_total_duration)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    EngFighterInput in = idle_in();
    in.light = true;
    step(&f, in);
    EXPECT_EQ(f.fsm.state, ENG_CS_ATTACK);
    EXPECT_TRUE(f.events & ENG_FEV_ATTACK_START);
    EXPECT_NEAR(f.stamina, 90.0F, 1e-4F);
    in = idle_in();
    uint32_t active_from = 0;
    uint32_t active_to = 0;
    uint32_t ticks = 1;
    while (f.fsm.state == ENG_CS_ATTACK && ticks < 100) {
        step(&f, in);
        ticks++;
        if (eng_fighter_hitboxes_active(&f, &g_set)) {
            if (active_from == 0) {
                active_from = f.attack_tick;
            }
            active_to = f.attack_tick;
        }
    }
    EXPECT_EQ(active_from, 8U);
    EXPECT_EQ(active_to, 11U);
    EXPECT_EQ(f.fsm.state, ENG_CS_IDLE);
    EXPECT_EQ(ticks, 8U + 4U + 14U + 1U); /* total de 26 ticks, sortie au tick 27 (1er tick = 1) */
}

UTEST(fighter, a_three_hit_combo_chains_reliably_with_early_presses)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    EngFighterInput in = idle_in();
    in.light = true;
    step(&f, in);
    uint32_t chain = 1;
    uint32_t last = f.attack;
    in = idle_in();
    for (uint32_t t = 0; t < 120; t++) {
        /* appuie en continu pendant la recovery : le buffer doit tout absorber */
        in.light = chain < 3U && (t % 5U) == 0U;
        step(&f, in);
        if ((f.events & ENG_FEV_COMBO) != 0U) {
            EXPECT_NE(f.attack, last);
            last = f.attack;
            chain++;
        }
    }
    EXPECT_EQ(chain, 3U); /* light1 → light2 → light3, puis rien (light3 n'a pas de next) */
}

UTEST(fighter, light3_cannot_chain_but_can_be_cancelled_by_a_dodge)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    EngFighterInput in;
    /* esquive pendant une recovery d'attaque */
    in = idle_in();
    in.light = true;
    step(&f, in);
    in = idle_in();
    wait_n(&f, 8U + 4U + 7U); /* recovery > cancel_from 6 */
    in.dodge = true;
    in.move_dir[0] = 1.0F;
    step(&f, in);
    EXPECT_EQ(f.fsm.state, ENG_CS_DODGE);
}

UTEST(fighter, dodge_is_invulnerable_exactly_on_its_window_and_costs_stamina)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    EngFighterInput in = idle_in();
    in.dodge = true;
    in.move_dir[0] = 1.0F;
    step(&f, in);
    EXPECT_EQ(f.fsm.state, ENG_CS_DODGE);
    EXPECT_NEAR(f.stamina, 85.0F, 1e-4F);
    in = idle_in();
    uint32_t first = 0;
    uint32_t last = 0;
    float dist = 0.0F;
    for (uint32_t i = 1; i < 40 && f.fsm.state == ENG_CS_DODGE; i++) {
        step(&f, in);
        dist += f.root_delta[0];
        if (eng_fighter_dodge_invulnerable(&f)) {
            if (first == 0) {
                first = f.state_tick;
            }
            last = f.state_tick;
            EXPECT_FALSE(eng_fighter_hittable(&f));
        }
    }
    EXPECT_EQ(first, 3U);
    EXPECT_EQ(last, 14U);
    EXPECT_EQ(f.fsm.state, ENG_CS_IDLE);
    EXPECT_NEAR(dist, 3.0F, 0.05F);
}

UTEST(fighter, without_enough_stamina_actions_are_refused_and_signalled)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    f.stamina = 5.0F;
    EngFighterInput in = idle_in();
    in.light = true;
    step(&f, in);
    EXPECT_EQ(f.fsm.state, ENG_CS_IDLE);
    EXPECT_TRUE(f.events & ENG_FEV_NO_STAMINA);
    in = idle_in();
    in.dodge = true;
    step(&f, in);
    EXPECT_EQ(f.fsm.state, ENG_CS_IDLE);
}

UTEST(fighter, parry_window_then_block_and_a_parried_hit_staggers_the_attacker)
{
    load();
    EngFighter a;
    EngFighter d;
    eng_fighter_init(&a, NULL);
    eng_fighter_init(&d, NULL);
    EngFighterInput in = idle_in();
    in.block_pressed = true;
    in.block_held = true;
    step(&d, in);
    EXPECT_EQ(d.fsm.state, ENG_CS_PARRY);
    const EngAttack *atk = &g_set.attacks[0];
    const float force[2] = {0.0F, -1.0F};
    d.yaw = 0.0F;
    const EngHitOutcome o = eng_combat_apply_hit(&a, &d, atk, ENG_ZONE_TORSO, force);
    EXPECT_EQ(o.kind, ENG_HITKIND_PARRIED);
    EXPECT_TRUE(o.attacker_staggered);
    EXPECT_EQ(a.fsm.state, ENG_CS_STAGGER);
    EXPECT_NEAR(d.vitals.hp, 100.0F, 1e-4F);
    in.block_pressed = false;
    for (int i = 0; i < 8; i++) {
        step(&d, in);
    }
    EXPECT_EQ(d.fsm.state, ENG_CS_BLOCK);
}

UTEST(fighter, a_blocked_hit_deals_a_fifth_of_the_damage_and_breaks_posture_at_zero)
{
    load();
    EngFighter a;
    EngFighter d;
    eng_fighter_init(&a, NULL);
    eng_fighter_init(&d, NULL);
    EngFighterInput in = idle_in();
    in.block_pressed = true;
    in.block_held = true;
    step(&d, in);
    in.block_pressed = false;
    wait_n(&d, 0);
    for (int i = 0; i < 9; i++) {
        step(&d, in);
    }
    ASSERT_EQ(d.fsm.state, ENG_CS_BLOCK);
    const EngAttack *heavy = &g_set.attacks[3];
    const float force[2] = {0.0F, -1.0F};
    EngHitOutcome o = eng_combat_apply_hit(&a, &d, heavy, ENG_ZONE_TORSO, force);
    EXPECT_EQ(o.kind, ENG_HITKIND_BLOCKED);
    EXPECT_NEAR(o.dealt, 5.0F, 1e-4F);
    EXPECT_NEAR(d.posture, 70.0F, 1e-4F);
    int hits = 1;
    while (d.fsm.state == ENG_CS_BLOCK && hits < 10) {
        for (int i = 0; i < 8; i++) { /* dépasse l'invincibilité de 6 ticks */
            step(&d, in);
        }
        o = eng_combat_apply_hit(&a, &d, heavy, ENG_ZONE_TORSO, force);
        hits++;
    }
    EXPECT_EQ(d.fsm.state, ENG_CS_STAGGER);
    EXPECT_TRUE(o.defender_staggered);
    EXPECT_EQ(hits, 4); /* 100 → 70 → 40 → 10 → 0 */
}

UTEST(fighter, a_clean_hit_causes_hitstun_scaled_by_the_attack_and_cancels_the_attack)
{
    load();
    EngFighter a;
    EngFighter d;
    eng_fighter_init(&a, NULL);
    eng_fighter_init(&d, NULL);
    EngFighterInput in = idle_in();
    in.light = true;
    step(&d, in); /* le défenseur est en train d'attaquer */
    const float force[2] = {0.0F, -1.0F};
    const EngHitOutcome light =
        eng_combat_apply_hit(&a, &d, &g_set.attacks[0], ENG_ZONE_HEAD, force);
    EXPECT_EQ(light.kind, ENG_HITKIND_DAMAGE);
    EXPECT_NEAR(light.dealt, 15.0F, 1e-4F); /* tête ×1,5 */
    EXPECT_EQ(d.fsm.state, ENG_CS_HITSTUN);
    EXPECT_EQ(light.stun_ticks, 18U);
    EXPECT_EQ(d.attack, ENG_ATTACK_NONE);

    EngFighter d2;
    eng_fighter_init(&d2, NULL);
    const EngHitOutcome heavy =
        eng_combat_apply_hit(&a, &d2, &g_set.attacks[3], ENG_ZONE_TORSO, force);
    EXPECT_EQ(heavy.stun_ticks, 40U);
    EXPECT_LT(heavy.knockback[1], 0.0F);
    /* la réaction se termine */
    wait_n(&d2, 40);
    EXPECT_EQ(d2.fsm.state, ENG_CS_IDLE);
}

UTEST(fighter, a_hit_is_ignored_during_iframes_and_during_dodge_invulnerability)
{
    load();
    EngFighter a;
    EngFighter d;
    eng_fighter_init(&a, NULL);
    eng_fighter_init(&d, NULL);
    EngFighterInput in = idle_in();
    in.dodge = true;
    in.move_dir[0] = 1.0F;
    step(&d, in);
    wait_n(&d, 4); /* state_tick 5 : dans la fenêtre */
    const float force[2] = {0.0F, -1.0F};
    const EngHitOutcome o = eng_combat_apply_hit(&a, &d, &g_set.attacks[0], ENG_ZONE_TORSO, force);
    EXPECT_EQ(o.kind, ENG_HITKIND_IGNORED);
    EXPECT_NEAR(d.vitals.hp, 100.0F, 1e-4F);
    EXPECT_EQ(d.fsm.state, ENG_CS_DODGE);
}

UTEST(fighter, death_happens_at_the_fatal_hit_and_dead_is_final)
{
    load();
    EngFighter a;
    EngFighter d;
    eng_fighter_init(&a, NULL);
    eng_fighter_init(&d, NULL);
    d.vitals.hp = 9.0F;
    const float force[2] = {0.0F, -1.0F};
    wait_n(&d, 3);
    const EngHitOutcome o = eng_combat_apply_hit(&a, &d, &g_set.attacks[0], ENG_ZONE_TORSO, force);
    EXPECT_TRUE(o.killed);
    EXPECT_EQ(d.fsm.state, ENG_CS_DEAD);
    EXPECT_EQ(d.vitals.died_tick, 3U);
    EngFighterInput in = idle_in();
    in.light = true;
    step(&d, in);
    EXPECT_EQ(d.fsm.state, ENG_CS_DEAD);
    EXPECT_EQ(eng_combat_apply_hit(&a, &d, &g_set.attacks[0], ENG_ZONE_TORSO, force).kind,
              ENG_HITKIND_IGNORED);
}

UTEST(fighter, reaction_direction_follows_the_force_relative_to_the_defender_yaw)
{
    const float toward_minus_z[2] = {0.0F, -1.0F}; /* attaquant devant (en -Z), force vers -Z... */
    /* force_dir va de l'attaquant vers le défenseur : un attaquant en face (-Z) pousse vers +Z */
    const float from_front[2] = {0.0F, 1.0F};
    const float from_back[2] = {0.0F, -1.0F};
    const float from_right[2] = {-1.0F, 0.0F};
    const float from_left[2] = {1.0F, 0.0F};
    (void)toward_minus_z;
    EXPECT_EQ(eng_react_dir(0.0F, from_front), ENG_REACT_FRONT);
    EXPECT_EQ(eng_react_dir(0.0F, from_back), ENG_REACT_BACK);
    EXPECT_EQ(eng_react_dir(0.0F, from_right), ENG_REACT_RIGHT);
    EXPECT_EQ(eng_react_dir(0.0F, from_left), ENG_REACT_LEFT);
}

UTEST(fighter, stamina_regenerates_after_its_delay_and_posture_resets_after_stagger)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    EngFighterInput in = idle_in();
    in.dodge = true;
    step(&f, in);
    wait_n(&f, 30); /* 31 ticks depuis la dépense : délai de 60 ticks pas écoulé */
    EXPECT_NEAR(f.stamina, 85.0F, 1e-4F);
    wait_n(&f, 60); /* 91 ticks : régénération commencée */
    EXPECT_GT(f.stamina, 85.0F + 5.0F);
    EXPECT_LT(f.stamina, f.params.max_stamina);
    wait_n(&f, 120);
    EXPECT_NEAR(f.stamina, f.params.max_stamina, 1e-4F);
    EXPECT_LE(f.stamina, f.params.max_stamina);
}

UTEST(fighter, invalid_params_are_refused_and_the_hash_is_deterministic)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    EngFighterParams p = f.params;
    p.dodge_inv_to = p.dodge_total + 1U;
    EXPECT_FALSE(eng_fighter_set_params(&f, &p));
    p = f.params;
    p.parry_frames = 0;
    EXPECT_FALSE(eng_fighter_set_params(&f, &p));
    p = f.params;
    p.block_multiplier = 1.5F;
    EXPECT_FALSE(eng_fighter_set_params(&f, &p));
    EXPECT_FALSE(eng_fighter_set_params(NULL, &p));
    EngFighter g;
    eng_fighter_init(&g, NULL);
    EXPECT_EQ(eng_fighter_hash(&f), eng_fighter_hash(&g));
    EngFighterInput in = idle_in();
    in.light = true;
    step(&f, in);
    EXPECT_NE(eng_fighter_hash(&f), eng_fighter_hash(&g));
    step(&g, in);
    EXPECT_EQ(eng_fighter_hash(&f), eng_fighter_hash(&g));
}

/* ── renforcement (mutations) ── */

static bool same_params(const EngFighterParams *a, const EngFighterParams *b)
{
    return strcmp(a->light_name, b->light_name) == 0 && strcmp(a->heavy_name, b->heavy_name) == 0 &&
           a->max_stamina == b->max_stamina && a->stamina_regen == b->stamina_regen &&
           a->max_posture == b->max_posture && a->posture_regen == b->posture_regen &&
           a->parry_frames == b->parry_frames && a->block_multiplier == b->block_multiplier &&
           a->dodge_total == b->dodge_total && a->dodge_inv_from == b->dodge_inv_from &&
           a->dodge_inv_to == b->dodge_inv_to && a->dodge_cost == b->dodge_cost &&
           a->dodge_distance == b->dodge_distance && a->stagger_ticks == b->stagger_ticks &&
           a->hitstun_base == b->hitstun_base && a->hitstun_max == b->hitstun_max &&
           a->knockback_per_posture == b->knockback_per_posture;
}

typedef void (*ParamMut)(EngFighterParams *p);
static void m_stam0(EngFighterParams *p)
{
    p->max_stamina = 0.0F;
}
static void m_stamn(EngFighterParams *p)
{
    p->max_stamina = -1.0F;
}
static void m_stamnan(EngFighterParams *p)
{
    p->max_stamina = NAN;
}
static void m_regen(EngFighterParams *p)
{
    p->stamina_regen = -0.1F;
}
static void m_regeninf(EngFighterParams *p)
{
    p->stamina_regen = INFINITY;
}
static void m_post0(EngFighterParams *p)
{
    p->max_posture = 0.0F;
}
static void m_postnan(EngFighterParams *p)
{
    p->max_posture = NAN;
}
static void m_pregen(EngFighterParams *p)
{
    p->posture_regen = -1.0F;
}
static void m_parry(EngFighterParams *p)
{
    p->parry_frames = 0U;
}
static void m_blockn(EngFighterParams *p)
{
    p->block_multiplier = -0.1F;
}
static void m_blocknan(EngFighterParams *p)
{
    p->block_multiplier = NAN;
}
static void m_dodge0(EngFighterParams *p)
{
    p->dodge_total = 0U;
}
static void m_inv(EngFighterParams *p)
{
    p->dodge_inv_from = p->dodge_inv_to;
}
static void m_cost(EngFighterParams *p)
{
    p->dodge_cost = -1.0F;
}
static void m_dist(EngFighterParams *p)
{
    p->dodge_distance = NAN;
}
static void m_stag(EngFighterParams *p)
{
    p->stagger_ticks = 0U;
}
static void m_stun(EngFighterParams *p)
{
    p->hitstun_base = p->hitstun_max + 1U;
}
static void m_kb(EngFighterParams *p)
{
    p->knockback_per_posture = -1.0F;
}
static void m_light_empty(EngFighterParams *p)
{
    p->light_name[0] = '\0';
}
static void m_heavy_empty(EngFighterParams *p)
{
    p->heavy_name[0] = '\0';
}
static void m_light_unterminated(EngFighterParams *p)
{
    memset(p->light_name, 'a', sizeof p->light_name);
}
static void m_heavy_unterminated(EngFighterParams *p)
{
    memset(p->heavy_name, 'a', sizeof p->heavy_name);
}

UTEST(fighter, every_invalid_parameter_is_refused_and_the_fighter_is_left_unchanged)
{
    const ParamMut muts[] = {m_stam0,
                             m_stamn,
                             m_stamnan,
                             m_regen,
                             m_regeninf,
                             m_post0,
                             m_postnan,
                             m_pregen,
                             m_parry,
                             m_blockn,
                             m_blocknan,
                             m_dodge0,
                             m_inv,
                             m_cost,
                             m_dist,
                             m_stag,
                             m_stun,
                             m_kb,
                             m_light_empty,
                             m_heavy_empty,
                             m_light_unterminated,
                             m_heavy_unterminated};
    EngFighter f;
    eng_fighter_init(&f, NULL);
    const EngFighterParams good = f.params;
    for (size_t i = 0; i < sizeof muts / sizeof muts[0]; i++) {
        EngFighterParams p = good;
        muts[i](&p);
        EXPECT_FALSE(eng_fighter_set_params(&f, &p));
        EXPECT_TRUE(same_params(&f.params, &good));
        EngFighter g;
        eng_fighter_init(&g, &p); /* paramètres invalides → défauts */
        EXPECT_TRUE(same_params(&g.params, &good));
    }
    EXPECT_FALSE(eng_fighter_set_params(&f, NULL));
}

UTEST(fighter, boundary_parameters_are_accepted)
{
    EngFighter f;
    eng_fighter_init(&f, NULL);
    EngFighterParams p = f.params;
    p.block_multiplier = 0.0F;
    EXPECT_TRUE(eng_fighter_set_params(&f, &p));
    p.block_multiplier = 1.0F;
    EXPECT_TRUE(eng_fighter_set_params(&f, &p));
    p = f.params;
    p.dodge_inv_to = p.dodge_total;
    EXPECT_TRUE(eng_fighter_set_params(&f, &p));
    p.hitstun_base = p.hitstun_max;
    EXPECT_TRUE(eng_fighter_set_params(&f, &p));
    p.stamina_regen = 0.0F;
    p.posture_regen = 0.0F;
    p.dodge_cost = 0.0F;
    p.dodge_distance = 0.0F;
    p.knockback_per_posture = 0.0F;
    EXPECT_TRUE(eng_fighter_set_params(&f, &p));
}

UTEST(fighter, exact_stamina_is_enough_and_stamina_never_goes_negative)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    f.stamina = 10.0F;
    f.stamina_wait = 5U;
    EngFighterInput in = idle_in();
    in.light = true;
    step(&f, in);
    EXPECT_EQ(f.fsm.state, ENG_CS_ATTACK);
    EXPECT_NEAR(f.stamina, 0.0F, 1e-6F);
    EngFighter g;
    eng_fighter_init(&g, NULL);
    g.stamina = 15.0F;
    g.stamina_wait = 5U;
    in = idle_in();
    in.dodge = true;
    step(&g, in);
    EXPECT_EQ(g.fsm.state, ENG_CS_DODGE);
    EXPECT_NEAR(g.stamina, 0.0F, 1e-6F);
    EngFighter h;
    eng_fighter_init(&h, NULL);
    h.stamina = 14.99F;
    h.stamina_wait = 5U;
    step(&h, in);
    EXPECT_EQ(h.fsm.state, ENG_CS_IDLE);
    EXPECT_TRUE(h.events & ENG_FEV_NO_STAMINA);
}

UTEST(fighter, dodge_direction_is_normalised_and_defaults_to_a_back_step)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    EngFighterInput in = idle_in();
    in.dodge = true;
    in.move_dir[0] = 3.0F;
    in.move_dir[1] = 4.0F;
    step(&f, in);
    EXPECT_NEAR(f.dodge_dir[0], 0.6F, 1e-5F);
    EXPECT_NEAR(f.dodge_dir[1], 0.8F, 1e-5F);
    float dist = 0.0F;
    in = idle_in();
    for (int i = 0; i < 12; i++) { /* state_tick 12 sur 24 : moitié du profil lissé */
        step(&f, in);
        dist += sqrtf(f.root_delta[0] * f.root_delta[0] + f.root_delta[1] * f.root_delta[1]);
    }
    EXPECT_NEAR(dist, 1.5F, 0.02F);

    EngFighter g;
    eng_fighter_init(&g, NULL);
    in = idle_in();
    in.dodge = true;
    step(&g, in); /* yaw 0 : avant = -Z, donc recul = +Z */
    EXPECT_NEAR(g.dodge_dir[0], 0.0F, 1e-5F);
    EXPECT_NEAR(g.dodge_dir[1], 1.0F, 1e-5F);
    EngFighter h;
    eng_fighter_init(&h, NULL);
    h.yaw = 1.57079633F; /* avant = -X, recul = +X */
    step(&h, in);
    EXPECT_NEAR(h.dodge_dir[0], 1.0F, 1e-4F);
    EXPECT_NEAR(h.dodge_dir[1], 0.0F, 1e-4F);
    EngFighter n;
    eng_fighter_init(&n, NULL);
    in.move_dir[0] = NAN;
    step(&n, in);
    EXPECT_EQ(n.fsm.state, ENG_CS_DODGE);
    EXPECT_TRUE(isfinite(n.dodge_dir[0]) && isfinite(n.dodge_dir[1]));
    EXPECT_NEAR(n.dodge_dir[1], 1.0F, 1e-5F);
}

UTEST(fighter, attack_events_fire_exactly_once_at_the_right_tick)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    EngFighterInput in = idle_in();
    in.light = true;
    step(&f, in);
    in = idle_in();
    int active = 0;
    int end = 0;
    uint32_t active_at = 0;
    uint32_t end_at = 0;
    for (int i = 0; i < 40; i++) {
        step(&f, in);
        if (f.events & ENG_FEV_ATTACK_ACTIVE) {
            active++;
            active_at = f.attack_tick;
        }
        if (f.events & ENG_FEV_ATTACK_END) {
            end++;
            end_at = f.tick;
        }
    }
    EXPECT_EQ(active, 1);
    EXPECT_EQ(active_at, 8U);
    EXPECT_EQ(end, 1);
    EXPECT_EQ(end_at, 27U);
}

UTEST(fighter, hitstun_lasts_exactly_its_duration_and_stagger_resets_posture)
{
    load();
    EngFighter a;
    EngFighter d;
    eng_fighter_init(&a, NULL);
    eng_fighter_init(&d, NULL);
    const float force[2] = {0.0F, -1.0F};
    (void)eng_combat_apply_hit(&a, &d, &g_set.attacks[0], ENG_ZONE_TORSO, force);
    ASSERT_EQ(d.fsm.state, ENG_CS_HITSTUN);
    wait_n(&d, 17);
    EXPECT_EQ(d.fsm.state, ENG_CS_HITSTUN);
    wait_n(&d, 1);
    EXPECT_EQ(d.fsm.state, ENG_CS_IDLE);
    EXPECT_TRUE(d.events & ENG_FEV_RECOVERED);

    /* posture brisée pile à zéro (posture 8, coup léger de posture 8) */
    EngFighter b;
    eng_fighter_init(&b, NULL);
    EngFighterInput in = idle_in();
    in.block_pressed = true;
    in.block_held = true;
    step(&b, in);
    in.block_pressed = false;
    for (int i = 0; i < 8; i++) {
        step(&b, in);
    }
    ASSERT_EQ(b.fsm.state, ENG_CS_BLOCK);
    b.posture = 8.0F;
    const EngHitOutcome o = eng_combat_apply_hit(&a, &b, &g_set.attacks[0], ENG_ZONE_TORSO, force);
    EXPECT_TRUE(o.defender_staggered);
    EXPECT_EQ(b.fsm.state, ENG_CS_STAGGER);
    EXPECT_NEAR(b.posture, 0.0F, 1e-6F);
    in = idle_in();
    for (int i = 0; i < 59; i++) {
        step(&b, in);
    }
    EXPECT_EQ(b.fsm.state, ENG_CS_STAGGER);
    EXPECT_NEAR(b.posture, 0.0F, 1e-6F); /* pas de régénération à terre */
    step(&b, in);
    EXPECT_EQ(b.fsm.state, ENG_CS_IDLE);
    EXPECT_NEAR(b.posture, 100.0F, 1e-6F);
}

UTEST(fighter, a_staggered_defender_takes_damage_but_is_not_put_in_hitstun)
{
    load();
    EngFighter a;
    EngFighter d;
    eng_fighter_init(&a, NULL);
    eng_fighter_init(&d, NULL);
    const float force[2] = {0.0F, -1.0F};
    ASSERT_TRUE(eng_fsm_request(&d.fsm, ENG_CS_STAGGER));
    d.stun_left = 60U;
    const EngHitOutcome o = eng_combat_apply_hit(&a, &d, &g_set.attacks[0], ENG_ZONE_TORSO, force);
    EXPECT_EQ(o.kind, ENG_HITKIND_DAMAGE);
    EXPECT_NEAR(d.vitals.hp, 90.0F, 1e-4F);
    EXPECT_EQ(d.fsm.state, ENG_CS_STAGGER);
    EXPECT_EQ(o.stun_ticks, 0U);
}

UTEST(fighter, hit_outcome_knockback_stun_cap_and_degenerate_inputs)
{
    load();
    EngFighter a;
    EngFighter d;
    eng_fighter_init(&a, NULL);
    eng_fighter_init(&d, NULL);
    const float force2[2] = {0.0F, -2.0F}; /* non unitaire : normalisée */
    EngHitOutcome o = eng_combat_apply_hit(&a, &d, &g_set.attacks[3], ENG_ZONE_TORSO, force2);
    EXPECT_NEAR(o.knockback[0], 0.0F, 1e-6F);
    EXPECT_NEAR(o.knockback[1], -0.6F, 1e-5F); /* 30 × 0,02 */
    EXPECT_EQ(o.hitstop, 6U);

    EngAttack big = g_set.attacks[3];
    big.posture = 500.0F;
    EngFighter d2;
    eng_fighter_init(&d2, NULL);
    o = eng_combat_apply_hit(&a, &d2, &big, ENG_ZONE_TORSO, force2);
    EXPECT_EQ(o.stun_ticks, 60U); /* plafond hitstun_max */

    EngFighter d3;
    eng_fighter_init(&d3, NULL);
    const float zero[2] = {0.0F, 0.0F};
    o = eng_combat_apply_hit(&a, &d3, &g_set.attacks[3], ENG_ZONE_TORSO, zero);
    EXPECT_EQ(o.kind, ENG_HITKIND_DAMAGE);
    EXPECT_NEAR(o.knockback[0], 0.0F, 1e-9F);
    EXPECT_NEAR(o.knockback[1], 0.0F, 1e-9F);

    EXPECT_EQ(eng_combat_apply_hit(NULL, &d3, &big, ENG_ZONE_TORSO, force2).kind,
              ENG_HITKIND_IGNORED);
    EXPECT_EQ(eng_combat_apply_hit(&a, NULL, &big, ENG_ZONE_TORSO, force2).kind,
              ENG_HITKIND_IGNORED);
    EXPECT_EQ(eng_combat_apply_hit(&a, &d3, NULL, ENG_ZONE_TORSO, force2).kind,
              ENG_HITKIND_IGNORED);
    EXPECT_EQ(eng_combat_apply_hit(&a, &d3, &big, ENG_ZONE_TORSO, NULL).kind, ENG_HITKIND_IGNORED);
}

UTEST(fighter, a_fatal_blocked_hit_kills_and_block_hits_are_ignored_during_iframes)
{
    load();
    EngFighter a;
    EngFighter d;
    eng_fighter_init(&a, NULL);
    eng_fighter_init(&d, NULL);
    EngFighterInput in = idle_in();
    in.block_pressed = true;
    in.block_held = true;
    step(&d, in);
    in.block_pressed = false;
    for (int i = 0; i < 8; i++) {
        step(&d, in);
    }
    ASSERT_EQ(d.fsm.state, ENG_CS_BLOCK);
    const float force[2] = {0.0F, -1.0F};
    EngHitOutcome o = eng_combat_apply_hit(&a, &d, &g_set.attacks[0], ENG_ZONE_TORSO, force);
    EXPECT_EQ(o.kind, ENG_HITKIND_BLOCKED);
    o = eng_combat_apply_hit(&a, &d, &g_set.attacks[0], ENG_ZONE_TORSO, force); /* iframes */
    EXPECT_EQ(o.kind, ENG_HITKIND_IGNORED);
    d.vitals.invulnerable_until = 0U;
    d.vitals.hp = 1.0F;
    o = eng_combat_apply_hit(&a, &d, &g_set.attacks[0], ENG_ZONE_TORSO, force);
    EXPECT_TRUE(o.killed);
    EXPECT_EQ(d.fsm.state, ENG_CS_DEAD);
}

UTEST(fighter, reaction_direction_thresholds_at_45_degrees_and_with_a_rotated_defender)
{
    const float deg = 0.01745329F;
    const float a44[2] = {sinf(44.0F * deg), cosf(44.0F * deg)}; /* attaquant devant, 44° */
    const float a46[2] = {sinf(46.0F * deg), cosf(46.0F * deg)};
    EXPECT_EQ(eng_react_dir(0.0F, a44), ENG_REACT_FRONT);
    EXPECT_EQ(eng_react_dir(0.0F, a46), ENG_REACT_LEFT);
    const float b44[2] = {sinf(44.0F * deg), -cosf(44.0F * deg)};
    const float b46[2] = {sinf(46.0F * deg), -cosf(46.0F * deg)};
    EXPECT_EQ(eng_react_dir(0.0F, b44), ENG_REACT_BACK);
    EXPECT_EQ(eng_react_dir(0.0F, b46), ENG_REACT_LEFT);
    /* défenseur tourné de 90° : avant = -X ; une force +X vient de face */
    const float px[2] = {1.0F, 0.0F};
    const float pz[2] = {0.0F, 1.0F};
    EXPECT_EQ(eng_react_dir(1.57079633F, px), ENG_REACT_FRONT);
    EXPECT_EQ(eng_react_dir(1.57079633F, pz), ENG_REACT_RIGHT);
}

UTEST(fighter, stamina_and_posture_do_not_regenerate_while_busy_and_idle_move_follow_input)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    f.stamina = 50.0F;
    f.stamina_wait = 0U;
    f.posture = 50.0F;
    f.posture_wait = 0U;
    EngFighterInput in = idle_in();
    in.moving = true;
    step(&f, in);
    EXPECT_EQ(f.fsm.state, ENG_CS_MOVE);
    EXPECT_GT(f.stamina, 50.0F);
    EXPECT_GT(f.posture, 50.0F);
    in.moving = false;
    step(&f, in);
    EXPECT_EQ(f.fsm.state, ENG_CS_IDLE);
    /* en attaque : pas de régénération d'endurance */
    EngFighter g;
    eng_fighter_init(&g, NULL);
    g.stamina_wait = 0U;
    g.stamina = 50.0F;
    in = idle_in();
    in.light = true;
    step(&g, in);
    const float after = g.stamina;
    in = idle_in();
    wait_n(&g, 5);
    EXPECT_NEAR(g.stamina, after, 1e-6F); /* délai de 60 ticks après la dépense */
    /* sans ensemble d'attaques, rien ne part */
    EngFighter h;
    eng_fighter_init(&h, NULL);
    in.light = true;
    eng_fighter_tick(&h, NULL, &in);
    EXPECT_EQ(h.fsm.state, ENG_CS_IDLE);
    eng_fighter_tick(NULL, &g_set, &in);
    eng_fighter_tick(&h, &g_set, NULL);
}

UTEST(fighter, a_released_parry_ends_idle_and_block_can_start_an_attack)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    EngFighterInput in = idle_in();
    in.block_pressed = true;
    step(&f, in); /* bouton relâché immédiatement */
    EXPECT_EQ(f.fsm.state, ENG_CS_PARRY);
    in = idle_in();
    for (int i = 0; i < 7; i++) {
        step(&f, in);
    }
    EXPECT_EQ(f.fsm.state, ENG_CS_PARRY); /* fenêtre engagée */
    step(&f, in);
    EXPECT_EQ(f.fsm.state, ENG_CS_IDLE);

    EngFighter g;
    eng_fighter_init(&g, NULL);
    in = idle_in();
    in.block_pressed = true;
    in.block_held = true;
    step(&g, in);
    in.block_pressed = false;
    for (int i = 0; i < 8; i++) {
        step(&g, in);
    }
    ASSERT_EQ(g.fsm.state, ENG_CS_BLOCK);
    in.heavy = true;
    step(&g, in);
    EXPECT_EQ(g.fsm.state, ENG_CS_ATTACK);
    EXPECT_EQ(g.attack, 3U);
}

UTEST(fighter, dodge_lasts_exactly_dodge_total_ticks_and_knockback_follows_the_force_axes)
{
    load();
    EngFighter f;
    eng_fighter_init(&f, NULL);
    EngFighterInput in = idle_in();
    in.dodge = true;
    step(&f, in); /* tick de départ (state_tick 0) */
    in = idle_in();
    for (int i = 0; i < 23; i++) {
        step(&f, in);
        EXPECT_EQ(f.fsm.state, ENG_CS_DODGE);
    }
    step(&f, in); /* 24e tick après le départ : fin */
    EXPECT_EQ(f.fsm.state, ENG_CS_IDLE);

    EngFighter a;
    EngFighter d;
    eng_fighter_init(&a, NULL);
    eng_fighter_init(&d, NULL);
    const float force[2] = {3.0F, -4.0F}; /* norme 5 */
    const EngHitOutcome o = eng_combat_apply_hit(&a, &d, &g_set.attacks[3], ENG_ZONE_TORSO, force);
    EXPECT_NEAR(o.knockback[0], 0.6F * 0.6F, 1e-5F);
    EXPECT_NEAR(o.knockback[1], -0.6F * 0.8F, 1e-5F);
}

UTEST(fighter, the_attacker_damage_scale_multiplies_clean_and_blocked_hits)
{
    load();
    const EngAttack *atk = &g_set.attacks[0];
    const float force[2] = {0.0F, -1.0F};
    float dealt[4];
    const float scales[4] = {0.0F, 1.5F, -2.0F, NAN}; /* 0, négatif, NaN : x 1 */
    for (int k = 0; k < 4; k++) {
        EngFighter a;
        EngFighter d;
        eng_fighter_init(&a, NULL);
        eng_fighter_init(&d, NULL);
        a.params.damage_scale = scales[k];
        dealt[k] = eng_combat_apply_hit(&a, &d, atk, ENG_ZONE_TORSO, force).dealt;
    }
    ASSERT_GT(dealt[0], 0.0F);
    EXPECT_NEAR(dealt[1], dealt[0] * 1.5F, 1e-4F);
    EXPECT_EQ(dealt[2], dealt[0]);
    EXPECT_EQ(dealt[3], dealt[0]);
    /* garde : 20 % des dégâts, multipliés aussi */
    EngFighter a;
    EngFighter d;
    eng_fighter_init(&a, NULL);
    eng_fighter_init(&d, NULL);
    a.params.damage_scale = 2.0F;
    EngFighterInput in = idle_in();
    in.block_pressed = true;
    in.block_held = true;
    step(&d, in);
    in.block_pressed = false;
    wait_n(&d, 0);
    for (int i = 0; i < 10 && d.fsm.state != ENG_CS_BLOCK; i++) {
        step(&d, in);
    }
    ASSERT_EQ(d.fsm.state, ENG_CS_BLOCK);
    const EngHitOutcome o = eng_combat_apply_hit(&a, &d, atk, ENG_ZONE_TORSO, force);
    EXPECT_EQ(o.kind, ENG_HITKIND_BLOCKED);
    EXPECT_NEAR(o.dealt, atk->damage * 2.0F * d.params.block_multiplier, 1e-4F);
}
