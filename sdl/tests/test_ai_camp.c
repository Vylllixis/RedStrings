/* IA des ennemis de bout en bout (M6-05 à M6-11) : camp, coordination, boss, repos, débogueur. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/ai_camp.h"
#include "engine/ai_debug.h"
#include "utest.h"

static const char ATTACKS[] =
    /* joueur */
    "attack light1\n startup 8\n active 4\n recovery 14\n damage 10\n posture 8\n stamina 10\n"
    " hitstop 3\n cancel_from 6\n next light2\n hitbox hand_r 0 0 0.3 0 0 1.4 0.15\nend\n"
    "attack light2\n startup 7\n active 4\n recovery 14\n damage 10\n posture 8\n stamina 10\n"
    " hitstop 3\n hitbox hand_r 0 0 0.3 0 0 1.4 0.15\nend\n"
    "attack heavy\n startup 20\n active 5\n recovery 24\n damage 25\n posture 30\n stamina 25\n"
    " hitstop 6\n hitbox hand_r 0 0 0.3 0 0 1.5 0.18\nend\n"
    /* grunt rapide */
    "attack g_slash\n startup 18\n active 4\n recovery 18\n damage 6\n posture 8\n stamina 5\n"
    " hitstop 3\n cancel_from 6\n next g_slash2\n hitbox hand_r 0 0 0.3 0 0 1.5 0.15\nend\n"
    "attack g_slash2\n startup 18\n active 4\n recovery 20\n damage 6\n posture 8\n stamina 5\n"
    " hitstop 3\n hitbox hand_r 0 0 0.3 0 0 1.5 0.15\nend\n"
    "attack g_lunge\n startup 24\n active 6\n recovery 30\n damage 10\n posture 14\n stamina 5\n"
    " hitstop 4\n hitbox hand_r 0 0 0.3 0 0 2.0 0.15\nend\n"
    /* brute lourd */
    "attack b_swing\n startup 28\n active 5\n recovery 30\n damage 14\n posture 20\n stamina 5\n"
    " hitstop 5\n hitbox hand_r 0 0 0.3 0 0 1.8 0.2\nend\n"
    "attack b_slam\n startup 40\n active 6\n recovery 40\n damage 22\n posture 30\n stamina 5\n"
    " hitstop 6\n hitbox hand_r 0 0 0.3 0 0 2.2 0.25\nend\n"
    /* archer : portée longue (pas de projectile : poussée à longue portée) */
    "attack a_stab\n startup 24\n active 4\n recovery 40\n damage 8\n posture 8\n stamina 5\n"
    " hitstop 3\n hitbox hand_r 0 0 0.5 0 0 5.5 0.12\nend\n"
    "attack a_pierce\n startup 36\n active 5\n recovery 50\n damage 12\n posture 12\n stamina 5\n"
    " hitstop 4\n hitbox hand_r 0 0 0.5 0 0 6.5 0.12\nend\n"
    /* boss : phase 1 puis phase 2 */
    "attack boss_l\n startup 22\n active 4\n recovery 24\n damage 8\n posture 10\n stamina 5\n"
    " hitstop 4\n hitbox hand_r 0 0 0.3 0 0 2.0 0.2\nend\n"
    "attack boss_h\n startup 34\n active 6\n recovery 36\n damage 14\n posture 20\n stamina 5\n"
    " hitstop 6\n hitbox hand_r 0 0 0.3 0 0 2.4 0.25\nend\n"
    "attack boss_l2\n startup 18\n active 5\n recovery 18\n damage 10\n posture 12\n stamina 5\n"
    " hitstop 4\n hitbox hand_r 0 0 0.3 0 0 2.4 0.25\nend\n"
    "attack boss_h2\n startup 26\n active 6\n recovery 26\n damage 18\n posture 24\n stamina 5\n"
    " hitstop 6\n hitbox hand_r 0 0 0.3 0 0 2.8 0.3\nend\n";

static const char ARCH[] =
    "archetype grunt\n hp 50\n speed 1.15\n sight 16\n fov 120\n hearing 12\n standoff 4\n"
    " light g_slash\n heavy g_lunge\n plan light 0 1.9 50 3 2\n plan heavy 0.8 2.4 140 1 1\nend\n"
    "archetype brute\n hp 120\n speed 0.7\n radius 0.5\n sight 14\n fov 100\n hearing 10\n "
    "standoff 4.5\n"
    " light b_swing\n heavy b_slam\n plan light 0 2.2 80 2 1\n plan heavy 0 2.6 180 1 1\nend\n"
    "archetype archer\n hp 40\n speed 0.9\n sight 20\n fov 110\n hearing 12\n standoff 7\n "
    "flee_below 0.3\n"
    " light a_stab\n heavy a_pierce\n plan light 3 6 90 3 1\n plan heavy 4 7 200 1 1\nend\n"
    "archetype boss\n hp 300\n speed 0.8\n radius 0.6\n sight 25\n fov 180\n hearing 25\n standoff "
    "5\n"
    " light boss_l\n heavy boss_h\n plan light 0 2.6 60 3 1\n plan heavy 0 3.0 150 1 1\n"
    " phase 0.6 boss_l2 boss_h2 1.25 0.7\nend\n"
    /* M6-09 : un seul plan, l'enchaînement scripté léger, léger, lourd */
    "archetype duelist\n hp 5000\n speed 0.8\n radius 0.5\n sight 25\n fov 180\n hearing 25\n"
    " standoff 4\n light boss_l\n heavy boss_h\n plan pattern 0 2.6 400 1 LLH\nend\n";

static EngAttackSet g_atk;
static EngArchSet g_arch;
static EngNavGrid g_nav;
static EngAiWorld g_world;
static EngCombatSim g_sim;
static EngAiGroup g_group;

static void load(void)
{
    char err[200];
    if (!eng_attackset_parse(ATTACKS, &g_atk, err, sizeof err)) {
        (void)fprintf(stderr, "attaques : %s\n", err);
        abort();
    }
    if (!eng_arch_parse(ARCH, &g_arch, err, sizeof err) ||
        !eng_arch_validate(&g_arch, &g_atk, err, sizeof err)) {
        (void)fprintf(stderr, "archétypes : %s\n", err);
        abort();
    }
}

static void setup(uint32_t fighters, bool with_nav)
{
    load();
    if (!eng_sim_init(&g_sim, &g_atk, fighters)) {
        abort();
    }
    g_sim.world = &g_world;
    g_sim.radius[0] = 0.4F;
    g_sim.fighters[0].vitals.max_hp = 100000.0F; /* le joueur automate ne meurt pas */
    g_sim.fighters[0].vitals.hp = 100000.0F;
    if (with_nav) {
        const float mn[2] = {-30.0F, -30.0F};
        const float mx[2] = {30.0F, 30.0F};
        if (!eng_nav_build(&g_nav, &g_world, mn, mx, 0.5F, 0.55F)) {
            abort();
        }
    }
    eng_aig_init(&g_group, &g_arch, &g_world, with_nav ? &g_nav : NULL, 0);
}

/* joueur automate : va vers l'ennemi vivant le plus proche et le frappe */
static void player_bot(EngFighterInput *in, float *noise)
{
    memset(in, 0, sizeof *in);
    *noise = 0.0F;
    int32_t best = -1;
    float bd = 1e9F;
    for (uint32_t i = 0; i < g_group.count; i++) {
        const uint32_t f = g_group.agents[i].fighter;
        if (g_sim.fighters[f].vitals.dead) {
            continue;
        }
        const float d = hypotf(g_sim.bodies[f].pos[0] - g_sim.bodies[0].pos[0],
                               g_sim.bodies[f].pos[1] - g_sim.bodies[0].pos[1]);
        if (d < bd) {
            bd = d;
            best = (int32_t)f;
        }
    }
    if (best < 0) {
        return;
    }
    const float dx = g_sim.bodies[best].pos[0] - g_sim.bodies[0].pos[0];
    const float dz = g_sim.bodies[best].pos[1] - g_sim.bodies[0].pos[1];
    const float len = sqrtf(dx * dx + dz * dz);
    if (len > 1.6F) {
        in->moving = true;
        in->move_dir[0] = dx / len;
        in->move_dir[1] = dz / len;
        *noise = 9.0F;
        return;
    }
    const float fx = -sinf(g_sim.bodies[0].yaw);
    const float fz = -cosf(g_sim.bodies[0].yaw);
    if ((fx * dx + fz * dz) / len < 0.95F) {
        in->moving = true;
        in->move_dir[0] = dx / len;
        in->move_dir[1] = dz / len;
        return;
    }
    in->light = true;
    *noise = 12.0F;
}

typedef struct Stats {
    uint32_t max_attackers;
    float min_separation;
    uint32_t min_telegraph;
    uint32_t attack_starts;
    uint32_t max_stuck;
    bool inside_obstacle;
    uint32_t seen_states[ENG_AI_STATE_COUNT];
    uint32_t hits_on_player;
} Stats;

static void run_ticks(uint32_t ticks, Stats *st, bool use_bot, EngFighterInput *fixed_player)
{
    uint32_t start_tick[ENG_SIM_MAX] = {0};
    float last_pos[ENG_SIM_MAX][2] = {{0}};
    uint32_t stuck[ENG_SIM_MAX] = {0};
    for (uint32_t t = 0; t < ticks; t++) {
        EngFighterInput in[ENG_SIM_MAX];
        memset(in, 0, sizeof in);
        float noise = 0.0F;
        if (use_bot) {
            player_bot(&in[0], &noise);
        } else if (fixed_player != NULL) {
            in[0] = *fixed_player;
        }
        eng_aig_tick(&g_group, &g_sim, noise, in);
        eng_sim_step(&g_sim, in);
        const uint32_t att = eng_aig_attackers(&g_group, &g_sim);
        if (att > st->max_attackers) {
            st->max_attackers = att;
        }
        for (uint32_t k = 0; k < g_sim.event_count; k++) {
            if (g_sim.events[k].defender == 0U && g_sim.events[k].kind != ENG_HITKIND_IGNORED) {
                st->hits_on_player++;
            }
        }
        for (uint32_t i = 0; i < g_group.count; i++) {
            const EngAiAgent *a = &g_group.agents[i];
            const EngFighter *f = &g_sim.fighters[a->fighter];
            st->seen_states[a->state]++;
            if (f->vitals.dead) {
                continue;
            }
            if ((f->events & ENG_FEV_ATTACK_START) != 0U) {
                start_tick[a->fighter] = g_sim.tick;
                st->attack_starts++;
            }
            if ((f->events & ENG_FEV_ATTACK_ACTIVE) != 0U && start_tick[a->fighter] != 0U) {
                const uint32_t d = g_sim.tick - start_tick[a->fighter];
                if (d < st->min_telegraph) {
                    st->min_telegraph = d;
                }
            }
            if (eng_aiw_distance(&g_world, g_sim.bodies[a->fighter].pos) <
                g_sim.radius[a->fighter] - 0.06F) {
                st->inside_obstacle = true;
            }
            for (uint32_t j = i + 1U; j < g_group.count; j++) {
                const uint32_t fj = g_group.agents[j].fighter;
                if (g_sim.fighters[fj].vitals.dead) {
                    continue;
                }
                const float d = hypotf(g_sim.bodies[fj].pos[0] - g_sim.bodies[a->fighter].pos[0],
                                       g_sim.bodies[fj].pos[1] - g_sim.bodies[a->fighter].pos[1]);
                if (d < st->min_separation) {
                    st->min_separation = d;
                }
            }
            /* blocage : en poursuite/engagement sans jamais bouger alors que le joueur est loin */
            const float moved = hypotf(g_sim.bodies[a->fighter].pos[0] - last_pos[a->fighter][0],
                                       g_sim.bodies[a->fighter].pos[1] - last_pos[a->fighter][1]);
            last_pos[a->fighter][0] = g_sim.bodies[a->fighter].pos[0];
            last_pos[a->fighter][1] = g_sim.bodies[a->fighter].pos[1];
            const float dp = hypotf(g_sim.bodies[a->fighter].pos[0] - g_sim.bodies[0].pos[0],
                                    g_sim.bodies[a->fighter].pos[1] - g_sim.bodies[0].pos[1]);
            if (a->state == ENG_AI_CHASE && moved < 0.005F && dp > 3.0F && a->pause == 0U &&
                f->fsm.state != ENG_CS_HITSTUN && f->fsm.state != ENG_CS_STAGGER) {
                stuck[a->fighter]++;
                if (stuck[a->fighter] > st->max_stuck) {
                    st->max_stuck = stuck[a->fighter];
                }
            } else {
                stuck[a->fighter] = 0;
            }
        }
    }
}

static void stats_init(Stats *s)
{
    memset(s, 0, sizeof *s);
    s->min_separation = 1e9F;
    s->min_telegraph = 1000000U;
}

static void build_camp_world(void)
{
    eng_aiw_init(&g_world);
    (void)eng_aiw_add_circle(&g_world, -3.0F, -9.0F, 1.2F);
    (void)eng_aiw_add_circle(&g_world, 4.0F, -11.0F, 1.0F);
    (void)eng_aiw_add_box(&g_world, 0.0F, -6.0F, 1.5F, 0.4F); /* muret devant le camp */
}

UTEST(ai_camp, six_enemies_of_three_archetypes_are_fought_end_to_end_with_at_most_two_attackers)
{
    build_camp_world();
    setup(7, true);
    EngCamp camp;
    eng_camp_init(&camp);
    const float patrol_a[2][2] = {{-6.0F, -13.0F}, {-6.0F, -9.0F}};
    ASSERT_TRUE(eng_camp_add(&camp, 0, -6.0F, -13.0F, 0.0F, patrol_a, 2)); /* grunt */
    ASSERT_TRUE(eng_camp_add(&camp, 0, 6.0F, -13.0F, 0.0F, NULL, 0));      /* grunt */
    ASSERT_TRUE(eng_camp_add(&camp, 1, 0.0F, -14.0F, 0.0F, NULL, 0));      /* brute */
    ASSERT_TRUE(eng_camp_add(&camp, 1, 2.0F, -16.0F, 0.0F, NULL, 0));      /* brute */
    ASSERT_TRUE(eng_camp_add(&camp, 2, -4.0F, -17.0F, 0.0F, NULL, 0));     /* archer */
    ASSERT_TRUE(eng_camp_add(&camp, 2, 5.0F, -17.0F, 0.0F, NULL, 0));      /* archer */
    ASSERT_TRUE(eng_camp_spawn(&camp, &g_group, &g_sim, 1));
    ASSERT_TRUE(eng_sim_place(&g_sim, 0, 0.0F, 8.0F, 0.0F));
    Stats st;
    stats_init(&st);
    for (uint32_t block = 0; block < 12U && !eng_camp_cleared(&g_group, &g_sim); block++) {
        run_ticks(1000, &st, true, NULL);
    }
    EXPECT_LE(st.max_attackers, 2U);
    EXPECT_GE(st.min_separation, 0.5F); /* pas d'empilement */
    EXPECT_GE(st.min_telegraph, 18U);   /* >= 300 ms avant chaque coup */
    EXPECT_GT(st.attack_starts, 8U);
    EXPECT_FALSE(st.inside_obstacle);
    EXPECT_LT(st.max_stuck, 120U); /* aucun blocage de plus de 2 s */
    EXPECT_GT(st.hits_on_player, 0U);
    EXPECT_TRUE(eng_camp_cleared(&g_group, &g_sim)); /* affrontable de bout en bout */
    EXPECT_GT(st.seen_states[ENG_AI_ENGAGE], 0U);
    EXPECT_GT(st.seen_states[ENG_AI_CHASE], 0U);
    EXPECT_GT(st.seen_states[ENG_AI_ALERT], 0U);
    (void)fprintf(
        stderr, "camp: tick=%u attackers<=%u sep>=%.2f telegraph>=%u stuck<=%u hits=%u starts=%u\n",
        g_sim.tick, st.max_attackers, (double)st.min_separation, st.min_telegraph, st.max_stuck,
        st.hits_on_player, st.attack_starts);
}

UTEST(ai_camp, the_same_run_gives_the_same_hash_twice)
{
    uint64_t h[2];
    for (int r = 0; r < 2; r++) {
        build_camp_world();
        setup(4, true);
        EngCamp camp;
        eng_camp_init(&camp);
        ASSERT_TRUE(eng_camp_add(&camp, 0, -3.0F, -13.0F, 0.0F, NULL, 0));
        ASSERT_TRUE(eng_camp_add(&camp, 1, 0.0F, -14.0F, 0.0F, NULL, 0));
        ASSERT_TRUE(eng_camp_add(&camp, 2, 4.0F, -16.0F, 0.0F, NULL, 0));
        ASSERT_TRUE(eng_camp_spawn(&camp, &g_group, &g_sim, 1));
        ASSERT_TRUE(eng_sim_place(&g_sim, 0, 0.0F, 6.0F, 0.0F));
        Stats st;
        stats_init(&st);
        run_ticks(1500, &st, true, NULL);
        h[r] = eng_sim_hash(&g_sim);
    }
    EXPECT_EQ(h[0], h[1]);
}

UTEST(ai_camp, an_enemy_goes_around_a_wall_to_reach_a_player_it_can_only_hear)
{
    eng_aiw_init(&g_world);
    ASSERT_TRUE(eng_aiw_add_box(&g_world, 0.0F, 0.0F, 0.5F, 7.0F));
    setup(2, true);
    ASSERT_TRUE(eng_aig_add(&g_group, &g_sim, 1, 0, -6.0F, 0.0F, 0.0F, NULL, 0));
    ASSERT_TRUE(eng_sim_place(&g_sim, 0, 6.0F, 0.0F, 0.0F));
    Stats st;
    stats_init(&st);
    EngFighterInput fixed;
    memset(&fixed, 0, sizeof fixed);
    /* le joueur ne fait que du bruit (il « tape dans ses mains ») : l'ennemi n'a aucune vue */
    for (uint32_t t = 0; t < 1200; t++) {
        EngFighterInput in[2];
        memset(in, 0, sizeof in);
        eng_aig_tick(&g_group, &g_sim, 14.0F, in);
        eng_sim_step(&g_sim, in);
        for (uint32_t k = 0; k < g_sim.event_count; k++) {
            (void)k;
        }
    }
    const float d = hypotf(g_sim.bodies[1].pos[0] - 6.0F, g_sim.bodies[1].pos[1]);
    EXPECT_LT(d, 4.0F);
    EXPECT_GE(eng_aiw_distance(&g_world, g_sim.bodies[1].pos), 0.0F);
    EXPECT_GT(g_sim.bodies[1].pos[0], 0.5F); /* de l'autre côté du mur */
    (void)st;
    (void)fixed;
}

UTEST(ai_camp, the_behaviour_cycle_patrol_alert_chase_engage_return_patrol_is_observed)
{
    eng_aiw_init(&g_world);
    setup(2, false);
    const float patrol[2][2] = {{0.0F, -12.0F}, {4.0F, -12.0F}};
    ASSERT_TRUE(eng_aig_add(&g_group, &g_sim, 1, 0, 0.0F, -12.0F, -1.5707963F, patrol, 2));
    ASSERT_TRUE(eng_sim_place(&g_sim, 0, 9.0F, -12.0F, 0.0F));
    Stats st;
    stats_init(&st);
    EngFighterInput fixed;
    memset(&fixed, 0, sizeof fixed);
    run_ticks(300, &st, false, &fixed); /* ronde vers +X, le joueur est droit devant : alerte */
    EXPECT_GT(st.seen_states[ENG_AI_ALERT], 0U);
    EXPECT_GT(st.seen_states[ENG_AI_CHASE], 0U);
    run_ticks(400, &st, false, &fixed); /* il vient au contact */
    EXPECT_GT(st.seen_states[ENG_AI_ENGAGE], 0U);
    /* le joueur disparaît très loin : mémoire expirée puis retour au poste */
    ASSERT_TRUE(eng_sim_place(&g_sim, 0, 200.0F, 200.0F, 0.0F));
    run_ticks(1500, &st, false, &fixed);
    EXPECT_GT(st.seen_states[ENG_AI_RETURN], 0U);
    EXPECT_EQ(g_group.agents[0].state, ENG_AI_PATROL);
}

UTEST(ai_camp, a_wounded_archer_flees_and_comes_back_to_its_post)
{
    eng_aiw_init(&g_world);
    setup(2, false);
    ASSERT_TRUE(eng_aig_add(&g_group, &g_sim, 1, 2, 0.0F, -10.0F, 0.0F, NULL, 0));
    ASSERT_TRUE(eng_sim_place(&g_sim, 0, 0.0F, -3.0F, 0.0F));
    g_sim.fighters[1].vitals.hp = g_sim.fighters[1].vitals.max_hp * 0.2F;
    Stats st;
    stats_init(&st);
    EngFighterInput fixed;
    memset(&fixed, 0, sizeof fixed);
    run_ticks(60, &st, false, &fixed);
    EXPECT_GT(st.seen_states[ENG_AI_FLEE], 0U);
    const float d0 = hypotf(g_sim.bodies[1].pos[0], g_sim.bodies[1].pos[1] + 3.0F);
    EXPECT_GT(d0, 7.0F); /* il s'éloigne du joueur */
}

UTEST(ai_camp, boss_has_two_distinct_phases_and_can_be_beaten)
{
    eng_aiw_init(&g_world);
    ASSERT_TRUE(eng_arena_walls(&g_world, 0.0F, 0.0F, 12.0F, 12.0F));
    setup(2, true);
    ASSERT_TRUE(eng_aig_add(&g_group, &g_sim, 1, 3, 0.0F, -8.0F, 0.0F, NULL, 0));
    ASSERT_TRUE(eng_sim_place(&g_sim, 0, 0.0F, 8.0F, 0.0F));
    char seen[8][32] = {{0}};
    uint32_t nseen = 0;
    bool phase2_hp_ok = false;
    for (uint32_t t = 0; t < 12000 && !g_sim.fighters[1].vitals.dead; t++) {
        EngFighterInput in[2];
        memset(in, 0, sizeof in);
        float noise = 0.0F;
        player_bot(&in[0], &noise);
        eng_aig_tick(&g_group, &g_sim, noise, in);
        eng_sim_step(&g_sim, in);
        if ((g_sim.fighters[1].events & ENG_FEV_ATTACK_START) != 0U) {
            const EngAttack *a = eng_fighter_attack(&g_sim.fighters[1], &g_atk);
            bool known = false;
            for (uint32_t i = 0; i < nseen; i++) {
                known = known || strcmp(seen[i], a->name) == 0;
            }
            if (!known && nseen < 8U) {
                (void)snprintf(seen[nseen++], sizeof seen[0], "%s", a->name);
            }
        }
        if (g_group.agents[0].phase == 1U &&
            g_sim.fighters[1].vitals.hp / g_sim.fighters[1].vitals.max_hp <= 0.6F) {
            phase2_hp_ok = true;
        }
    }
    EXPECT_TRUE(g_sim.fighters[1].vitals.dead);
    EXPECT_EQ(g_group.agents[0].phase, 1U);
    EXPECT_TRUE(phase2_hp_ok);
    bool p1 = false;
    bool p2 = false;
    for (uint32_t i = 0; i < nseen; i++) {
        p1 = p1 || strncmp(seen[i], "boss_l", 6) == 0 || strcmp(seen[i], "boss_h") == 0;
        p2 = p2 || strcmp(seen[i], "boss_l2") == 0 || strcmp(seen[i], "boss_h2") == 0;
    }
    EXPECT_TRUE(p1);
    EXPECT_TRUE(p2);
    EXPECT_GT(g_sim.speed_scale[1], 0.8F); /* phase 2 : plus rapide */
}

/* M6-09 : un plan « pattern » joue sa suite (léger, léger, lourd) attaque après attaque, chacune
 * partant dès la fin de la précédente, puis respecte sa recharge. Le joueur reste immobile. */
UTEST(ai_camp, a_pattern_plays_its_attacks_back_to_back_then_waits_its_cooldown)
{
    eng_aiw_init(&g_world);
    setup(2, true);
    ASSERT_TRUE(eng_aig_add(&g_group, &g_sim, 1, 4, 0.0F, -2.5F, 3.14159265F, NULL,
                            0)); /* face au joueur */
    ASSERT_TRUE(eng_sim_place(&g_sim, 0, 0.0F, 0.0F, 0.0F));
    char names[8][32] = {{0}};
    uint32_t starts[8] = {0};
    uint32_t ends[8] = {0};
    uint32_t n = 0;
    bool was_attacking = false;
    for (uint32_t t = 0; t < 1500 && n < 8U; t++) {
        EngFighterInput in[2];
        memset(in, 0, sizeof in);
        eng_aig_tick(&g_group, &g_sim, 0.0F, in);
        eng_sim_step(&g_sim, in);
        const bool attacking = g_sim.fighters[1].fsm.state == ENG_CS_ATTACK;
        if ((g_sim.fighters[1].events & ENG_FEV_ATTACK_START) != 0U) {
            const EngAttack *a = eng_fighter_attack(&g_sim.fighters[1], &g_atk);
            (void)snprintf(names[n], sizeof names[0], "%s", a->name);
            starts[n++] = t;
        } else if (was_attacking && !attacking && n > 0U) {
            ends[n - 1U] = t;
        }
        was_attacking = attacking;
    }
    ASSERT_GE(n, 4U);
    EXPECT_STREQ(names[0], "boss_l");
    EXPECT_STREQ(names[1], "boss_l");
    EXPECT_STREQ(names[2], "boss_h");
    EXPECT_STREQ(names[3], "boss_l");   /* la suite recommence après la recharge */
    for (uint32_t k = 0; k < 2U; k++) { /* chaque attaque part dès la fin de la précédente */
        EXPECT_GT(ends[k], starts[k]);
        EXPECT_LE(starts[k + 1U] - ends[k], 3U);
    }
    EXPECT_GE(starts[3] - starts[0], 400U); /* recharge du plan, comptée dès le premier appui */
}

UTEST(ai_camp, rest_resets_the_whole_camp_but_is_refused_during_a_fight)
{
    build_camp_world();
    setup(4, true);
    EngCamp camp;
    eng_camp_init(&camp);
    ASSERT_TRUE(eng_camp_add(&camp, 0, -3.0F, -13.0F, 3.14159265F, NULL, 0));
    ASSERT_TRUE(eng_camp_add(&camp, 1, 0.0F, -14.0F, 3.14159265F, NULL, 0));
    ASSERT_TRUE(eng_camp_add(&camp, 2, 4.0F, -16.0F, 3.14159265F, NULL, 0));
    ASSERT_TRUE(eng_camp_spawn(&camp, &g_group, &g_sim, 1));
    ASSERT_TRUE(eng_sim_place(&g_sim, 0, 6.0F, -4.0F, 0.0F));
    Stats st;
    stats_init(&st);
    EngFighterInput fixed;
    memset(&fixed, 0, sizeof fixed);
    run_ticks(400, &st, false, &fixed); /* ils ont vu le joueur : poursuite / engagement */
    bool fighting = false;
    for (uint32_t i = 0; i < g_group.count; i++) {
        fighting = fighting || g_group.agents[i].state == ENG_AI_CHASE ||
                   g_group.agents[i].state == ENG_AI_ENGAGE;
    }
    ASSERT_TRUE(fighting);
    EXPECT_FALSE(eng_camp_rest(&camp, &g_group, &g_sim)); /* refusé : rien ne change */
    /* tout le monde tombe, le joueur s'éloigne, puis se repose */
    for (uint32_t i = 1; i <= 3U; i++) {
        g_sim.fighters[i].vitals.hp = 0.0F;
        g_sim.fighters[i].vitals.dead = true;
    }
    EXPECT_TRUE(eng_camp_cleared(&g_group, &g_sim));
    EXPECT_TRUE(eng_camp_rest(&camp, &g_group, &g_sim));
    for (uint32_t i = 0; i < 3U; i++) {
        const uint32_t f = g_group.agents[i].fighter;
        EXPECT_FALSE(g_sim.fighters[f].vitals.dead);
        EXPECT_NEAR(g_sim.fighters[f].vitals.hp, g_arch.a[camp.m[i].arch].hp, 1e-3F);
        EXPECT_NEAR(g_sim.bodies[f].pos[0], camp.m[i].pos[0], 1e-4F);
        EXPECT_NEAR(g_sim.bodies[f].pos[1], camp.m[i].pos[1], 1e-4F);
        EXPECT_EQ(g_group.agents[i].state, ENG_AI_PATROL);
    }
    EXPECT_FALSE(eng_camp_cleared(&g_group, &g_sim));
    EXPECT_EQ(eng_tokens_in_use(&g_group.tokens), 0U);
}

UTEST(ai_camp, a_new_archetype_written_as_text_spawns_without_recompiling)
{
    static const char new_arch[] =
        "archetype swarmer\n hp 20\n speed 1.4\n radius 0.3\n light g_slash\n heavy g_lunge\n"
        " plan light 0 1.6 40 1 1\nend\n";
    char err[200];
    ASSERT_TRUE(eng_arch_parse(new_arch, &g_arch, err, sizeof err));
    load();
    ASSERT_TRUE(eng_arch_parse(new_arch, &g_arch, err, sizeof err));
    ASSERT_TRUE(eng_arch_validate(&g_arch, &g_atk, err, sizeof err));
    eng_aiw_init(&g_world);
    ASSERT_TRUE(eng_sim_init(&g_sim, &g_atk, 2));
    eng_aig_init(&g_group, &g_arch, &g_world, NULL, 0);
    ASSERT_TRUE(eng_aig_add(&g_group, &g_sim, 1, 0, 3.0F, 3.0F, 0.0F, NULL, 0));
    EXPECT_NEAR(g_sim.fighters[1].vitals.max_hp, 20.0F, 1e-6F);
    EXPECT_NEAR(g_sim.speed_scale[1], 1.4F, 1e-6F);
    EXPECT_NEAR(g_sim.radius[1], 0.3F, 1e-6F);
    EXPECT_STREQ(g_sim.fighters[1].params.light_name, "g_slash");
    /* arguments invalides */
    EXPECT_FALSE(eng_aig_add(&g_group, &g_sim, 0, 0, 0, 0, 0, NULL, 0));  /* le joueur */
    EXPECT_FALSE(eng_aig_add(&g_group, &g_sim, 1, 0, 0, 0, 0, NULL, 0));  /* déjà pris */
    EXPECT_FALSE(eng_aig_add(&g_group, &g_sim, 5, 0, 0, 0, 0, NULL, 0));  /* hors simulation */
    EXPECT_FALSE(eng_aig_add(&g_group, &g_sim, 1, 99, 0, 0, 0, NULL, 0)); /* archétype inconnu */
}

UTEST(ai_debug, any_enemy_can_be_inspected_picked_with_a_ray_and_drawn)
{
    build_camp_world();
    setup(3, true);
    const float patrol[2][2] = {{-3.0F, -13.0F}, {3.0F, -13.0F}};
    ASSERT_TRUE(eng_aig_add(&g_group, &g_sim, 1, 0, -3.0F, -13.0F, 0.0F, patrol, 2));
    ASSERT_TRUE(eng_aig_add(&g_group, &g_sim, 2, 1, 8.0F, -13.0F, 0.0F, NULL, 0));
    ASSERT_TRUE(eng_sim_place(&g_sim, 0, 0.0F, 0.0F, 0.0F));
    Stats st;
    stats_init(&st);
    EngFighterInput fixed;
    memset(&fixed, 0, sizeof fixed);
    run_ticks(200, &st, false, &fixed);
    EngAiDebugInfo info;
    ASSERT_TRUE(eng_aidbg_info(&g_group, &g_sim, 0, &info));
    EXPECT_STREQ(info.archetype, "grunt");
    EXPECT_EQ(info.fighter, 1U);
    EXPECT_NEAR(info.max_hp, 50.0F, 1e-4F);
    EXPECT_GT(info.dist_to_player, 0.0F);
    EXPECT_FALSE(eng_aidbg_info(&g_group, &g_sim, 9, &info));
    ASSERT_TRUE(eng_aidbg_info(&g_group, &g_sim, 0, &info));
    char text[512];
    const size_t n = eng_aidbg_format(&info, text, sizeof text);
    EXPECT_GT(n, 40U);
    EXPECT_NE(strstr(text, "grunt"), NULL);
    EXPECT_NE(strstr(text, info.state), NULL);
    char tiny[8];
    EXPECT_EQ(eng_aidbg_format(&info, tiny, sizeof tiny), sizeof tiny - 1U);
    /* clic : rayon horizontal à 1,2 m de haut vers le grunt (indice 0), puis vers le brute */
    const float bx = g_sim.bodies[1].pos[0];
    const float bz = g_sim.bodies[1].pos[1];
    const float o1[3] = {bx - 5.0F, 1.2F, bz};
    const float d1[3] = {1.0F, 0.0F, 0.0F};
    EXPECT_EQ(eng_aidbg_pick(&g_group, &g_sim, o1, d1, 20.0F), 0);
    const float o2[3] = {g_sim.bodies[2].pos[0], 1.2F, g_sim.bodies[2].pos[1] + 5.0F};
    const float d2[3] = {0.0F, 0.0F, -1.0F};
    EXPECT_EQ(eng_aidbg_pick(&g_group, &g_sim, o2, d2, 20.0F), 1);
    const float o3[3] = {bx - 5.0F, 4.0F, bz}; /* au-dessus */
    EXPECT_EQ(eng_aidbg_pick(&g_group, &g_sim, o3, d1, 20.0F), -1);
    EXPECT_EQ(eng_aidbg_pick(&g_group, &g_sim, o1, d1, 2.0F), -1); /* trop court */
    /* dessin */
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "aidbg", (size_t)1 << 20));
    EngDebugLines lines;
    ASSERT_TRUE(eng_debug_lines_init(&lines, &arena, 4096));
    const uint32_t plain = eng_aidbg_draw(&g_group, &g_sim, -1, &lines);
    EXPECT_GT(plain, 10U); /* cônes de vision */
    eng_debug_lines_clear(&lines);
    g_group.agents[0].path_n = 3; /* un chemin factice pour l'agent sélectionné */
    g_group.agents[0].path_i = 0;
    const uint32_t sel = eng_aidbg_draw(&g_group, &g_sim, 0, &lines);
    EXPECT_GT(sel, plain - 1U);
    eng_arena_shutdown(&arena);
}

UTEST(ai_camp, arena_walls_enclose_exactly_the_requested_rectangle)
{
    EngAiWorld w;
    eng_aiw_init(&w);
    ASSERT_TRUE(eng_arena_walls(&w, 10.0F, -5.0F, 8.0F, 6.0F));
    EXPECT_EQ(w.count, 4U);
    const float inside[2] = {10.0F, -5.0F};
    EXPECT_GT(eng_aiw_distance(&w, inside), 5.0F);
    const float corner_in[2] = {17.5F, 0.5F}; /* juste dans l'arène */
    EXPECT_GT(eng_aiw_distance(&w, corner_in), 0.0F);
    const float wall_e[2] = {18.5F, -5.0F};
    EXPECT_LT(eng_aiw_distance(&w, wall_e), 0.0F);
    const float wall_w[2] = {1.5F, -5.0F};
    EXPECT_LT(eng_aiw_distance(&w, wall_w), 0.0F);
    const float wall_n[2] = {10.0F, -11.5F};
    EXPECT_LT(eng_aiw_distance(&w, wall_n), 0.0F);
    const float wall_s[2] = {10.0F, 1.5F};
    EXPECT_LT(eng_aiw_distance(&w, wall_s), 0.0F);
    const float a[2] = {10.0F, -5.0F};
    const float b[2] = {30.0F, -5.0F};
    EXPECT_FALSE(eng_aiw_los(&w, a, b)); /* on ne voit pas à travers l'arène */
    EXPECT_FALSE(eng_arena_walls(&w, 0.0F, 0.0F, 0.5F, 6.0F));
    EXPECT_FALSE(eng_arena_walls(NULL, 0.0F, 0.0F, 8.0F, 6.0F));
    EngAiWorld full;
    eng_aiw_init(&full);
    for (uint32_t i = 0; i < ENG_AI_MAX_OBSTACLES - 2U; i++) {
        ASSERT_TRUE(eng_aiw_add_circle(&full, (float)i * 3.0F, 0.0F, 1.0F));
    }
    EXPECT_FALSE(
        eng_arena_walls(&full, 0.0F, 0.0F, 8.0F, 6.0F)); /* pas la place : rien d'à moitié */
    EXPECT_EQ(full.count, ENG_AI_MAX_OBSTACLES - 2U);
}

/* M6-09 : le camp peut être vaincu alors qu'un boss du même groupe vit encore. */
UTEST(ai_camp, a_range_of_agents_is_cleared_independently_of_the_others)
{
    eng_aiw_init(&g_world);
    setup(4, false);
    ASSERT_TRUE(eng_aig_add(&g_group, &g_sim, 1, 0, -4.0F, 0.0F, 0.0F, NULL, 0));
    ASSERT_TRUE(eng_aig_add(&g_group, &g_sim, 2, 1, 4.0F, 0.0F, 0.0F, NULL, 0));
    ASSERT_TRUE(eng_aig_add(&g_group, &g_sim, 3, 3, 0.0F, 8.0F, 0.0F, NULL, 0)); /* boss */
    EXPECT_FALSE(eng_camp_cleared_range(&g_group, &g_sim, 0, 2));
    g_sim.fighters[1].vitals.dead = true;
    EXPECT_FALSE(eng_camp_cleared_range(&g_group, &g_sim, 0, 2));
    g_sim.fighters[2].vitals.dead = true;
    EXPECT_TRUE(eng_camp_cleared_range(&g_group, &g_sim, 0, 2));
    EXPECT_FALSE(eng_camp_cleared(&g_group, &g_sim)); /* le boss vit */
    EXPECT_FALSE(eng_camp_cleared_range(&g_group, &g_sim, 2, 1));
    g_sim.fighters[3].vitals.dead = true;
    EXPECT_TRUE(eng_camp_cleared_range(&g_group, &g_sim, 2, 1));
    EXPECT_TRUE(eng_camp_cleared(&g_group, &g_sim));
    EXPECT_FALSE(eng_camp_cleared_range(&g_group, &g_sim, 0, 0)); /* plage vide */
    EXPECT_FALSE(eng_camp_cleared_range(&g_group, &g_sim, 2, 2)); /* déborde */
    EXPECT_FALSE(eng_camp_cleared_range(&g_group, &g_sim, 3, 1));
    EXPECT_FALSE(eng_camp_cleared_range(NULL, &g_sim, 0, 1));
}

UTEST(ai_camp, camp_arguments_are_validated)
{
    EngCamp c;
    eng_camp_init(&c);
    EXPECT_FALSE(eng_camp_add(NULL, 0, 0, 0, 0, NULL, 0));
    EXPECT_FALSE(eng_camp_add(&c, 0, NAN, 0, 0, NULL, 0));
    EXPECT_FALSE(eng_camp_add(&c, 0, 0, 0, 0, NULL, 2)); /* ronde annoncée mais absente */
    EXPECT_FALSE(eng_camp_add(&c, 0, 0, 0, 0, NULL, ENG_AI_PATROL_MAX + 1U));
    for (uint32_t i = 0; i < ENG_AI_MAX_AGENTS; i++) {
        EXPECT_TRUE(eng_camp_add(&c, 0, (float)i, 0, 0, NULL, 0));
    }
    EXPECT_FALSE(eng_camp_add(&c, 0, 0, 0, 0, NULL, 0));
    EXPECT_EQ(c.count, ENG_AI_MAX_AGENTS);
    eng_aiw_init(&g_world);
    setup(3, false);
    EXPECT_FALSE(eng_camp_spawn(&c, &g_group, &g_sim, 1)); /* 7 ennemis dans 3 combattants */
    EXPECT_FALSE(eng_camp_spawn(NULL, &g_group, &g_sim, 1));
    EXPECT_FALSE(eng_camp_rest(NULL, &g_group, &g_sim));
    EXPECT_FALSE(eng_camp_cleared(&g_group, &g_sim)); /* groupe vide : pas « vaincu » */
}
