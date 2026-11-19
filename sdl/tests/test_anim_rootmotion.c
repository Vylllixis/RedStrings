/* Root motion (M4-20) : delta de la racine, enchaînement, bouclage, pose « en place ». Chaîne de 3
 * articulations, racine = articulation 0. Clip « walk » : la racine avance de 1,5 m (z de 0 à -1,5)
 * en 1,2 s. Clip « face » : racine tournée de 90 degrés (cap constant) qui se déplace vers -x. */
#include <math.h>
#include <string.h>

#include "engine/anim_rootmotion.h"
#include "utest.h"

#define H 0.70710678F
#define PI_F 3.14159265F

static EngJoint g_joints[3] = {
    {-1, {0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, "r"},
    {0, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1}, "a"},
    {1, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -2, 0, 1}, "b"},
};
/* walk : translation de la racine, avec un balancement vertical (y = 0,1 au milieu) */
static EngAnimChannel g_w_ch[1] = {{0, ENG_ANIM_TRANSLATION, 3, 0}};
static float g_w_t[3] = {0.0F, 0.6F, 1.2F};
static float g_w_v[9] = {0, 0, 0, 0, 0.1F, -0.75F, 0, 0, -1.5F};
static uint32_t g_w_f[1] = {0};
/* face : racine tournée de 90 degrés, déplacée vers -x */
static EngAnimChannel g_f_ch[2] = {{0, ENG_ANIM_TRANSLATION, 2, 0}, {0, ENG_ANIM_ROTATION, 2, 2}};
static float g_f_t[4] = {0.0F, 1.0F, 0.0F, 1.0F};
static float g_f_v[14] = {0, 0, 0, -1, 0, 0, 0, H, 0, H, 0, H, 0, H};
static uint32_t g_f_f[2] = {0, 6};
/* none : aucun canal sur la racine (un canal sur l'articulation 1) */
static EngAnimChannel g_n_ch[1] = {{1, ENG_ANIM_TRANSLATION, 2, 0}};
static float g_n_t[2] = {0.0F, 1.0F};
static float g_n_v[6] = {0, 0, 0, 5, 5, 5};
static uint32_t g_n_f[1] = {0};

static EngAnimClip make_clip(float dur, uint32_t nch, EngAnimChannel *ch, uint32_t keys, float *t,
                             float *v, uint32_t *first)
{
    return (EngAnimClip){.duration = dur,
                         .channel_count = nch,
                         .channels = ch,
                         .key_total = keys,
                         .times = t,
                         .values = v,
                         .value_first = first,
                         .skeleton_path = "p.skel"};
}

typedef struct Fx {
    EngSkeleton sk;
    EngAnimClip walk;
    EngAnimClip face;
    EngAnimClip none;
} Fx;

static void fx_init(Fx *f)
{
    f->sk = (EngSkeleton){.joint_count = 3, .joints = g_joints};
    f->walk = make_clip(1.2F, 1, g_w_ch, 3, g_w_t, g_w_v, g_w_f);
    f->face = make_clip(1.0F, 2, g_f_ch, 4, g_f_t, g_f_v, g_f_f);
    f->none = make_clip(1.0F, 1, g_n_ch, 2, g_n_t, g_n_v, g_n_f);
}

UTEST(anim_rootmotion, yaw_of_a_quaternion_is_the_heading_around_y)
{
    const float id[4] = {0, 0, 0, 1};
    EXPECT_NEAR(eng_rootmotion_yaw(id), 0.0F, 1e-6F);
    const float left[4] = {0, H, 0, H}; /* +90 degrés autour de Y */
    EXPECT_NEAR(eng_rootmotion_yaw(left), PI_F / 2.0F, 1e-5F);
    const float right[4] = {0, -H, 0, H};
    EXPECT_NEAR(eng_rootmotion_yaw(right), -PI_F / 2.0F, 1e-5F);
    const float back[4] = {0, 1, 0, 0};
    EXPECT_NEAR(fabsf(eng_rootmotion_yaw(back)), PI_F, 1e-5F);
    const float a30[4] = {sinf(glm_rad(15.0F)), 0, 0, cosf(glm_rad(15.0F))}; /* tangage seul */
    EXPECT_NEAR(eng_rootmotion_yaw(a30), 0.0F, 1e-5F);
    const float vertical[4] = {H, 0, 0, H}; /* avant pointé vers le haut : cap indéfini */
    EXPECT_NEAR(eng_rootmotion_yaw(vertical), 0.0F, 1e-6F);
    /* 40 degrés de lacet mêlés à 20 de tangage : cap = 40 */
    const float yaw = glm_rad(40.0F);
    const float pitch = glm_rad(20.0F);
    const float qy[4] = {0, sinf(yaw / 2), 0, cosf(yaw / 2)};
    const float qx[4] = {sinf(pitch / 2), 0, 0, cosf(pitch / 2)};
    float q[4]; /* qy * qx */
    q[0] = (qy[3] * qx[0]) + (qy[0] * qx[3]) + (qy[1] * qx[2]) - (qy[2] * qx[1]);
    q[1] = (qy[3] * qx[1]) - (qy[0] * qx[2]) + (qy[1] * qx[3]) + (qy[2] * qx[0]);
    q[2] = (qy[3] * qx[2]) + (qy[0] * qx[1]) - (qy[1] * qx[0]) + (qy[2] * qx[3]);
    q[3] = (qy[3] * qx[3]) - (qy[0] * qx[0]) - (qy[1] * qx[1]) - (qy[2] * qx[2]);
    EXPECT_NEAR(eng_rootmotion_yaw(q), yaw, 1e-4F);
}

UTEST(anim_rootmotion, delta_without_loop_follows_the_root_and_clamps_to_the_clip)
{
    Fx f;
    fx_init(&f);
    EngRootMotion d;
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 0.0F, 1.2F, false, &d));
    EXPECT_NEAR(d.translation[0], 0.0F, 1e-6F);
    EXPECT_NEAR(d.translation[1], 0.0F, 1e-7F);
    EXPECT_NEAR(d.translation[2], -1.5F, 1e-5F);
    EXPECT_NEAR(d.yaw, 0.0F, 1e-6F);
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 0.0F, 0.6F, false, &d));
    EXPECT_NEAR(d.translation[2], -0.75F, 1e-5F);
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 0.6F, 0.6F, false, &d));
    EXPECT_NEAR(d.translation[2], 0.0F, 1e-7F);
    /* au-delà de la fin et avant le début : borné */
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, -5.0F, 99.0F, false, &d));
    EXPECT_NEAR(d.translation[2], -1.5F, 1e-5F);
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 5.0F, 99.0F, false, &d));
    EXPECT_NEAR(d.translation[2], 0.0F, 1e-7F);
    /* additivité : [0 ; 0,3] + [0,3 ; 1,2] = [0 ; 1,2] */
    EngRootMotion a;
    EngRootMotion b;
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 0.0F, 0.3F, false, &a));
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 0.3F, 1.2F, false, &b));
    EXPECT_NEAR(a.translation[2] + b.translation[2], -1.5F, 1e-5F);
}

UTEST(anim_rootmotion, a_clip_without_root_channels_gives_no_motion)
{
    Fx f;
    fx_init(&f);
    EngRootMotion d = {{9, 9, 9}, 9};
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.none, 0, 0.0F, 1.0F, false, &d));
    EXPECT_NEAR(d.translation[0], 0.0F, 1e-7F);
    EXPECT_NEAR(d.translation[2], 0.0F, 1e-7F);
    EXPECT_NEAR(d.yaw, 0.0F, 1e-7F);
    /* la racine animée de l'articulation 1 ne compte pas pour la racine 0 */
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.none, 1, 0.0F, 1.0F, false, &d));
    EXPECT_NEAR(d.translation[0], 5.0F, 1e-5F); /* mais elle compte si on la désigne */
}

UTEST(anim_rootmotion, looping_chains_the_end_to_the_start_over_any_number_of_cycles)
{
    Fx f;
    fx_init(&f);
    EngRootMotion d;
    /* 1,0 -> 1,4 : 0,2 s avant la fin + 0,2 s après le début = 2 x 0,25 m */
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 1.0F, 1.4F, true, &d));
    EXPECT_NEAR(d.translation[2], -0.5F, 1e-5F);
    /* trois cycles entiers */
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 0.5F, 0.5F + (3.0F * 1.2F), true, &d));
    EXPECT_NEAR(d.translation[2], -4.5F, 1e-4F);
    /* temps déjà déroulés (au-delà d'un cycle) : même résultat */
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 2.5F, 2.7F, true, &d));
    EXPECT_NEAR(d.translation[2], -0.25F, 1e-5F);
    /* pile sur un multiple de la durée */
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 0.0F, 2.4F, true, &d));
    EXPECT_NEAR(d.translation[2], -3.0F, 1e-4F);
    /* sans boucle, la même demande est bornée à un cycle */
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 1.0F, 1.4F, false, &d));
    EXPECT_NEAR(d.translation[2], -0.25F, 1e-5F);
}

UTEST(anim_rootmotion, the_delta_is_expressed_in_the_frame_the_character_starts_from)
{
    Fx f;
    fx_init(&f);
    EngRootMotion d;
    /* tournée de 90 degrés et allant vers -x : dans son repère, c'est « tout droit » */
    ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.face, 0, 0.0F, 1.0F, false, &d));
    EXPECT_NEAR(d.translation[0], 0.0F, 1e-5F);
    EXPECT_NEAR(d.translation[2], -1.0F, 1e-5F);
    EXPECT_NEAR(d.yaw, 0.0F, 1e-5F); /* le cap ne change pas dans ce clip */
}

UTEST(anim_rootmotion, compose_applies_the_second_motion_in_the_arrival_frame)
{
    const EngRootMotion a = {{0, 0, -1.0F}, PI_F / 2.0F};
    const EngRootMotion b = {{0, 0, -1.0F}, 0.0F};
    const EngRootMotion c = eng_rootmotion_compose(&a, &b);
    EXPECT_NEAR(c.translation[0], -1.0F, 1e-5F); /* tourné à gauche : avancer = vers -x */
    EXPECT_NEAR(c.translation[2], -1.0F, 1e-5F);
    EXPECT_NEAR(c.yaw, PI_F / 2.0F, 1e-5F);
    /* le cap est ramené dans (-pi ; pi] */
    const EngRootMotion d = {{0, 0, 0}, PI_F * 0.75F};
    const EngRootMotion e = eng_rootmotion_compose(&d, &d);
    EXPECT_NEAR(e.yaw, -PI_F * 0.5F, 1e-5F);
}

UTEST(anim_rootmotion, lerp_weights_the_translation_and_takes_the_short_way_round_for_the_heading)
{
    const EngRootMotion a = {{0, 0, -2.0F}, glm_rad(170.0F)};
    const EngRootMotion b = {{2.0F, 0, 0.0F}, glm_rad(-170.0F)};
    EngRootMotion m = eng_rootmotion_lerp(&a, &b, 0.5F);
    EXPECT_NEAR(m.translation[0], 1.0F, 1e-6F);
    EXPECT_NEAR(m.translation[2], -1.0F, 1e-6F);
    EXPECT_NEAR(fabsf(m.yaw), PI_F, 1e-4F); /* 180 degrés, pas 0 */
    m = eng_rootmotion_lerp(&a, &b, 0.0F);
    EXPECT_NEAR(m.translation[2], -2.0F, 1e-6F);
    m = eng_rootmotion_lerp(&a, &b, 1.0F);
    EXPECT_NEAR(m.translation[0], 2.0F, 1e-6F);
    m = eng_rootmotion_lerp(&a, &b, 7.0F); /* borné */
    EXPECT_NEAR(m.translation[0], 2.0F, 1e-6F);
    m = eng_rootmotion_lerp(&a, &b, NAN);
    EXPECT_NEAR(m.translation[2], -2.0F, 1e-6F);
    m = eng_rootmotion_lerp(&a, &b, -3.0F);
    EXPECT_NEAR(m.translation[2], -2.0F, 1e-6F);
}

UTEST(anim_rootmotion, delta_refuses_invalid_arguments_and_leaves_the_output_alone)
{
    Fx f;
    fx_init(&f);
    EngRootMotion d = {{7, 7, 7}, 7};
    EXPECT_FALSE(eng_rootmotion_delta(NULL, &f.walk, 0, 0.0F, 1.0F, false, &d));
    EXPECT_FALSE(eng_rootmotion_delta(&f.sk, NULL, 0, 0.0F, 1.0F, false, &d));
    EXPECT_FALSE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 0.0F, 1.0F, false, NULL));
    EXPECT_FALSE(
        eng_rootmotion_delta(&f.sk, &f.walk, 3, 0.0F, 1.0F, false, &d)); /* hors squelette */
    EXPECT_FALSE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 1.0F, 0.5F, false, &d)); /* t1 < t0 */
    EXPECT_FALSE(eng_rootmotion_delta(&f.sk, &f.walk, 0, NAN, 1.0F, false, &d));
    EXPECT_FALSE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 0.0F, INFINITY, false, &d));
    EngAnimClip zero = f.walk;
    zero.duration = 0.0F;
    EXPECT_FALSE(
        eng_rootmotion_delta(&f.sk, &zero, 0, 0.0F, 1.0F, true, &d)); /* boucle impossible */
    EXPECT_TRUE(eng_rootmotion_delta(&f.sk, &zero, 0, 0.0F, 1.0F, false, &d));
    EngAnimClip nan_clip = f.walk;
    nan_clip.duration = NAN;
    EXPECT_FALSE(eng_rootmotion_delta(&f.sk, &nan_clip, 0, 0.0F, 1.0F, false, &d));
    EngSkeleton no_joints = {.joint_count = 3, .joints = NULL};
    EXPECT_FALSE(eng_rootmotion_delta(&no_joints, &f.walk, 0, 0.0F, 1.0F, false, &d));
    d = (EngRootMotion){{7, 7, 7}, 7};
    EXPECT_FALSE(eng_rootmotion_delta(&f.sk, &f.walk, 0, 1.0F, 0.5F, false, &d));
    EXPECT_NEAR(d.translation[0], 7.0F, 1e-7F); /* inchangé */
}

static EngPose make_pose(EngArena *arena, const EngSkeleton *sk)
{
    EngPose p;
    const bool ok = eng_pose_init(arena, sk, &p);
    (void)ok;
    return p;
}

UTEST(anim_rootmotion, strip_locks_the_horizontal_translation_and_heading_but_keeps_the_rest)
{
    Fx f;
    fx_init(&f);
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "rm", (size_t)1 << 14));
    EngPose pose = make_pose(&arena, &f.sk);
    ASSERT_TRUE(eng_pose_sample(&pose, &f.sk, &f.walk, 0.6F, ENG_ANIM_WRAP_CLAMP));
    EXPECT_NEAR(pose.translation[2], -0.75F, 1e-5F); /* avant : la racine a avancé */
    ASSERT_TRUE(eng_rootmotion_strip(&pose, &f.sk, &f.walk, 0));
    EXPECT_NEAR(pose.translation[0], 0.0F, 1e-7F);
    EXPECT_NEAR(pose.translation[2], 0.0F, 1e-7F); /* en place */
    EXPECT_NEAR(pose.translation[1], 0.1F, 1e-5F); /* le balancement vertical reste */
    /* les autres articulations ne sont pas touchées */
    EXPECT_NEAR(pose.translation[4], 1.0F, 1e-7F);

    /* cap : 40 degrés de lacet + 20 de tangage -> le lacet part, le tangage reste */
    const float yaw = glm_rad(40.0F);
    const float pitch = glm_rad(20.0F);
    const float qy[4] = {0, sinf(yaw / 2), 0, cosf(yaw / 2)};
    const float qx[4] = {sinf(pitch / 2), 0, 0, cosf(pitch / 2)};
    float q[4];
    q[0] = (qy[3] * qx[0]) + (qy[0] * qx[3]) + (qy[1] * qx[2]) - (qy[2] * qx[1]);
    q[1] = (qy[3] * qx[1]) - (qy[0] * qx[2]) + (qy[1] * qx[3]) + (qy[2] * qx[0]);
    q[2] = (qy[3] * qx[2]) + (qy[0] * qx[1]) - (qy[1] * qx[0]) + (qy[2] * qx[3]);
    q[3] = (qy[3] * qx[3]) - (qy[0] * qx[0]) - (qy[1] * qx[1]) - (qy[2] * qx[2]);
    memcpy(pose.rotation, q, sizeof q);
    ASSERT_TRUE(eng_rootmotion_strip(&pose, &f.sk, &f.walk, 0));
    EXPECT_NEAR(pose.rotation[0], sinf(pitch / 2), 1e-4F);
    EXPECT_NEAR(pose.rotation[1], 0.0F, 1e-4F);
    EXPECT_NEAR(pose.rotation[2], 0.0F, 1e-4F);
    EXPECT_NEAR(pose.rotation[3], cosf(pitch / 2), 1e-4F);

    /* un clip qui démarre tourné de 90 degrés : on verrouille sur CE cap */
    ASSERT_TRUE(eng_pose_sample(&pose, &f.sk, &f.face, 0.5F, ENG_ANIM_WRAP_CLAMP));
    ASSERT_TRUE(eng_rootmotion_strip(&pose, &f.sk, &f.face, 0));
    EXPECT_NEAR(pose.translation[0], 0.0F, 1e-6F); /* départ du clip : x = 0 */
    EXPECT_NEAR(eng_rootmotion_yaw(pose.rotation), PI_F / 2.0F, 1e-4F);
    /* un clip sans canal de racine : on verrouille sur le repos */
    pose.translation[0] = 4.0F;
    pose.translation[2] = 3.0F;
    ASSERT_TRUE(eng_rootmotion_strip(&pose, &f.sk, &f.none, 0));
    EXPECT_NEAR(pose.translation[0], 0.0F, 1e-7F);
    EXPECT_NEAR(pose.translation[2], 0.0F, 1e-7F);
    EXPECT_NEAR(eng_rootmotion_yaw(pose.rotation), 0.0F, 1e-5F);
    eng_arena_shutdown(&arena);
}

UTEST(anim_rootmotion, strip_refuses_invalid_arguments)
{
    Fx f;
    fx_init(&f);
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "rm", (size_t)1 << 14));
    EngPose pose = make_pose(&arena, &f.sk);
    EXPECT_FALSE(eng_rootmotion_strip(NULL, &f.sk, &f.walk, 0));
    EXPECT_FALSE(eng_rootmotion_strip(&pose, NULL, &f.walk, 0));
    EXPECT_FALSE(eng_rootmotion_strip(&pose, &f.sk, NULL, 0));
    EXPECT_FALSE(eng_rootmotion_strip(&pose, &f.sk, &f.walk, 3));
    EngPose small = pose;
    small.joint_count = 2;
    EXPECT_FALSE(eng_rootmotion_strip(&small, &f.sk, &f.walk, 0));
    eng_arena_shutdown(&arena);
}

UTEST(anim_rootmotion, stripped_pose_plus_accumulated_delta_reproduces_the_original_root_path)
{
    Fx f;
    fx_init(&f);
    /* en cumulant les deltas image par image on retrouve la position de la racine du clip */
    EngRootMotion total = {{0, 0, 0}, 0};
    float t = 0.0F;
    for (int i = 0; i < 24; i++) {
        EngRootMotion d;
        ASSERT_TRUE(eng_rootmotion_delta(&f.sk, &f.walk, 0, t, t + 0.05F, false, &d));
        total = eng_rootmotion_compose(&total, &d);
        t += 0.05F;
    }
    EXPECT_NEAR(total.translation[2], -1.5F, 1e-4F);
    EXPECT_NEAR(total.translation[0], 0.0F, 1e-5F);
}
