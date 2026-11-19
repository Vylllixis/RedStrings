/* Hurtboxes pilotées par un squelette animé dans la simulation de combat (M5-05). */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "engine/combat_sim.h"
#include "engine/hurtbox.h"
#include "engine/pose.h"
#include "utest.h"

#define H 0.70710678F

static EngJoint g_joints[3] = {
    {-1, {0, 0, 0}, {0, 0, 0, 1}, {0.01F, 0.01F, 0.01F}, {0}, "Armature"},
    {0, {0, 100, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "Hips"},
    {1, {30, 0, 0}, {0, 0, H, H}, {1, 1, 1}, {0}, "Hand"},
};

static const char SET[] =
    "attack light1\n startup 8\n active 4\n recovery 14\n damage 10\n posture 8\n stamina 10\n"
    " hitstop 3\n hitbox hand_r 0 0 0 0 0 0.6 0.12\nend\n";

typedef struct Fx {
    EngSkeleton sk;
    EngArena arena;
    EngPose pose;
    mat4 world[3];
    EngHurtboxSet set;
    EngAttackSet atk;
    EngCombatSim sim;
} Fx;

static void fx_init(Fx *f)
{
    char err[128];
    f->sk = (EngSkeleton){.joint_count = 3, .joints = g_joints};
    if (!eng_arena_init(&f->arena, "rig", (size_t)1 << 16) ||
        !eng_pose_init(&f->arena, &f->sk, &f->pose) ||
        !eng_attackset_parse(SET, &f->atk, err, sizeof err) || !eng_sim_init(&f->sim, &f->atk, 2) ||
        !eng_sim_place(&f->sim, 0, 0.0F, 0.0F, 0.0F) ||
        !eng_sim_place(&f->sim, 1, 0.0F, -3.0F, 0.0F)) {
        abort();
    }
    eng_hurtboxset_init(&f->set);
    const float a[3] = {0, 0, 0};
    const float b[3] = {0.4F, 0, 0};
    if (!eng_hurtboxset_add(&f->set, &f->sk, "main", ENG_ZONE_LIMB, "Hand", a, b, 0.07F)) {
        abort();
    }
}

/* pose courante -> capsules monde (modèle placé pour que la main soit en (0, 1..1.4, -0.5)) */
static void feed(Fx *f)
{
    mat4 model;
    glm_mat4_identity(model);
    const vec3 t = {-0.3F, 0.0F, -0.5F};
    glm_translate(model, (float *)t);
    if (!eng_pose_world(&f->pose, &f->sk, f->world)) {
        abort();
    }
    EngCapsule c;
    if (!eng_hurtboxset_world(&f->set, f->world, 3, model, &c)) {
        abort();
    }
    const EngHitZone z = ENG_ZONE_LIMB;
    if (!eng_sim_set_rig(&f->sim, 1, &c, &z, 1)) {
        abort();
    }
}

static uint32_t swing(Fx *f)
{
    EngFighterInput in[2];
    memset(in, 0, sizeof in);
    in[0].light = true;
    feed(f);
    eng_sim_step(&f->sim, in);
    in[0].light = false;
    for (int i = 0; i < 30; i++) {
        feed(f);
        eng_sim_step(&f->sim, in);
    }
    return f->sim.total_hits;
}

UTEST(combat_rig, a_swing_hits_the_rig_capsule_where_the_animation_put_it)
{
    Fx f;
    fx_init(&f);
    /* le corps simplifié est à 3 m : seule la capsule du squelette (à 0,5 m) peut être touchée */
    EXPECT_EQ(swing(&f), 1U);
    EXPECT_NEAR(f.sim.fighters[1].vitals.max_hp - f.sim.fighters[1].vitals.hp, 7.5F,
                0.01F); /* membre x0,75 */
    eng_arena_shutdown(&f.arena);
}

UTEST(combat_rig, the_same_swing_misses_once_the_animation_moves_the_limb_away)
{
    Fx f;
    fx_init(&f);
    f.pose.translation[1 * 3 + 0] = 500.0F; /* bassin 5 m sur le côté (cm sous l'Armature) */
    EXPECT_EQ(swing(&f), 0U);
    eng_arena_shutdown(&f.arena);
}

UTEST(combat_rig, the_capsule_is_followed_tick_by_tick_during_an_animation)
{
    Fx f;
    fx_init(&f);
    EngFighterInput in[2];
    memset(in, 0, sizeof in);
    in[0].light = true;
    uint32_t hit_tick = 0;
    for (int i = 0; i < 40; i++) {
        /* la main entre dans la zone de frappe au tick 9 : avant, elle est à 5 m */
        f.pose.translation[1 * 3 + 0] = (i + 1 < 9) ? 500.0F : 0.0F;
        feed(&f);
        eng_sim_step(&f.sim, in);
        in[0].light = false;
        if (f.sim.event_count > 0U && hit_tick == 0U) {
            hit_tick = f.sim.events[0].tick;
        }
    }
    EXPECT_EQ(f.sim.total_hits, 1U);
    EXPECT_GE(hit_tick, 9U);
    eng_arena_shutdown(&f.arena);
}

UTEST(combat_rig, set_rig_validates_and_zero_count_restores_the_simple_body)
{
    Fx f;
    fx_init(&f);
    EngCapsule c;
    memset(&c, 0, sizeof c);
    const EngHitZone z = ENG_ZONE_HEAD;
    EXPECT_FALSE(eng_sim_set_rig(NULL, 0, &c, &z, 1));
    EXPECT_FALSE(eng_sim_set_rig(&f.sim, 9, &c, &z, 1));
    EXPECT_FALSE(eng_sim_set_rig(&f.sim, 1, NULL, &z, 1));
    EXPECT_FALSE(eng_sim_set_rig(&f.sim, 1, &c, &z, ENG_HURTBOX_MAX + 1U));
    EXPECT_TRUE(eng_sim_set_rig(&f.sim, 1, &c, &z, 1));
    EXPECT_EQ(f.sim.rig[1].count, 1U);
    EXPECT_TRUE(eng_sim_set_rig(&f.sim, 1, NULL, NULL, 0));
    EXPECT_EQ(f.sim.rig[1].count, 0U);
    eng_arena_shutdown(&f.arena);
}

/* Arme animée (M5-06) : les hitboxes `hand_r` suivent la lame que donne l'appelant. La cible (corps
 * simplifié) est à 3 m : la hitbox du repère du corps (0,6 m) ne l'atteint pas ; une lame posée
 * contre son torse la touche ; sans lame, plus rien. Une hitbox d'un autre nom (carreau) n'est
 * jamais remplacée. */
static uint32_t swing_with_blade(bool blade, const char *set_text)
{
    EngAttackSet atk;
    char err[128];
    static EngCombatSim sim;
    if (!eng_attackset_parse(set_text, &atk, err, sizeof err) || !eng_sim_init(&sim, &atk, 2) ||
        !eng_sim_place(&sim, 0, 0.0F, 0.0F, 0.0F) || !eng_sim_place(&sim, 1, 0.0F, -3.0F, 0.0F)) {
        abort();
    }
    EngFighterInput in[2];
    memset(in, 0, sizeof in);
    in[0].light = true;
    /* lame verticale à 2,8 m devant, à hauteur du torse de la cible */
    const EngCapsule c = {{0.0F, 0.9F, -2.8F}, {0.0F, 1.5F, -2.8F}, 0.1F};
    for (int i = 0; i < 30; i++) {
        if (!eng_sim_set_weapon(&sim, 0, blade ? &c : NULL, blade ? 1U : 0U)) {
            abort();
        }
        eng_sim_step(&sim, in);
        in[0].light = false;
    }
    return sim.total_hits;
}

UTEST(combat_rig, hand_hitboxes_follow_the_animated_blade_when_it_is_given)
{
    EXPECT_EQ(swing_with_blade(false, SET), 0U); /* repère du corps : 0,6 m, trop court */
    EXPECT_EQ(swing_with_blade(true, SET), 1U);  /* la lame atteint la cible */
    static const char bolt[] =
        "attack light1\n startup 8\n active 4\n recovery 14\n damage 10\n posture 8\n"
        " stamina 10\n hitstop 3\n hitbox bolt 0 0 0 0 0 0.6 0.12\nend\n";
    EXPECT_EQ(swing_with_blade(true, bolt), 0U); /* carreau : jamais remplacé par la lame */
}

UTEST(combat_rig, set_weapon_validates_and_zero_count_returns_to_the_body_frame)
{
    static EngCombatSim sim;
    EngAttackSet atk;
    char err[128];
    ASSERT_TRUE(eng_attackset_parse(SET, &atk, err, sizeof err));
    ASSERT_TRUE(eng_sim_init(&sim, &atk, 2));
    const EngCapsule c[3] = {{{0, 0, 0}, {0, 1, 0}, 0.1F}};
    EXPECT_FALSE(eng_sim_set_weapon(NULL, 0, c, 1));
    EXPECT_FALSE(eng_sim_set_weapon(&sim, 5, c, 1));
    EXPECT_FALSE(eng_sim_set_weapon(&sim, 0, NULL, 1));
    EXPECT_FALSE(eng_sim_set_weapon(&sim, 0, c, ENG_SIM_WEAPON_MAX + 1U));
    EXPECT_TRUE(eng_sim_set_weapon(&sim, 0, c, 1));
    EXPECT_EQ(sim.weapon[0].count, 1U);
    EXPECT_TRUE(eng_sim_set_weapon(&sim, 0, NULL, 0));
    EXPECT_EQ(sim.weapon[0].count, 0U);
}
