/* Évaluation de pose (M4-14) : temps -> pose locale -> matrices monde -> matrices de skinning.
 * Chaîne de 3 articulations construite à la main (valeurs attendues calculées à la main). */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "engine/pose.h"
#include "utest.h"

#define H 0.70710678F /* sin 45° = cos 45° */

/* Chaîne verticale : racine à l'origine, puis +1 m en Y à chaque étape (repos = liaison). */
static EngJoint g_joints[3] = {
    {-1, {0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, "r"},
    {0, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1}, "a"},
    {1, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -2, 0, 1}, "b"},
};

static EngSkeleton chain(void)
{
    return (EngSkeleton){.joint_count = 3, .joints = g_joints};
}

/* Clip : articulation 1 tourne de 0 à 90° autour de Z entre 0 et 1 s ; articulation 0 a une
 * échelle constante 2 (une clé). Canaux triés (articulation, propriété). */
static EngAnimChannel g_channels[2] = {{0, ENG_ANIM_SCALE, 1, 0}, {1, ENG_ANIM_ROTATION, 2, 1}};
static float g_times[3] = {0.0F, 0.0F, 1.0F};
static float g_values[3 + 8] = {1, 1, 1,     /* échelle de l'articulation 0 : tenue à 1 */
                                0, 0, 0, 1,  /* identité */
                                0, 0, H, H}; /* 90° autour de Z */
static uint32_t g_value_first[2] = {0, 3};

static EngAnimClip rot_clip(void)
{
    return (EngAnimClip){.duration = 1.0F,
                         .channel_count = 2,
                         .channels = g_channels,
                         .key_total = 3,
                         .times = g_times,
                         .values = g_values,
                         .value_first = g_value_first,
                         .skeleton_path = "p.skel"};
}

static void apply(mat4 m, const float p[3], float out[3])
{
    vec3 r;
    glm_mat4_mulv3(m, (float *)(vec3){p[0], p[1], p[2]}, 1.0F, r);
    out[0] = r[0];
    out[1] = r[1];
    out[2] = r[2];
}

/* Égalité bit à bit (memcmp sur des flottants est refusé par clang-tidy). */
static bool same_bytes(const void *a, const void *b, size_t n)
{
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) {
            return false;
        }
    }
    return true;
}

/* Valeur d'énumération hors plage, sans constante visible de l'analyseur. */
static EngAnimWrap unknown_wrap(int v)
{
    EngAnimWrap w = ENG_ANIM_WRAP_CLAMP;
    memcpy(&w, &v, sizeof w);
    return w;
}

static bool near3(const float a[3], float x, float y, float z, float eps)
{
    return fabsf(a[0] - x) <= eps && fabsf(a[1] - y) <= eps && fabsf(a[2] - z) <= eps;
}

static bool is_identity(mat4 m, float eps)
{
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            if (fabsf(m[c][r] - (c == r ? 1.0F : 0.0F)) > eps) {
                return false;
            }
        }
    }
    return true;
}

UTEST(pose, wrap_time_clamps_loops_and_rejects_invalid_input)
{
    EXPECT_NEAR(eng_anim_wrap_time(2.0F, 1.5F, ENG_ANIM_WRAP_CLAMP), 1.5F, 1e-7F);
    EXPECT_NEAR(eng_anim_wrap_time(2.0F, 9.0F, ENG_ANIM_WRAP_CLAMP), 2.0F, 1e-7F);
    EXPECT_NEAR(eng_anim_wrap_time(2.0F, -9.0F, ENG_ANIM_WRAP_CLAMP), 0.0F, 1e-7F);
    EXPECT_NEAR(eng_anim_wrap_time(2.0F, 2.5F, ENG_ANIM_WRAP_LOOP), 0.5F, 1e-6F);
    EXPECT_NEAR(eng_anim_wrap_time(2.0F, -0.5F, ENG_ANIM_WRAP_LOOP), 1.5F, 1e-6F);
    EXPECT_NEAR(eng_anim_wrap_time(2.0F, 4.0F, ENG_ANIM_WRAP_LOOP), 0.0F, 1e-7F); /* [0 ; durée[ */
    EXPECT_NEAR(eng_anim_wrap_time(0.0F, 7.0F, ENG_ANIM_WRAP_LOOP), 0.0F, 1e-7F);
    EXPECT_NEAR(eng_anim_wrap_time(0.0F, 7.0F, ENG_ANIM_WRAP_CLAMP), 0.0F, 1e-7F);
    /* un tout petit négatif ne doit pas donner exactement `duration` */
    const float tiny = eng_anim_wrap_time(1.0F, -1e-9F, ENG_ANIM_WRAP_LOOP);
    EXPECT_TRUE(tiny >= 0.0F && tiny < 1.0F);
    EXPECT_TRUE(eng_anim_wrap_time(1.0F, NAN, ENG_ANIM_WRAP_LOOP) < 0.0F);
    EXPECT_TRUE(eng_anim_wrap_time(1.0F, INFINITY, ENG_ANIM_WRAP_CLAMP) < 0.0F);
    EXPECT_TRUE(eng_anim_wrap_time(INFINITY, 1.0F, ENG_ANIM_WRAP_CLAMP) < 0.0F);
    EXPECT_TRUE(eng_anim_wrap_time(-1.0F, 1.0F, ENG_ANIM_WRAP_CLAMP) < 0.0F);
    EXPECT_TRUE(eng_anim_wrap_time(1.0F, 0.5F, unknown_wrap(7)) < 0.0F);
}

UTEST(pose, init_starts_at_rest_and_matches_the_skeleton_rest_world)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose", (size_t)1 << 16));
    const EngSkeleton sk = chain();
    EngPose pose;
    ASSERT_TRUE(eng_pose_init(&arena, &sk, &pose));
    EXPECT_EQ(pose.joint_count, 3U);
    mat4 a[3];
    mat4 b[3];
    ASSERT_TRUE(eng_pose_world(&pose, &sk, a));
    ASSERT_TRUE(eng_skeleton_rest_world(&sk, b));
    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(a[i][3][1], (float)i, 1e-6F);
        EXPECT_TRUE(same_bytes(a[i], b[i], sizeof(mat4)));
    }
    eng_arena_shutdown(&arena);
}

UTEST(pose, rest_pose_gives_identity_skin_matrices_when_rest_equals_bind)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose", (size_t)1 << 16));
    const EngSkeleton sk = chain();
    EngPose pose;
    ASSERT_TRUE(eng_pose_init(&arena, &sk, &pose));
    mat4 world[3];
    mat4 skin[3];
    ASSERT_TRUE(eng_pose_world(&pose, &sk, world));
    ASSERT_TRUE(eng_pose_skin_matrices(&sk, world, skin));
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(is_identity(skin[i], 1e-6F));
    }
    eng_arena_shutdown(&arena);
}

UTEST(pose, a_quarter_turn_at_the_middle_joint_moves_the_tip_as_computed_by_hand)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose", (size_t)1 << 16));
    const EngSkeleton sk = chain();
    const EngAnimClip clip = rot_clip();
    EngPose pose;
    ASSERT_TRUE(eng_pose_init(&arena, &sk, &pose));
    ASSERT_TRUE(eng_pose_sample(&pose, &sk, &clip, 1.0F, ENG_ANIM_WRAP_CLAMP));
    mat4 world[3];
    mat4 skin[3];
    ASSERT_TRUE(eng_pose_world(&pose, &sk, world));
    ASSERT_TRUE(eng_pose_skin_matrices(&sk, world, skin));
    /* A : monde = T(0,1,0)·Rz(90°) ; B = A·T(0,1,0) : Rz(90°) envoie (0,1,0) sur (-1,0,0). */
    EXPECT_NEAR(world[1][3][1], 1.0F, 1e-6F);
    EXPECT_NEAR(world[2][3][0], -1.0F, 1e-5F);
    EXPECT_NEAR(world[2][3][1], 1.0F, 1e-5F);
    /* Sommet posé sur B en liaison (0,2,0) : il se retrouve là où est B. */
    float p[3] = {0.0F, 2.0F, 0.0F};
    float q[3];
    apply(skin[2], p, q);
    EXPECT_TRUE(near3(q, -1.0F, 1.0F, 0.0F, 1e-5F));
    /* Sommet décalé de +1 en X par rapport à B : tourné de 90° -> +1 en Y. */
    const float p2[3] = {1.0F, 2.0F, 0.0F};
    apply(skin[2], p2, q);
    EXPECT_TRUE(near3(q, -1.0F, 2.0F, 0.0F, 1e-5F));
    /* La racine, sans canal de rotation, ne bouge pas. */
    EXPECT_TRUE(is_identity(skin[0], 1e-6F));
    eng_arena_shutdown(&arena);
}

UTEST(pose, mid_clip_time_interpolates_the_rotation)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose", (size_t)1 << 16));
    const EngSkeleton sk = chain();
    const EngAnimClip clip = rot_clip();
    EngPose pose;
    ASSERT_TRUE(eng_pose_init(&arena, &sk, &pose));
    ASSERT_TRUE(eng_pose_sample(&pose, &sk, &clip, 0.5F, ENG_ANIM_WRAP_CLAMP)); /* 45° */
    mat4 world[3];
    ASSERT_TRUE(eng_pose_world(&pose, &sk, world));
    EXPECT_NEAR(world[2][3][0], -H, 1e-5F);
    EXPECT_NEAR(world[2][3][1], 1.0F + H, 1e-5F);
    eng_arena_shutdown(&arena);
}

UTEST(pose, loop_and_clamp_pick_the_expected_instants)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose", (size_t)1 << 16));
    const EngSkeleton sk = chain();
    const EngAnimClip clip = rot_clip();
    EngPose a;
    EngPose b;
    ASSERT_TRUE(eng_pose_init(&arena, &sk, &a));
    ASSERT_TRUE(eng_pose_init(&arena, &sk, &b));
    ASSERT_TRUE(eng_pose_sample(&a, &sk, &clip, 1.5F, ENG_ANIM_WRAP_LOOP));
    ASSERT_TRUE(eng_pose_sample(&b, &sk, &clip, 0.5F, ENG_ANIM_WRAP_CLAMP));
    EXPECT_TRUE(same_bytes(a.rotation, b.rotation, sizeof(float) * 12U));
    ASSERT_TRUE(eng_pose_sample(&a, &sk, &clip, 5.0F, ENG_ANIM_WRAP_CLAMP));
    ASSERT_TRUE(eng_pose_sample(&b, &sk, &clip, 1.0F, ENG_ANIM_WRAP_CLAMP));
    EXPECT_TRUE(same_bytes(a.rotation, b.rotation, sizeof(float) * 12U));
    ASSERT_TRUE(eng_pose_sample(&a, &sk, &clip, -3.0F, ENG_ANIM_WRAP_CLAMP));
    EXPECT_NEAR(a.rotation[4 + 3], 1.0F, 1e-6F); /* première image : identité */
    eng_arena_shutdown(&arena);
}

UTEST(pose, joints_without_a_channel_keep_their_rest_pose_and_scale_propagates)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose", (size_t)1 << 16));
    const EngSkeleton sk = chain();
    static float scale_values[3 + 8];
    memcpy(scale_values, g_values, sizeof scale_values);
    scale_values[0] = 2.0F;
    scale_values[1] = 2.0F;
    scale_values[2] = 2.0F;
    EngAnimClip clip = rot_clip();
    clip.values = scale_values;
    EngPose pose;
    ASSERT_TRUE(eng_pose_init(&arena, &sk, &pose));
    ASSERT_TRUE(eng_pose_sample(&pose, &sk, &clip, 0.0F, ENG_ANIM_WRAP_CLAMP));
    EXPECT_NEAR(pose.scale[0], 2.0F, 1e-7F);
    EXPECT_NEAR(pose.scale[3], 1.0F, 1e-7F); /* articulation 1 : échelle de repos */
    EXPECT_NEAR(pose.translation[4], 1.0F, 1e-7F);
    mat4 world[3];
    ASSERT_TRUE(eng_pose_world(&pose, &sk, world));
    EXPECT_NEAR(world[2][3][1], 4.0F, 1e-5F); /* (1 + 1) m, doublés par l'échelle de la racine */
    eng_arena_shutdown(&arena);
}

UTEST(pose, a_clip_for_another_skeleton_is_refused_and_leaves_the_pose_untouched)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose", (size_t)1 << 16));
    const EngSkeleton sk = chain();
    EngAnimChannel bad[2] = {{0, ENG_ANIM_SCALE, 1, 0},
                             {3, ENG_ANIM_ROTATION, 2, 1}}; /* 3 = hors */
    EngAnimClip clip = rot_clip();
    clip.channels = bad;
    EngPose pose;
    ASSERT_TRUE(eng_pose_init(&arena, &sk, &pose));
    const EngAnimClip good = rot_clip();
    ASSERT_TRUE(eng_pose_sample(&pose, &sk, &good, 1.0F, ENG_ANIM_WRAP_CLAMP));
    float before[12];
    memcpy(before, pose.rotation, sizeof before);
    EXPECT_FALSE(eng_pose_sample(&pose, &sk, &clip, 0.0F, ENG_ANIM_WRAP_CLAMP));
    EXPECT_TRUE(same_bytes(before, pose.rotation, sizeof before));
    EngAnimChannel bad_path[1] = {{0, ENG_ANIM_PATH_COUNT, 1, 0}};
    clip = rot_clip();
    clip.channel_count = 1;
    clip.channels = bad_path;
    EXPECT_FALSE(eng_pose_sample(&pose, &sk, &clip, 0.0F, ENG_ANIM_WRAP_CLAMP));
    EXPECT_TRUE(same_bytes(before, pose.rotation, sizeof before));
    eng_arena_shutdown(&arena);
}

UTEST(pose, invalid_arguments_and_times_are_refused)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose", (size_t)1 << 16));
    const EngSkeleton sk = chain();
    const EngAnimClip clip = rot_clip();
    EngPose pose;
    ASSERT_TRUE(eng_pose_init(&arena, &sk, &pose));
    EXPECT_FALSE(eng_pose_sample(&pose, &sk, &clip, NAN, ENG_ANIM_WRAP_LOOP));
    EXPECT_FALSE(eng_pose_sample(&pose, &sk, NULL, 0.0F, ENG_ANIM_WRAP_LOOP));
    EXPECT_FALSE(eng_pose_sample(NULL, &sk, &clip, 0.0F, ENG_ANIM_WRAP_LOOP));
    EXPECT_FALSE(eng_pose_sample(&pose, NULL, &clip, 0.0F, ENG_ANIM_WRAP_LOOP));
    EXPECT_FALSE(eng_pose_sample(&pose, &sk, &clip, 0.0F, unknown_wrap(3)));
    EngSkeleton two = {.joint_count = 2, .joints = g_joints};
    EXPECT_FALSE(eng_pose_sample(&pose, &two, &clip, 0.0F, ENG_ANIM_WRAP_LOOP)); /* 3 != 2 */
    mat4 m[3];
    EXPECT_FALSE(eng_pose_world(&pose, &two, m));
    EXPECT_FALSE(eng_pose_world(&pose, &sk, NULL));
    EXPECT_FALSE(eng_pose_world(NULL, &sk, m));
    EXPECT_FALSE(eng_pose_skin_matrices(&sk, NULL, m));
    EXPECT_FALSE(eng_pose_skin_matrices(&sk, m, NULL));
    EXPECT_FALSE(eng_pose_skin_matrices(NULL, m, m));
    EXPECT_FALSE(eng_pose_set_rest(&pose, &two));
    eng_arena_shutdown(&arena);
}

UTEST(pose, init_refuses_invalid_skeletons_and_restores_the_arena)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose", (size_t)1 << 16));
    const EngArenaMark mark = eng_arena_mark(&arena);
    EngPose pose;
    memset(&pose, 0xAB, sizeof pose);
    EngSkeleton none = {.joint_count = 0, .joints = g_joints};
    EXPECT_FALSE(eng_pose_init(&arena, &none, &pose));
    EXPECT_EQ(pose.joint_count, 0U);
    EXPECT_TRUE(pose.translation == NULL);
    EngSkeleton too_many = {.joint_count = ENG_SKELETON_MAX_JOINTS + 1U, .joints = g_joints};
    EXPECT_FALSE(eng_pose_init(&arena, &too_many, &pose));
    EngJoint loop[2] = {g_joints[0], g_joints[1]};
    loop[0].parent = 1; /* un parent doit PRÉCÉDER */
    EngSkeleton bad_order = {.joint_count = 2, .joints = loop};
    EXPECT_FALSE(eng_pose_init(&arena, &bad_order, &pose));
    EngSkeleton null_joints = {.joint_count = 2, .joints = NULL};
    EXPECT_FALSE(eng_pose_init(&arena, &null_joints, &pose));
    EXPECT_FALSE(eng_pose_init(NULL, &none, &pose));
    EXPECT_FALSE(eng_pose_init(&arena, NULL, &pose));
    EXPECT_FALSE(eng_pose_init(&arena, &none, NULL));
    EXPECT_EQ(arena.used, mark.used);
    /* mémoire insuffisante : arène minuscule */
    EngArena tiny;
    ASSERT_TRUE(eng_arena_init(&tiny, "tiny", 64));
    const EngSkeleton sk = chain();
    EXPECT_FALSE(eng_pose_init(&tiny, &sk, &pose));
    EXPECT_EQ(tiny.used, 0U); /* l'allocation partielle est rendue */
    EXPECT_TRUE(pose.translation == NULL);
    eng_arena_shutdown(&tiny);
    eng_arena_shutdown(&arena);
}

UTEST(pose, per_frame_evaluation_does_not_allocate_and_is_deterministic)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose", (size_t)1 << 16));
    const EngSkeleton sk = chain();
    const EngAnimClip clip = rot_clip();
    EngPose pose;
    ASSERT_TRUE(eng_pose_init(&arena, &sk, &pose));
    const size_t used = arena.used;
    mat4 w1[3];
    mat4 w2[3];
    ASSERT_TRUE(eng_pose_sample(&pose, &sk, &clip, 0.3F, ENG_ANIM_WRAP_LOOP));
    ASSERT_TRUE(eng_pose_world(&pose, &sk, w1));
    ASSERT_TRUE(eng_pose_sample(&pose, &sk, &clip, 0.9F, ENG_ANIM_WRAP_LOOP));
    ASSERT_TRUE(eng_pose_sample(&pose, &sk, &clip, 0.3F, ENG_ANIM_WRAP_LOOP));
    ASSERT_TRUE(eng_pose_world(&pose, &sk, w2));
    EXPECT_TRUE(same_bytes(w1, w2, sizeof w1));
    EXPECT_EQ(arena.used, used);
    eng_arena_shutdown(&arena);
}

/* Articulation 2 : translation et échelle animées (indices 6..8 des tableaux), repos non trivial
 * sur les autres. */
UTEST(pose, every_property_lands_in_its_own_slot_of_the_right_joint)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose", (size_t)1 << 16));
    EngJoint js[3] = {g_joints[0], g_joints[1], g_joints[2]};
    /* repos : articulation 1 tournée de 90° autour de X, racine étirée en Y */
    js[1].rotation[0] = H;
    js[1].rotation[3] = H;
    js[0].scale[1] = 3.0F;
    const EngSkeleton sk = {.joint_count = 3, .joints = js};
    EngAnimChannel ch[2] = {{2, ENG_ANIM_TRANSLATION, 1, 0}, {2, ENG_ANIM_SCALE, 1, 1}};
    float times[2] = {0.0F, 0.0F};
    float values[6] = {0.5F, 0.25F, 0.125F, 1.0F, 2.0F, 4.0F};
    uint32_t first[2] = {0, 3};
    const EngAnimClip clip = {.duration = 1.0F,
                              .channel_count = 2,
                              .channels = ch,
                              .key_total = 2,
                              .times = times,
                              .values = values,
                              .value_first = first,
                              .skeleton_path = "p.skel"};
    EngPose pose;
    ASSERT_TRUE(eng_pose_init(&arena, &sk, &pose));
    ASSERT_TRUE(eng_pose_sample(&pose, &sk, &clip, 0.0F, ENG_ANIM_WRAP_CLAMP));
    /* repos conservé articulation par articulation */
    EXPECT_NEAR(pose.scale[1], 3.0F, 1e-7F);
    EXPECT_NEAR(pose.scale[4], 1.0F, 1e-7F);
    EXPECT_NEAR(pose.rotation[4], H, 1e-7F); /* articulation 1 : x */
    EXPECT_NEAR(pose.rotation[7], H, 1e-7F); /* articulation 1 : w */
    EXPECT_NEAR(pose.rotation[11], 1.0F, 1e-7F);
    EXPECT_NEAR(pose.rotation[3], 1.0F, 1e-7F);
    EXPECT_NEAR(pose.translation[3 + 1], 1.0F, 1e-7F);
    /* valeurs animées : articulation 2 seulement */
    EXPECT_NEAR(pose.translation[6], 0.5F, 1e-7F);
    EXPECT_NEAR(pose.translation[7], 0.25F, 1e-7F);
    EXPECT_NEAR(pose.translation[8], 0.125F, 1e-7F);
    EXPECT_NEAR(pose.scale[6], 1.0F, 1e-7F);
    EXPECT_NEAR(pose.scale[7], 2.0F, 1e-7F);
    EXPECT_NEAR(pose.scale[8], 4.0F, 1e-7F);
    EXPECT_NEAR(pose.translation[0], 0.0F, 1e-7F);
    eng_arena_shutdown(&arena);
}

UTEST(pose, the_maximum_joint_count_is_accepted_and_a_self_parent_is_not)
{
    static EngJoint many[ENG_SKELETON_MAX_JOINTS];
    for (uint32_t i = 0; i < ENG_SKELETON_MAX_JOINTS; i++) {
        many[i] = g_joints[i == 0 ? 0 : 1];
        many[i].parent = (int32_t)i - 1;
    }
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose", (size_t)1 << 20));
    EngSkeleton sk = {.joint_count = ENG_SKELETON_MAX_JOINTS, .joints = many};
    EngPose pose;
    ASSERT_TRUE(eng_pose_init(&arena, &sk, &pose));
    EXPECT_EQ(pose.joint_count, ENG_SKELETON_MAX_JOINTS);
    static mat4 world[ENG_SKELETON_MAX_JOINTS];
    ASSERT_TRUE(eng_pose_world(&pose, &sk, world));
    EXPECT_NEAR(world[ENG_SKELETON_MAX_JOINTS - 1U][3][1], 255.0F, 1e-3F);
    many[5].parent = 5; /* son propre parent */
    EXPECT_FALSE(eng_pose_init(&arena, &sk, &pose));
    many[5].parent = -2; /* indice négatif autre que -1 */
    EXPECT_FALSE(eng_pose_init(&arena, &sk, &pose));
    eng_arena_shutdown(&arena);
}

/* Coût CPU d'une image avec 10 personnages de 65 articulations (taille d'un squelette Mixamo),
 * chacun avec un clip de 65 canaux de rotation à 60 clés : échantillonnage + matrices monde +
 * matrices de skinning. Le temps est ÉCRIT (stderr) ; seule une borne très large est exigée,
 * valable même sous ASan/UBSan (la mesure utile est celle du build release). */
#define BENCH_JOINTS 65U
#define BENCH_KEYS 60U
#define BENCH_CHARS 10U
#define BENCH_FRAMES 200U

UTEST(pose, ten_characters_of_65_joints_cost_little_cpu_per_frame)
{
    static EngJoint joints[BENCH_JOINTS];
    for (uint32_t i = 0; i < BENCH_JOINTS; i++) {
        joints[i] = g_joints[1];
        joints[i].parent = (int32_t)i - 1;
        joints[i].inverse_bind[13] = -(float)i;
    }
    const EngSkeleton sk = {.joint_count = BENCH_JOINTS, .joints = joints};
    static EngAnimChannel channels[BENCH_JOINTS];
    static float times[BENCH_JOINTS * BENCH_KEYS];
    static float values[BENCH_JOINTS * BENCH_KEYS * 4U];
    static uint32_t first[BENCH_JOINTS];
    for (uint32_t j = 0; j < BENCH_JOINTS; j++) {
        channels[j] = (EngAnimChannel){j, ENG_ANIM_ROTATION, BENCH_KEYS, j * BENCH_KEYS};
        first[j] = j * BENCH_KEYS * 4U;
        for (uint32_t k = 0; k < BENCH_KEYS; k++) {
            const float angle = 0.3F * sinf((float)k * 0.2F + (float)j);
            float *q = &values[first[j] + (k * 4U)];
            q[0] = 0.0F;
            q[1] = 0.0F;
            q[2] = sinf(angle * 0.5F);
            q[3] = cosf(angle * 0.5F);
            times[(j * BENCH_KEYS) + k] = (float)k / 30.0F;
        }
    }
    const EngAnimClip clip = {.duration = (float)(BENCH_KEYS - 1U) / 30.0F,
                              .channel_count = BENCH_JOINTS,
                              .channels = channels,
                              .key_total = BENCH_JOINTS * BENCH_KEYS,
                              .times = times,
                              .values = values,
                              .value_first = first,
                              .skeleton_path = "p.skel"};
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "pose-bench", (size_t)1 << 20));
    EngPose poses[BENCH_CHARS];
    for (uint32_t c = 0; c < BENCH_CHARS; c++) {
        ASSERT_TRUE(eng_pose_init(&arena, &sk, &poses[c]));
    }
    static mat4 world[BENCH_JOINTS];
    static mat4 skin[BENCH_JOINTS];
    float checksum = 0.0F;
    const uint64_t start = SDL_GetPerformanceCounter();
    for (uint32_t f = 0; f < BENCH_FRAMES; f++) {
        for (uint32_t c = 0; c < BENCH_CHARS; c++) {
            const float t = ((float)f * 0.016F) + ((float)c * 0.1F);
            ASSERT_TRUE(eng_pose_sample(&poses[c], &sk, &clip, t, ENG_ANIM_WRAP_LOOP));
            ASSERT_TRUE(eng_pose_world(&poses[c], &sk, world));
            ASSERT_TRUE(eng_pose_skin_matrices(&sk, world, skin));
            checksum += skin[BENCH_JOINTS - 1U][3][0];
        }
    }
    const double ms = (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
                      (double)SDL_GetPerformanceFrequency() / (double)BENCH_FRAMES;
    (void)fprintf(stderr, "pose : %u personnages x %u os : %.3f ms par image (CPU, un coeur)\n",
                  BENCH_CHARS, BENCH_JOINTS, ms);
    EXPECT_TRUE(isfinite(checksum)); /* le calcul n'est pas éliminé */
    EXPECT_LT(ms, 20.0);             /* borne large : sous ASan/UBSan aussi */
    eng_arena_shutdown(&arena);
}
