/* Mélange de poses et fondu enchaîné (M4-17). Chaîne de 3 articulations ; clip A : l'articulation 1
 * tourne de 0 à 90 degrés autour de Z en 1 s ; clip B : 90 degrés constants, et la racine à
 * (2, 0, 0). Position de l'articulation 2 pour une racine en (x, 0, 0) et un angle t :
 * (x - sin t, 1 + cos t, 0). */
#include <math.h>
#include <string.h>

#include "engine/anim_blend.h"
#include "utest.h"

#define H 0.70710678F

static EngJoint g_joints[3] = {
    {-1, {0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, "r"},
    {0, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1}, "a"},
    {1, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -2, 0, 1}, "b"},
};
/* A */
static EngAnimChannel g_a_channels[1] = {{1, ENG_ANIM_ROTATION, 2, 0}};
static float g_a_times[2] = {0.0F, 1.0F};
static float g_a_values[8] = {0, 0, 0, 1, 0, 0, H, H};
static uint32_t g_a_first[1] = {0};
/* B */
static EngAnimChannel g_b_channels[2] = {{0, ENG_ANIM_TRANSLATION, 2, 0},
                                         {1, ENG_ANIM_ROTATION, 2, 2}};
static float g_b_times[4] = {0.0F, 1.0F, 0.0F, 1.0F};
static float g_b_values[14] = {2, 0, 0, 2, 0, 0, 0, 0, H, H, 0, 0, H, H};
static uint32_t g_b_first[2] = {0, 6};

typedef struct Fx {
    EngArena arena;
    EngSkeleton sk;
    EngAnimClip a;
    EngAnimClip b;
    EngCrossfade cf;
} Fx;

static bool fx_init(Fx *f)
{
    f->sk = (EngSkeleton){.joint_count = 3, .joints = g_joints};
    f->a = (EngAnimClip){.duration = 1.0F,
                         .channel_count = 1,
                         .channels = g_a_channels,
                         .key_total = 2,
                         .times = g_a_times,
                         .values = g_a_values,
                         .value_first = g_a_first,
                         .skeleton_path = "p.skel"};
    f->b = (EngAnimClip){.duration = 1.0F,
                         .channel_count = 2,
                         .channels = g_b_channels,
                         .key_total = 4,
                         .times = g_b_times,
                         .values = g_b_values,
                         .value_first = g_b_first,
                         .skeleton_path = "p.skel"};
    return eng_arena_init(&f->arena, "blend", (size_t)1 << 16) &&
           eng_crossfade_init(&f->cf, &f->arena, &f->sk);
}

/* position monde de l'articulation 2 */
static void tip(const Fx *f, float out[3])
{
    mat4 world[3];
    (void)(eng_pose_world(&f->cf.pose, &f->sk, world));
    out[0] = world[2][3][0];
    out[1] = world[2][3][1];
    out[2] = world[2][3][2];
}

UTEST(anim_blend, curve_is_a_smoothstep_with_zero_slope_at_both_ends)
{
    EXPECT_NEAR(eng_crossfade_curve(0.0F), 0.0F, 1e-7F);
    EXPECT_NEAR(eng_crossfade_curve(1.0F), 1.0F, 1e-7F);
    EXPECT_NEAR(eng_crossfade_curve(0.5F), 0.5F, 1e-7F);
    EXPECT_NEAR(eng_crossfade_curve(0.25F), 0.15625F, 1e-6F);
    EXPECT_NEAR(eng_crossfade_curve(-3.0F), 0.0F, 1e-7F);
    EXPECT_NEAR(eng_crossfade_curve(7.0F), 1.0F, 1e-7F);
    EXPECT_NEAR(eng_crossfade_curve(NAN), 0.0F, 1e-7F);
    EXPECT_LT(eng_crossfade_curve(0.01F), 0.0003F); /* départ tangent à 0 */
    EXPECT_GT(eng_crossfade_curve(0.99F), 0.9997F); /* arrivée tangente à 1 */
    float prev = 0.0F;
    for (int i = 1; i <= 100; i++) {
        const float v = eng_crossfade_curve((float)i / 100.0F);
        EXPECT_GE(v, prev);
        prev = v;
    }
}

static EngPose make_pose(EngArena *arena, const EngSkeleton *sk)
{
    EngPose p;
    const bool ok = eng_pose_init(arena, sk, &p);
    (void)ok;
    return p;
}

UTEST(anim_blend, blend_interpolates_linearly_and_takes_the_short_way_round)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngPose a = make_pose(&f.arena, &f.sk);
    EngPose b = make_pose(&f.arena, &f.sk);
    EngPose out = make_pose(&f.arena, &f.sk);
    a.translation[0] = 0.0F;
    b.translation[0] = 4.0F;
    a.scale[3] = 1.0F;
    b.scale[3] = 3.0F;
    /* quaternions opposés = même rotation : le résultat doit rester cette rotation, pas s'annuler
     */
    b.rotation[3] = -1.0F;
    ASSERT_TRUE(eng_pose_blend(&out, &a, &b, 0.25F));
    EXPECT_NEAR(out.translation[0], 1.0F, 1e-6F);
    EXPECT_NEAR(out.scale[3], 1.5F, 1e-6F);
    EXPECT_NEAR(fabsf(out.rotation[3]), 1.0F, 1e-6F);
    ASSERT_TRUE(eng_pose_blend(&out, &a, &b, 0.5F));
    EXPECT_NEAR(fabsf(out.rotation[3]), 1.0F, 1e-6F);
    /* 0 et 1 rendent exactement a et b (translation) */
    ASSERT_TRUE(eng_pose_blend(&out, &a, &b, 0.0F));
    EXPECT_NEAR(out.translation[0], 0.0F, 1e-7F);
    ASSERT_TRUE(eng_pose_blend(&out, &a, &b, 1.0F));
    EXPECT_NEAR(out.translation[0], 4.0F, 1e-7F);
    /* poids hors plage ou NaN : borné */
    ASSERT_TRUE(eng_pose_blend(&out, &a, &b, 9.0F));
    EXPECT_NEAR(out.translation[0], 4.0F, 1e-7F);
    ASSERT_TRUE(eng_pose_blend(&out, &a, &b, -9.0F));
    EXPECT_NEAR(out.translation[0], 0.0F, 1e-7F);
    ASSERT_TRUE(eng_pose_blend(&out, &a, &b, NAN));
    EXPECT_NEAR(out.translation[0], 0.0F, 1e-7F);
    /* rotation à 90 degrés / 0 : milieu à 45 degrés (même axe), unitaire */
    a.rotation[4 * 1U + 0] = 0.0F;
    a.rotation[4 * 1U + 1] = 0.0F;
    a.rotation[4 * 1U + 2] = 0.0F;
    a.rotation[4 * 1U + 3] = 1.0F;
    b.rotation[4 * 1U + 0] = 0.0F;
    b.rotation[4 * 1U + 1] = 0.0F;
    b.rotation[4 * 1U + 2] = H;
    b.rotation[4 * 1U + 3] = H;
    ASSERT_TRUE(eng_pose_blend(&out, &a, &b, 0.5F));
    const float s = sinf(glm_rad(22.5F));
    EXPECT_NEAR(out.rotation[4 * 1U + 2], s, 1e-5F);
    EXPECT_NEAR(out.rotation[4 * 1U + 3], cosf(glm_rad(22.5F)), 1e-5F);
    /* le résultat peut écraser a */
    ASSERT_TRUE(eng_pose_blend(&a, &a, &b, 1.0F));
    EXPECT_NEAR(a.translation[0], 4.0F, 1e-7F);
    /* quaternions nuls : pas de NaN */
    memset(a.rotation, 0, sizeof(float) * 12);
    memset(b.rotation, 0, sizeof(float) * 12);
    ASSERT_TRUE(eng_pose_blend(&out, &a, &b, 0.5F));
    for (int i = 0; i < 12; i++) {
        EXPECT_TRUE(isfinite(out.rotation[i]));
    }
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, blend_refuses_missing_or_mismatched_poses)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngPose a = make_pose(&f.arena, &f.sk);
    EngPose out = make_pose(&f.arena, &f.sk);
    EngPose small = a;
    small.joint_count = 2;
    EngPose empty;
    memset(&empty, 0, sizeof empty);
    EXPECT_FALSE(eng_pose_blend(NULL, &a, &a, 0.5F));
    EXPECT_FALSE(eng_pose_blend(&out, NULL, &a, 0.5F));
    EXPECT_FALSE(eng_pose_blend(&out, &a, NULL, 0.5F));
    EXPECT_FALSE(eng_pose_blend(&out, &a, &small, 0.5F));
    EXPECT_FALSE(eng_pose_blend(&small, &a, &a, 0.5F));
    EXPECT_FALSE(eng_pose_blend(&empty, &a, &a, 0.5F));
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, init_starts_at_rest_and_refuses_bad_arguments)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EXPECT_FALSE(eng_crossfade_active(&f.cf));
    EXPECT_NEAR(eng_crossfade_weight(&f.cf), 1.0F, 1e-7F);
    float p[3];
    tip(&f, p);
    EXPECT_NEAR(p[1], 2.0F, 1e-6F);                 /* repos : (0, 2, 0) */
    EXPECT_TRUE(eng_crossfade_update(&f.cf, 0.1F)); /* sans clip : sans effet */
    EngCrossfade c;
    EXPECT_FALSE(eng_crossfade_init(NULL, &f.arena, &f.sk));
    EXPECT_FALSE(eng_crossfade_init(&c, NULL, &f.sk));
    EXPECT_FALSE(eng_crossfade_init(&c, &f.arena, NULL));
    EngArena tiny;
    ASSERT_TRUE(eng_arena_init(&tiny, "tiny", 200));
    EXPECT_FALSE(eng_crossfade_init(&c, &tiny, &f.sk));
    EXPECT_EQ(tiny.used, 0U);
    eng_arena_shutdown(&tiny);
    EXPECT_FALSE(eng_crossfade_play(NULL, &f.a, 0.1F, true, 1.0F, 0.0F));
    EXPECT_FALSE(eng_crossfade_update(NULL, 0.1F));
    EXPECT_FALSE(eng_crossfade_active(NULL));
    EXPECT_NEAR(eng_crossfade_weight(NULL), 1.0F, 1e-7F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, the_first_clip_starts_at_once_without_fading_from_nothing)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.5F, false, 1.0F, 0.5F));
    EXPECT_FALSE(eng_crossfade_active(&f.cf));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.0F));
    float p[3];
    tip(&f, p);
    EXPECT_NEAR(p[0], -H, 1e-5F); /* 45 degrés d'emblée */
    EXPECT_NEAR(p[1], 1.0F + H, 1e-5F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, a_fade_starts_on_the_old_pose_crosses_at_half_and_ends_on_the_new_one)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, false, 1.0F, 0.0F));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.0F));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.b, 1.0F, false, 1.0F, 0.0F));
    EXPECT_TRUE(eng_crossfade_active(&f.cf));
    float p[3];
    /* à l'instant du changement : exactement l'ancienne pose (A à 0 : (0, 2, 0)) */
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.0F));
    tip(&f, p);
    EXPECT_NEAR(p[0], 0.0F, 1e-5F);
    EXPECT_NEAR(p[1], 2.0F, 1e-5F);
    EXPECT_NEAR(eng_crossfade_weight(&f.cf), 0.0F, 1e-7F);
    /* à mi-fondu : A à 0,5 s (45 degrés), B à 90 degrés, racine à mi-chemin (1, 0, 0) ; poids 0,5
     * donc 67,5 degrés */
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.5F));
    EXPECT_NEAR(eng_crossfade_weight(&f.cf), 0.5F, 1e-6F);
    tip(&f, p);
    EXPECT_NEAR(p[0], 1.0F - sinf(glm_rad(67.5F)), 1e-4F);
    EXPECT_NEAR(p[1], 1.0F + cosf(glm_rad(67.5F)), 1e-4F);
    /* fin : exactement B, et le fondu est terminé */
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.5F));
    EXPECT_FALSE(eng_crossfade_active(&f.cf));
    EXPECT_NEAR(eng_crossfade_weight(&f.cf), 1.0F, 1e-7F);
    tip(&f, p);
    EXPECT_NEAR(p[0], 1.0F, 1e-5F);
    EXPECT_NEAR(p[1], 1.0F, 1e-5F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, the_old_clip_keeps_playing_while_it_fades_out)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, true, 1.0F, 0.0F));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.b, 0.5F, true, 1.0F, 0.0F));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.25F));
    EXPECT_NEAR(f.cf.from.time, 0.25F, 1e-6F);
    EXPECT_NEAR(f.cf.to.time, 0.25F, 1e-6F);
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.25F));
    EXPECT_FALSE(eng_crossfade_active(&f.cf));
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, a_fade_has_no_visible_jerk_at_60_hz)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, true, 1.0F, 0.0F));
    for (int i = 0; i < 30; i++) {
        ASSERT_TRUE(eng_crossfade_update(&f.cf, 1.0F / 60.0F));
    }
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.b, 0.3F, true, 1.0F, 0.0F));
    float prev[3];
    tip(&f, prev);
    float first_step = -1.0F;
    float last_step = 0.0F;
    float max_step = 0.0F;
    int steps_in_fade = 0;
    for (int i = 0; i < 40; i++) {
        const bool was_active = eng_crossfade_active(&f.cf);
        ASSERT_TRUE(eng_crossfade_update(&f.cf, 1.0F / 60.0F));
        float p[3];
        tip(&f, p);
        const float d =
            sqrtf(((p[0] - prev[0]) * (p[0] - prev[0])) + ((p[1] - prev[1]) * (p[1] - prev[1])) +
                  ((p[2] - prev[2]) * (p[2] - prev[2])));
        if (first_step < 0.0F) {
            first_step = d;
        }
        if (was_active) {
            steps_in_fade++;
            last_step = d;
        }
        max_step = fmaxf(max_step, d);
        memcpy(prev, p, sizeof p);
    }
    EXPECT_EQ(steps_in_fade, 18); /* 0,3 s à 60 Hz */
    EXPECT_LT(first_step,
              0.04F); /* départ tangent : le mouvement propre du clip (~0,025 m), pas un saut */
    EXPECT_LT(last_step, 0.06F); /* arrivée tangente aussi */
    EXPECT_LT(max_step, 0.2F);   /* la distance A-B (~1,8 m) est répartie sur ~18 images */
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, restarting_during_a_fade_keeps_the_displayed_pose_continuous)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, false, 1.0F, 0.0F));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.b, 0.4F, false, 1.0F, 0.0F));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.15F));
    EXPECT_TRUE(eng_crossfade_active(&f.cf));
    float before[3];
    tip(&f, before);
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.4F, false, 1.0F, 0.0F));
    EXPECT_TRUE(f.cf.from_frozen);
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.0F));
    float after[3];
    tip(&f, after);
    EXPECT_NEAR(after[0], before[0], 1e-5F);
    EXPECT_NEAR(after[1], before[1], 1e-5F);
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.4F));
    EXPECT_FALSE(eng_crossfade_active(&f.cf));
    tip(&f, after); /* A à 0,4 s : 36 degrés, racine au repos */
    EXPECT_NEAR(after[0], -sinf(glm_rad(36.0F)), 1e-3F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, play_refuses_bad_clips_and_values_and_keeps_the_state)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, true, 1.0F, 0.0F));
    EngAnimChannel far[1] = {{3, ENG_ANIM_ROTATION, 2, 0}};
    EngAnimClip other = f.a;
    other.channels = far;
    EXPECT_FALSE(eng_crossfade_play(&f.cf, &other, 0.2F, true, 1.0F, 0.0F));
    EngAnimClip nan_clip = f.a;
    nan_clip.duration = NAN;
    EXPECT_FALSE(eng_crossfade_play(&f.cf, &nan_clip, 0.2F, true, 1.0F, 0.0F));
    EngAnimClip no_ch = f.a;
    no_ch.channels = NULL;
    EXPECT_FALSE(eng_crossfade_play(&f.cf, &no_ch, 0.2F, true, 1.0F, 0.0F));
    EXPECT_FALSE(eng_crossfade_play(&f.cf, NULL, 0.2F, true, 1.0F, 0.0F));
    EXPECT_FALSE(eng_crossfade_play(&f.cf, &f.b, NAN, true, 1.0F, 0.0F));
    EXPECT_FALSE(eng_crossfade_play(&f.cf, &f.b, -0.1F, true, 1.0F, 0.0F));
    EXPECT_FALSE(eng_crossfade_play(&f.cf, &f.b, 0.2F, true, INFINITY, 0.0F));
    EXPECT_FALSE(eng_crossfade_play(&f.cf, &f.b, 0.2F, true, 1.0F, NAN));
    EXPECT_TRUE(f.cf.to.clip == &f.a);
    EXPECT_FALSE(eng_crossfade_active(&f.cf));
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, a_zero_second_fade_switches_at_once)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, true, 1.0F, 0.0F));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.b, 0.0F, true, 1.0F, 0.0F));
    EXPECT_FALSE(eng_crossfade_active(&f.cf));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.0F));
    float p[3];
    tip(&f, p);
    EXPECT_NEAR(p[0], 1.0F, 1e-5F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, time_loops_or_stops_at_the_end_and_ignores_bad_steps)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, true, 1.0F, 0.9F));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.3F));
    EXPECT_NEAR(f.cf.to.time, 0.2F, 1e-5F);
    EXPECT_FALSE(f.cf.finished);
    ASSERT_TRUE(eng_crossfade_update(&f.cf, -1.0F));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, NAN));
    EXPECT_NEAR(f.cf.to.time, 0.2F, 1e-5F);

    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, false, 1.0F, 0.9F));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.3F));
    EXPECT_NEAR(f.cf.to.time, 1.0F, 1e-7F);
    EXPECT_TRUE(f.cf.finished);
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.3F));
    EXPECT_TRUE(f.cf.finished);
    /* en arrière sans boucle : s'arrête à 0 */
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, false, -1.0F, 0.1F));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.3F));
    EXPECT_NEAR(f.cf.to.time, 0.0F, 1e-7F);
    EXPECT_TRUE(f.cf.finished);
    /* vitesse nulle : gelé */
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, true, 0.0F, 0.4F));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 5.0F));
    EXPECT_NEAR(f.cf.to.time, 0.4F, 1e-7F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, nothing_is_allocated_after_init)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    const size_t used = f.arena.used;
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, true, 1.0F, 0.0F));
    for (int i = 0; i < 200; i++) {
        if (i % 20 == 0) {
            ASSERT_TRUE(eng_crossfade_play(&f.cf, (i / 20) % 2 != 0 ? &f.a : &f.b, 0.25F, true,
                                           1.0F, 0.0F));
        }
        ASSERT_TRUE(eng_crossfade_update(&f.cf, 1.0F / 60.0F));
    }
    EXPECT_EQ(f.arena.used, used);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, every_quaternion_component_takes_part_in_the_shortest_path_choice)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngPose a = make_pose(&f.arena, &f.sk);
    EngPose b = make_pose(&f.arena, &f.sk);
    EngPose out = make_pose(&f.arena, &f.sk);
    for (uint32_t axis = 0; axis < 4; axis++) {
        const float qa[4] = {0.5F, 0.5F, 0.5F, 0.5F};
        memcpy(a.rotation, qa, sizeof qa);
        float qb[4] = {0.0F, 0.0F, 0.0F, 0.0F};
        qb[axis] = -1.0F; /* a . b = -0,5 : b est retourné */
        memcpy(b.rotation, qb, sizeof qb);
        ASSERT_TRUE(eng_pose_blend(&out, &a, &b, 1.0F));
        for (uint32_t c = 0; c < 4; c++) {
            EXPECT_NEAR(out.rotation[c], c == axis ? 1.0F : 0.0F, 1e-6F);
        }
    }
    /* quaternions orthogonaux : produit scalaire nul, pas de retournement */
    const float oa[4] = {1.0F, 0.0F, 0.0F, 0.0F};
    const float ob[4] = {0.0F, 1.0F, 0.0F, 0.0F};
    memcpy(a.rotation, oa, sizeof oa);
    memcpy(b.rotation, ob, sizeof ob);
    ASSERT_TRUE(eng_pose_blend(&out, &a, &b, 1.0F));
    EXPECT_NEAR(out.rotation[1], 1.0F, 1e-6F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, freezing_the_displayed_pose_copies_every_joint_and_property)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, false, 1.0F, 0.0F));
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.b, 0.4F, false, 1.0F, 0.0F));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.15F));
    /* on met des valeurs reconnaissables partout dans la pose affichée */
    for (uint32_t i = 0; i < 9; i++) {
        f.cf.pose.translation[i] = 10.0F + (float)i;
        f.cf.pose.scale[i] = 20.0F + (float)i;
    }
    for (uint32_t i = 0; i < 12; i++) {
        f.cf.pose.rotation[i] = 30.0F + (float)i;
    }
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.4F, false, 1.0F, 0.0F));
    for (uint32_t i = 0; i < 9; i++) {
        EXPECT_NEAR(f.cf.snapshot.translation[i], 10.0F + (float)i, 1e-7F);
        EXPECT_NEAR(f.cf.snapshot.scale[i], 20.0F + (float)i, 1e-7F);
    }
    for (uint32_t i = 0; i < 12; i++) {
        EXPECT_NEAR(f.cf.snapshot.rotation[i], 30.0F + (float)i, 1e-7F);
    }
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, a_zero_length_clip_is_accepted_and_the_end_is_detected_exactly)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngAnimClip zero = f.a;
    zero.duration = 0.0F;
    EXPECT_TRUE(eng_crossfade_play(&f.cf, &zero, 0.0F, false, 1.0F, 0.0F));
    /* arrivée pile sur la durée : fini */
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, false, 1.0F, 0.5F));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.5F));
    EXPECT_TRUE(f.cf.finished);
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, false, -1.0F, 0.5F));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.5F));
    EXPECT_TRUE(f.cf.finished);
    ASSERT_TRUE(eng_crossfade_play(&f.cf, &f.a, 0.0F, false, 1.0F, 0.25F));
    ASSERT_TRUE(eng_crossfade_update(&f.cf, 0.25F));
    EXPECT_FALSE(f.cf.finished);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blend, a_layer_advances_on_its_own_and_reports_the_end)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f));
    EngAnimLayer l = {.clip = &f.a, .time = 0.5F, .speed = 1.0F, .loop = false};
    EXPECT_FALSE(eng_anim_layer_advance(&l, 0.25F));
    EXPECT_NEAR(l.time, 0.75F, 1e-6F);
    EXPECT_TRUE(eng_anim_layer_advance(&l, 0.5F));
    EXPECT_NEAR(l.time, 1.0F, 1e-7F);
    l.loop = true;
    EXPECT_FALSE(eng_anim_layer_advance(&l, 0.5F)); /* boucle : jamais « fini » */
    EXPECT_NEAR(l.time, 0.5F, 1e-6F);
    EXPECT_FALSE(eng_anim_layer_advance(NULL, 0.1F));
    EngAnimLayer empty = {0};
    EXPECT_FALSE(eng_anim_layer_advance(&empty, 0.1F));
    eng_arena_shutdown(&f.arena);
}
