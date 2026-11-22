/* IK à deux os (M4-23). Jambe : racine > cuisse (0,1,0) > genou (0,-0.6,0) > pied (0,-0.4,0), donc
 * os de 0,6 m et 0,4 m, jambe droite pendant vers le bas : A = (0,1,0), B = (0,0.4,0), C = 0. */
#include <math.h>
#include <string.h>

#include "engine/anim_ik.h"
#include "utest.h"

static EngJoint g_leg[4] = {
    {-1, {0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "root"},
    {0, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "thigh"},
    {1, {0, -0.6F, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "knee"},
    {2, {0, -0.4F, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "foot"},
};

/* même jambe en centimètres sous une racine à l'échelle 0,01 (import Mixamo) */
static EngJoint g_leg_cm[4] = {
    {-1, {0, 0, 0}, {0, 0, 0, 1}, {0.01F, 0.01F, 0.01F}, {0}, "Armature"},
    {0, {0, 100, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "thigh"},
    {1, {0, -60, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "knee"},
    {2, {0, -40, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "foot"},
};

typedef struct Fx {
    EngSkeleton sk;
    EngArena arena;
    EngPose pose;
    mat4 world[4];
} Fx;

static bool fx_init(Fx *f, EngJoint *joints)
{
    f->sk = (EngSkeleton){.joint_count = 4, .joints = joints};
    return eng_arena_init(&f->arena, "ik", (size_t)1 << 14) &&
           eng_pose_init(&f->arena, &f->sk, &f->pose) && eng_pose_world(&f->pose, &f->sk, f->world);
}

static bool refresh(Fx *f)
{
    return eng_pose_world(&f->pose, &f->sk, f->world);
}

static void pos(Fx *f, uint32_t j, vec3 out)
{
    out[0] = f->world[j][3][0];
    out[1] = f->world[j][3][1];
    out[2] = f->world[j][3][2];
}

static float dist(const vec3 a, const vec3 b)
{
    return glm_vec3_distance((float *)a, (float *)b);
}

/* chaîne OK : longueurs des os conservées (en mètres, mesurées dans le monde) */
static bool bones_keep_length(Fx *f, float l1, float l2)
{
    vec3 a;
    vec3 b;
    vec3 c;
    pos(f, 1, a);
    pos(f, 2, b);
    pos(f, 3, c);
    return fabsf(dist(a, b) - l1) < 1e-4F && fabsf(dist(b, c) - l2) < 1e-4F;
}

static bool same_rotations(const EngPose *a, const EngPose *b)
{
    for (uint32_t i = 0; i < 4U * a->joint_count; i++) {
        if (a->rotation[i] != b->rotation[i]) {
            return false;
        }
    }
    return true;
}

static bool solve_reaches(EngJoint *joints, const float target[3], const float pole[3], float l1,
                          float l2, vec3 knee)
{
    Fx f;
    glm_vec3_zero(knee);
    if (!fx_init(&f, joints) || !eng_ik_two_bone(&f.pose, &f.sk, 1, 2, 3, target, pole, 1.0F) ||
        !refresh(&f)) {
        return false;
    }
    vec3 c;
    pos(&f, 3, c);
    pos(&f, 2, knee);
    const bool ok = dist(c, (float *)target) < 1e-4F && bones_keep_length(&f, l1, l2);
    eng_arena_shutdown(&f.arena);
    return ok;
}

UTEST(anim_ik, the_foot_reaches_a_reachable_target_and_the_bones_keep_their_length)
{
    const float target[3] = {0.2F, 0.3F, 0.0F};
    const float pole_front[3] = {0.0F, 0.5F, -5.0F};
    const float pole_back[3] = {0.0F, 0.5F, 5.0F};
    vec3 knee_front;
    vec3 knee_back;
    EXPECT_TRUE(solve_reaches(g_leg, target, pole_front, 0.6F, 0.4F, knee_front));
    EXPECT_TRUE(solve_reaches(g_leg, target, pole_back, 0.6F, 0.4F, knee_back));
    EXPECT_LT(knee_front[2], -0.1F); /* le genou se plie du côté du pôle */
    EXPECT_GT(knee_back[2], 0.1F);
    EXPECT_NEAR(knee_front[2], -knee_back[2], 1e-4F); /* symétriques */
    /* sans pôle : le genou garde son côté ; la jambe pendait droite -> repli vers l'avant (-Z) */
    vec3 knee_default;
    EXPECT_TRUE(solve_reaches(g_leg, target, NULL, 0.6F, 0.4F, knee_default));
    EXPECT_LT(knee_default[2], -0.1F);
    /* triangle (A, genou, pied) : le genou est à 0,6 m de A et 0,4 m du pied */
    EXPECT_NEAR(dist(knee_front, (vec3){0.0F, 1.0F, 0.0F}), 0.6F, 1e-4F);
    EXPECT_NEAR(dist(knee_front, (float *)target), 0.4F, 1e-4F);
}

UTEST(anim_ik, a_target_on_the_edge_of_the_reach_is_reached_exactly)
{
    vec3 knee;
    const float nearly_far[3] = {0.0F, 1.0F - 0.99F, 0.0F}; /* à 0,99 m de A, portée 0,999 m */
    EXPECT_TRUE(solve_reaches(g_leg, nearly_far, NULL, 0.6F, 0.4F, knee));
    const float nearly_close[3] = {0.0F, 1.0F - 0.21F, 0.0F}; /* à 0,21 m, portée mini 0,2 m */
    EXPECT_TRUE(solve_reaches(g_leg, nearly_close, NULL, 0.6F, 0.4F, knee));
}

UTEST(anim_ik, an_unreachable_target_stretches_the_leg_towards_it)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_leg));
    const float far[3] = {3.0F, 1.0F, 0.0F}; /* à 3 m, sur la droite horizontale passant par A */
    ASSERT_TRUE(eng_ik_two_bone(&f.pose, &f.sk, 1, 2, 3, far, NULL, 1.0F));
    ASSERT_TRUE(refresh(&f));
    vec3 a;
    vec3 c;
    pos(&f, 1, a);
    pos(&f, 3, c);
    EXPECT_NEAR(dist(a, c), 0.999F, 1e-4F); /* 1 mm avant la jambe tendue */
    EXPECT_NEAR(c[0], 0.999F, 1e-3F);       /* dans la direction de la cible */
    EXPECT_NEAR(c[1], 1.0F, 1e-3F);
    EXPECT_TRUE(bones_keep_length(&f, 0.6F, 0.4F));
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_ik, a_target_too_close_stops_at_the_minimum_reach)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_leg));
    const float on_thigh[3] = {0.0F, 1.0F, 0.0F}; /* exactement sur la cuisse */
    ASSERT_TRUE(eng_ik_two_bone(&f.pose, &f.sk, 1, 2, 3, on_thigh, NULL, 1.0F));
    ASSERT_TRUE(refresh(&f));
    vec3 a;
    vec3 c;
    pos(&f, 1, a);
    pos(&f, 3, c);
    EXPECT_NEAR(dist(a, c), 0.2F, 1e-4F); /* |0,6 - 0,4| */
    EXPECT_TRUE(bones_keep_length(&f, 0.6F, 0.4F));
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_ik, a_straight_leg_without_pole_bends_forward_or_sideways_when_aligned_with_z)
{
    vec3 knee;
    const float below[3] = {0.0F, 0.4F, 0.0F};
    EXPECT_TRUE(solve_reaches(g_leg, below, NULL, 0.6F, 0.4F, knee));
    EXPECT_LT(knee[2], -0.1F); /* avant = -Z */
    EXPECT_NEAR(knee[0], 0.0F, 1e-4F);
    const float along_z[3] = {0.0F, 1.0F, -0.7F}; /* chaîne alignée sur Z : repli vers +X */
    EXPECT_TRUE(solve_reaches(g_leg, along_z, NULL, 0.6F, 0.4F, knee));
    EXPECT_GT(knee[0], 0.1F);
    const float behind[3] = {0.0F, 1.0F, 0.7F};
    EXPECT_TRUE(solve_reaches(g_leg, behind, NULL, 0.6F, 0.4F, knee));
    EXPECT_GT(knee[0], 0.1F);
}

UTEST(anim_ik, weight_blends_between_the_original_and_the_solved_pose)
{
    Fx full;
    Fx half;
    Fx none;
    ASSERT_TRUE(fx_init(&full, g_leg));
    ASSERT_TRUE(fx_init(&half, g_leg));
    ASSERT_TRUE(fx_init(&none, g_leg));
    const float target[3] = {0.3F, 0.3F, 0.0F};
    ASSERT_TRUE(eng_ik_two_bone(&full.pose, &full.sk, 1, 2, 3, target, NULL, 1.0F));
    ASSERT_TRUE(eng_ik_two_bone(&half.pose, &half.sk, 1, 2, 3, target, NULL, 0.5F));
    ASSERT_TRUE(eng_ik_two_bone(&none.pose, &none.sk, 1, 2, 3, target, NULL, 0.0F));
    EXPECT_TRUE(same_rotations(
        &none.pose,
        &(EngPose){.joint_count = 4,
                   .rotation = (float[16]){0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1}}));
    /* l'angle de la cuisse à mi-poids est la moitié de l'angle à poids plein */
    const float full_angle = 2.0F * acosf(fminf(fabsf(full.pose.rotation[(4U * 1U) + 3U]), 1.0F));
    const float half_angle = 2.0F * acosf(fminf(fabsf(half.pose.rotation[(4U * 1U) + 3U]), 1.0F));
    EXPECT_GT(full_angle, 0.1F);
    EXPECT_NEAR(half_angle, full_angle * 0.5F, 2e-3F);
    /* rotations unitaires, articulations hors chaîne intactes */
    for (size_t j = 0; j < 4; j++) {
        const float *q = &half.pose.rotation[4U * j];
        EXPECT_NEAR((q[0] * q[0]) + (q[1] * q[1]) + (q[2] * q[2]) + (q[3] * q[3]), 1.0F, 1e-5F);
    }
    EXPECT_NEAR(half.pose.rotation[3], 1.0F, 0.0F); /* racine */
    EXPECT_NEAR(half.pose.rotation[15], 1.0F,
                0.0F); /* pied : sa rotation LOCALE n'est pas touchée */
    ASSERT_TRUE(refresh(&half));
    vec3 c;
    pos(&half, 3, c);
    const float d_half = dist(c, (float *)target);
    ASSERT_TRUE(refresh(&none));
    pos(&none, 3, c);
    const float d_none = dist(c, (float *)target);
    EXPECT_GT(d_half, 1e-3F);
    EXPECT_LT(d_half, d_none);
    eng_arena_shutdown(&full.arena);
    eng_arena_shutdown(&half.arena);
    eng_arena_shutdown(&none.arena);
}

UTEST(anim_ik, works_under_a_rotated_translated_parent_and_an_armature_scale)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_leg));
    /* racine tournée de 90° autour de Y et décalée : la cible est en espace du personnage */
    const float h = 0.70710678F;
    f.pose.rotation[0] = 0.0F;
    f.pose.rotation[1] = h;
    f.pose.rotation[2] = 0.0F;
    f.pose.rotation[3] = h;
    f.pose.translation[0] = 5.0F;
    ASSERT_TRUE(refresh(&f));
    vec3 a;
    pos(&f, 1, a);
    const float target[3] = {a[0] + 0.1F, 0.3F, a[2] + 0.2F};
    ASSERT_TRUE(eng_ik_two_bone(&f.pose, &f.sk, 1, 2, 3, target, NULL, 1.0F));
    ASSERT_TRUE(refresh(&f));
    vec3 c;
    pos(&f, 3, c);
    EXPECT_LT(dist(c, (float *)target), 1e-4F);
    EXPECT_TRUE(bones_keep_length(&f, 0.6F, 0.4F));
    eng_arena_shutdown(&f.arena);

    /* version en centimètres sous une racine à 0,01 : mêmes résultats en mètres */
    Fx g;
    ASSERT_TRUE(fx_init(&g, g_leg_cm));
    const float target_m[3] = {0.2F, 0.3F, 0.0F};
    ASSERT_TRUE(eng_ik_two_bone(&g.pose, &g.sk, 1, 2, 3, target_m, NULL, 1.0F));
    ASSERT_TRUE(refresh(&g));
    pos(&g, 3, c);
    EXPECT_LT(dist(c, (float *)target_m), 1e-4F);
    EXPECT_TRUE(bones_keep_length(&g, 0.6F, 0.4F));
    eng_arena_shutdown(&g.arena);
}

UTEST(anim_ik, a_bent_starting_pose_and_a_second_solve_stay_consistent)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_leg));
    const float t1[3] = {0.25F, 0.35F, -0.1F};
    const float t2[3] = {-0.2F, 0.5F, 0.15F};
    ASSERT_TRUE(eng_ik_two_bone(&f.pose, &f.sk, 1, 2, 3, t1, NULL, 1.0F));
    ASSERT_TRUE(
        eng_ik_two_bone(&f.pose, &f.sk, 1, 2, 3, t2, NULL, 1.0F)); /* part d'une pose pliée */
    ASSERT_TRUE(refresh(&f));
    vec3 c;
    pos(&f, 3, c);
    EXPECT_LT(dist(c, (float *)t2), 1e-4F);
    EXPECT_TRUE(bones_keep_length(&f, 0.6F, 0.4F));
    eng_arena_shutdown(&f.arena);
}

static bool refused_unchanged(Fx *f, uint32_t u, uint32_t l, uint32_t e, const float *target,
                              const float *pole, float weight)
{
    EngPose before = f->pose;
    float saved[16];
    memcpy(saved, f->pose.rotation, sizeof saved);
    before.rotation = saved;
    const bool refused = !eng_ik_two_bone(&f->pose, &f->sk, u, l, e, target, pole, weight);
    return refused && same_rotations(&before, &f->pose);
}

UTEST(anim_ik, bad_requests_are_refused_and_leave_the_pose_alone)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_leg));
    const float ok[3] = {0.1F, 0.3F, 0.0F};
    const float nan3[3] = {0.0F, NAN, 0.0F};
    const float inf3[3] = {INFINITY, 0.0F, 0.0F};
    EXPECT_TRUE(refused_unchanged(&f, 1, 2, 3, nan3, NULL, 1.0F));
    EXPECT_TRUE(refused_unchanged(&f, 1, 2, 3, inf3, NULL, 1.0F));
    EXPECT_TRUE(refused_unchanged(&f, 1, 2, 3, ok, nan3, 1.0F));
    EXPECT_TRUE(refused_unchanged(&f, 1, 2, 3, ok, NULL, NAN));
    EXPECT_TRUE(refused_unchanged(&f, 1, 2, 3, ok, NULL, -0.01F));
    EXPECT_TRUE(refused_unchanged(&f, 1, 2, 3, ok, NULL, 1.01F));
    EXPECT_TRUE(refused_unchanged(&f, 2, 1, 3, ok, NULL, 1.0F)); /* ordre inversé */
    EXPECT_TRUE(refused_unchanged(&f, 1, 2, 2, ok, NULL, 1.0F)); /* pied = genou */
    EXPECT_TRUE(refused_unchanged(&f, 0, 2, 3, ok, NULL, 1.0F)); /* genou pas enfant de la racine */
    EXPECT_TRUE(refused_unchanged(&f, 0, 1, 3, ok, NULL, 1.0F)); /* pied pas enfant du genou */
    EXPECT_TRUE(refused_unchanged(&f, 1, 2, 4, ok, NULL, 1.0F)); /* indice == nombre */
    EXPECT_TRUE(refused_unchanged(&f, 1, 4, 3, ok, NULL, 1.0F));
    EXPECT_TRUE(refused_unchanged(&f, 4, 2, 3, ok, NULL, 1.0F));
    EXPECT_FALSE(eng_ik_two_bone(NULL, &f.sk, 1, 2, 3, ok, NULL, 1.0F));
    EXPECT_FALSE(eng_ik_two_bone(&f.pose, NULL, 1, 2, 3, ok, NULL, 1.0F));
    EXPECT_FALSE(eng_ik_two_bone(&f.pose, &f.sk, 1, 2, 3, NULL, NULL, 1.0F));
    /* poids 0 : accepté, aucune modification, même avec une cible hors de portée */
    EXPECT_TRUE(eng_ik_two_bone(&f.pose, &f.sk, 1, 2, 3, ok, NULL, 0.0F));
    /* poids 1 accepté (borne) */
    EXPECT_TRUE(eng_ik_two_bone(&f.pose, &f.sk, 1, 2, 3, ok, NULL, 1.0F));
    /* pose d'une autre taille */
    EngPose other = f.pose;
    other.joint_count = 3;
    EXPECT_FALSE(eng_ik_two_bone(&other, &f.sk, 1, 2, 3, ok, NULL, 1.0F));
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_ik, a_zero_length_bone_is_refused)
{
    static EngJoint flat[4] = {
        {-1, {0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "root"},
        {0, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "thigh"},
        {1, {0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "knee"}, /* genou sur la cuisse */
        {2, {0, -0.4F, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "foot"},
    };
    Fx f;
    ASSERT_TRUE(fx_init(&f, flat));
    const float target[3] = {0.1F, 0.7F, 0.0F};
    EXPECT_TRUE(refused_unchanged(&f, 1, 2, 3, target, NULL, 1.0F));
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_ik, without_a_pole_the_knee_keeps_the_side_it_is_already_on)
{
    for (int side = -1; side <= 1; side += 2) {
        Fx f;
        ASSERT_TRUE(fx_init(&f, g_leg));
        /* plie d'abord vers le côté voulu (pôle explicite), puis résout SANS pôle une autre cible
         */
        const float pole[3] = {0.0F, 0.5F, (float)side * 5.0F};
        const float first[3] = {0.0F, 0.45F, 0.0F};
        ASSERT_TRUE(eng_ik_two_bone(&f.pose, &f.sk, 1, 2, 3, first, pole, 1.0F));
        const float second[3] = {0.15F, 0.35F, 0.0F};
        ASSERT_TRUE(eng_ik_two_bone(&f.pose, &f.sk, 1, 2, 3, second, NULL, 1.0F));
        ASSERT_TRUE(refresh(&f));
        vec3 knee;
        pos(&f, 2, knee);
        EXPECT_GT(knee[2] * (float)side, 0.1F);
        vec3 c;
        pos(&f, 3, c);
        EXPECT_LT(dist(c, (float *)second), 1e-4F);
        eng_arena_shutdown(&f.arena);
    }
}

UTEST(anim_ik, a_target_above_the_hip_flips_the_leg_by_half_a_turn)
{
    /* le pied pend sous la hanche ; la cible est 0,7 m AU-DESSUS : demi-tour exact de la cuisse */
    const float above[3] = {0.0F, 1.7F, 0.0F};
    const float pole[3] = {0.0F, 1.3F, -5.0F};
    vec3 knee;
    EXPECT_TRUE(solve_reaches(g_leg, above, pole, 0.6F, 0.4F, knee));
    EXPECT_LT(knee[2], -0.1F);
    EXPECT_GT(knee[1], 1.0F); /* le genou est monté au-dessus de la hanche */
}

UTEST(anim_ik, equal_bones_stop_one_millimetre_from_the_hip)
{
    static EngJoint equal[4] = {
        {-1, {0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "root"},
        {0, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "thigh"},
        {1, {0, -0.5F, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "knee"},
        {2, {0, -0.5F, 0}, {0, 0, 0, 1}, {1, 1, 1}, {0}, "foot"},
    };
    Fx f;
    ASSERT_TRUE(fx_init(&f, equal));
    const float on_hip[3] = {0.0F, 1.0F, 0.0F};
    ASSERT_TRUE(eng_ik_two_bone(&f.pose, &f.sk, 1, 2, 3, on_hip, NULL, 1.0F));
    ASSERT_TRUE(refresh(&f));
    vec3 a;
    vec3 c;
    pos(&f, 1, a);
    pos(&f, 3, c);
    EXPECT_NEAR(dist(a, c), 0.001F, 5e-5F); /* la rotation en flottants perd ~2e-5 m */
    EXPECT_TRUE(bones_keep_length(&f, 0.5F, 0.5F));
    eng_arena_shutdown(&f.arena);
}
