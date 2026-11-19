/* Espace de mélange 1D (M4-18) : repos / marche / course sur une chaîne de 3 articulations.
 * I (repos, 2 s) : racine à (0, 0, 5). W (marche, 1,2 s) : l'articulation 1 tourne de 0 à 90
 * degrés. R (course, 0,6 s) : même rotation, racine à (3, 0, 0). Positions : 0, 1,5 et 4,5 m/s.
 * Comme W et R font la même rotation sur leur cycle, une lecture synchronisée donne l'angle 90 x
 * phase quel que soit le paramètre entre 1,5 et 4,5. */
#include <math.h>
#include <string.h>

#include "engine/anim_blendspace.h"
#include "utest.h"

#define H 0.70710678F

static EngJoint g_joints[3] = {
    {-1, {0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, "r"},
    {0, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1}, "a"},
    {1, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -2, 0, 1}, "b"},
};
static EngAnimChannel g_i_ch[1] = {{0, ENG_ANIM_TRANSLATION, 2, 0}};
static float g_i_t[2] = {0.0F, 2.0F};
static float g_i_v[6] = {0, 0, 5, 0, 0, 5};
static uint32_t g_i_f[1] = {0};
static EngAnimChannel g_w_ch[1] = {{1, ENG_ANIM_ROTATION, 2, 0}};
static float g_w_t[2] = {0.0F, 1.2F};
static float g_w_v[8] = {0, 0, 0, 1, 0, 0, H, H};
static uint32_t g_w_f[1] = {0};
static EngAnimChannel g_r_ch[2] = {{0, ENG_ANIM_TRANSLATION, 2, 0}, {1, ENG_ANIM_ROTATION, 2, 2}};
static float g_r_t[4] = {0.0F, 0.6F, 0.0F, 0.6F};
static float g_r_v[14] = {3, 0, 0, 3, 0, 0, 0, 0, 0, 1, 0, 0, H, H};
static uint32_t g_r_f[2] = {0, 6};

typedef struct Fx {
    EngArena arena;
    EngSkeleton sk;
    EngAnimClip idle;
    EngAnimClip walk;
    EngAnimClip run;
    EngBlendSpace1D bs;
} Fx;

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

static bool fx_init(Fx *f, bool fill)
{
    f->sk = (EngSkeleton){.joint_count = 3, .joints = g_joints};
    f->idle = make_clip(2.0F, 1, g_i_ch, 2, g_i_t, g_i_v, g_i_f);
    f->walk = make_clip(1.2F, 1, g_w_ch, 2, g_w_t, g_w_v, g_w_f);
    f->run = make_clip(0.6F, 2, g_r_ch, 4, g_r_t, g_r_v, g_r_f);
    if (!eng_arena_init(&f->arena, "bs", (size_t)1 << 16) ||
        !eng_blendspace_init(&f->bs, &f->arena, &f->sk)) {
        return false;
    }
    return !fill || (eng_blendspace_add(&f->bs, &f->idle, 0.0F) &&
                     eng_blendspace_add(&f->bs, &f->walk, 1.5F) &&
                     eng_blendspace_add(&f->bs, &f->run, 4.5F));
}

static void tip(const Fx *f, float out[3])
{
    mat4 world[3];
    const bool ok = eng_pose_world(&f->bs.pose, &f->sk, world);
    (void)ok;
    out[0] = world[2][3][0];
    out[1] = world[2][3][1];
    out[2] = world[2][3][2];
}

UTEST(anim_blendspace, add_requires_increasing_positions_and_a_valid_clip)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, false));
    EXPECT_TRUE(eng_blendspace_add(&f.bs, &f.walk, 1.5F));
    EXPECT_FALSE(eng_blendspace_add(&f.bs, &f.run, 1.5F)); /* même position */
    EXPECT_FALSE(eng_blendspace_add(&f.bs, &f.run, 1.0F)); /* décroissante */
    EXPECT_FALSE(eng_blendspace_add(&f.bs, &f.run, NAN));
    EXPECT_FALSE(eng_blendspace_add(&f.bs, NULL, 2.0F));
    EngAnimClip zero = f.run;
    zero.duration = 0.0F;
    EXPECT_FALSE(eng_blendspace_add(&f.bs, &zero, 2.0F)); /* durée nulle : phase impossible */
    zero.duration = NAN;
    EXPECT_FALSE(eng_blendspace_add(&f.bs, &zero, 2.0F));
    EngAnimChannel far[1] = {{3, ENG_ANIM_ROTATION, 2, 0}};
    EngAnimClip other = f.run;
    other.channels = far;
    EXPECT_FALSE(eng_blendspace_add(&f.bs, &other, 2.0F));
    other.channels = NULL;
    EXPECT_FALSE(eng_blendspace_add(&f.bs, &other, 2.0F));
    EXPECT_EQ(f.bs.count, 1U);
    EXPECT_TRUE(eng_blendspace_add(&f.bs, &f.run, 4.5F));
    for (uint32_t i = 2; i < ENG_BLENDSPACE_MAX_SAMPLES; i++) {
        EXPECT_TRUE(eng_blendspace_add(&f.bs, &f.run, 5.0F + (float)i));
    }
    EXPECT_FALSE(eng_blendspace_add(&f.bs, &f.run, 100.0F)); /* complet */
    EXPECT_FALSE(eng_blendspace_add(NULL, &f.run, 1.0F));
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blendspace, init_refuses_bad_arguments_and_restores_the_arena)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, false));
    EngBlendSpace1D b;
    EXPECT_FALSE(eng_blendspace_init(NULL, &f.arena, &f.sk));
    EXPECT_FALSE(eng_blendspace_init(&b, NULL, &f.sk));
    EXPECT_FALSE(eng_blendspace_init(&b, &f.arena, NULL));
    EngArena tiny;
    ASSERT_TRUE(eng_arena_init(&tiny, "tiny", 100));
    EXPECT_FALSE(eng_blendspace_init(&b, &tiny, &f.sk));
    EXPECT_EQ(tiny.used, 0U);
    eng_arena_shutdown(&tiny);
    EXPECT_FALSE(eng_blendspace_update(&f.bs, 0.1F, 1.0F)); /* aucun échantillon */
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blendspace, the_segment_follows_the_parameter_and_is_clamped_at_the_ends)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, true));
    uint32_t lo = 9;
    uint32_t hi = 9;
    float t = -1.0F;
    ASSERT_TRUE(eng_blendspace_segment(&f.bs, -3.0F, &lo, &hi, &t));
    EXPECT_EQ(lo, 0U);
    EXPECT_EQ(hi, 0U);
    EXPECT_NEAR(t, 0.0F, 1e-7F);
    ASSERT_TRUE(eng_blendspace_segment(&f.bs, 0.75F, &lo, &hi, &t));
    EXPECT_EQ(lo, 0U);
    EXPECT_EQ(hi, 1U);
    EXPECT_NEAR(t, 0.5F, 1e-6F);
    ASSERT_TRUE(eng_blendspace_segment(&f.bs, 1.5F, &lo, &hi, &t)); /* pile sur la marche */
    EXPECT_EQ(lo, 1U);
    EXPECT_EQ(hi, 2U);
    EXPECT_NEAR(t, 0.0F, 1e-7F);
    ASSERT_TRUE(eng_blendspace_segment(&f.bs, 3.0F, &lo, &hi, &t));
    EXPECT_EQ(lo, 1U);
    EXPECT_EQ(hi, 2U);
    EXPECT_NEAR(t, 0.5F, 1e-6F);
    ASSERT_TRUE(eng_blendspace_segment(&f.bs, 4.5F, &lo, &hi, &t));
    EXPECT_EQ(lo, 2U);
    EXPECT_EQ(hi, 2U);
    ASSERT_TRUE(eng_blendspace_segment(&f.bs, 99.0F, &lo, &hi, &t));
    EXPECT_EQ(lo, 2U);
    EXPECT_EQ(hi, 2U);
    EXPECT_NEAR(t, 0.0F, 1e-7F);
    ASSERT_TRUE(eng_blendspace_segment(&f.bs, NAN, &lo, &hi, &t));
    EXPECT_EQ(lo, 0U);
    EXPECT_FALSE(eng_blendspace_segment(&f.bs, 1.0F, NULL, &hi, &t));
    EXPECT_FALSE(eng_blendspace_segment(NULL, 1.0F, &lo, &hi, &t));
    /* un seul échantillon */
    Fx g;
    ASSERT_TRUE(fx_init(&g, false));
    EXPECT_FALSE(eng_blendspace_segment(&g.bs, 1.0F, &lo, &hi, &t));
    ASSERT_TRUE(eng_blendspace_add(&g.bs, &g.walk, 1.5F));
    ASSERT_TRUE(eng_blendspace_segment(&g.bs, 7.0F, &lo, &hi, &t));
    EXPECT_EQ(lo, 0U);
    EXPECT_EQ(hi, 0U);
    ASSERT_TRUE(eng_blendspace_segment(&g.bs, -7.0F, &lo, &hi, &t));
    EXPECT_EQ(hi, 0U);
    eng_arena_shutdown(&g.arena);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blendspace, the_cycle_duration_is_interpolated_between_the_two_clips)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, true));
    EXPECT_NEAR(eng_blendspace_cycle(&f.bs, 0.0F), 2.0F, 1e-6F);
    EXPECT_NEAR(eng_blendspace_cycle(&f.bs, 0.75F), 1.6F, 1e-6F);
    EXPECT_NEAR(eng_blendspace_cycle(&f.bs, 1.5F), 1.2F, 1e-6F);
    EXPECT_NEAR(eng_blendspace_cycle(&f.bs, 3.0F), 0.9F, 1e-6F);
    EXPECT_NEAR(eng_blendspace_cycle(&f.bs, 4.5F), 0.6F, 1e-6F);
    EXPECT_NEAR(eng_blendspace_cycle(&f.bs, 50.0F), 0.6F, 1e-6F);
    EXPECT_NEAR(eng_blendspace_cycle(NULL, 1.0F), 0.0F, 1e-7F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blendspace, clips_are_read_on_the_same_phase_whatever_the_parameter)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, true));
    const float params[4] = {1.5F, 2.2F, 3.0F, 4.5F};
    for (int k = 0; k < 4; k++) {
        ASSERT_TRUE(eng_blendspace_set_phase(&f.bs, 0.0F));
        ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, params[k]));
        /* un demi-cycle : phase 0,5, donc 45 degrés pour W comme pour R, et donc pour le mélange */
        const float half = 0.5F * eng_blendspace_cycle(&f.bs, params[k]);
        ASSERT_TRUE(eng_blendspace_update(&f.bs, half, params[k]));
        EXPECT_NEAR(f.bs.phase, 0.5F, 1e-5F);
        float p[3];
        tip(&f, p);
        /* le mélange de racines (0 -> 3) ne change pas la hauteur ni l'angle : y = 1 + cos 45 */
        EXPECT_NEAR(p[1], 1.0F + H, 1e-4F);
        EXPECT_NEAR(p[0] - f.bs.pose.translation[0], -H, 1e-4F);
    }
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blendspace, weights_blend_the_root_between_the_neighbouring_clips)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, true));
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, 0.0F));
    EXPECT_NEAR(f.bs.pose.translation[2], 5.0F, 1e-6F); /* repos pur */
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, 0.75F));
    EXPECT_NEAR(f.bs.pose.translation[2], 2.5F, 1e-6F); /* mi-repos mi-marche */
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, 1.5F));
    EXPECT_NEAR(f.bs.pose.translation[2], 0.0F, 1e-6F); /* marche pure */
    EXPECT_NEAR(f.bs.pose.translation[0], 0.0F, 1e-6F);
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, 3.0F));
    EXPECT_NEAR(f.bs.pose.translation[0], 1.5F, 1e-6F); /* mi-marche mi-course */
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, 4.5F));
    EXPECT_NEAR(f.bs.pose.translation[0], 3.0F, 1e-6F);
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, 80.0F)); /* borné : course pure */
    EXPECT_NEAR(f.bs.pose.translation[0], 3.0F, 1e-6F);
    EXPECT_NEAR(f.bs.parameter, 4.5F, 1e-7F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blendspace, the_phase_survives_a_change_of_parameter_and_the_pace_follows_the_cycle)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, true));
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.3F, 1.5F)); /* 0,3 / 1,2 = 0,25 */
    EXPECT_NEAR(f.bs.phase, 0.25F, 1e-6F);
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, 4.5F)); /* on passe à la course : même phase */
    EXPECT_NEAR(f.bs.phase, 0.25F, 1e-6F);
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.15F, 4.5F)); /* 0,15 / 0,6 = +0,25 */
    EXPECT_NEAR(f.bs.phase, 0.5F, 1e-6F);
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.6F, 4.5F)); /* un cycle entier : retour à 0,5 */
    EXPECT_NEAR(f.bs.phase, 0.5F, 1e-5F);
    ASSERT_TRUE(eng_blendspace_update(&f.bs, -1.0F, 4.5F)); /* dt ignoré */
    ASSERT_TRUE(eng_blendspace_update(&f.bs, NAN, 4.5F));
    EXPECT_NEAR(f.bs.phase, 0.5F, 1e-5F);
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, NAN)); /* paramètre NaN : on garde 4,5 */
    EXPECT_NEAR(f.bs.parameter, 4.5F, 1e-7F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blendspace, set_phase_wraps_and_refuses_non_finite_values)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, true));
    ASSERT_TRUE(eng_blendspace_set_phase(&f.bs, 2.25F));
    EXPECT_NEAR(f.bs.phase, 0.25F, 1e-6F);
    ASSERT_TRUE(eng_blendspace_set_phase(&f.bs, -0.25F));
    EXPECT_NEAR(f.bs.phase, 0.75F, 1e-6F);
    EXPECT_FALSE(eng_blendspace_set_phase(&f.bs, NAN));
    EXPECT_FALSE(eng_blendspace_set_phase(&f.bs, INFINITY));
    EXPECT_FALSE(eng_blendspace_set_phase(NULL, 0.5F));
    EXPECT_NEAR(f.bs.phase, 0.75F, 1e-6F);
    /* phase juste sous 1 : reste dans [0 ; 1[ */
    ASSERT_TRUE(eng_blendspace_set_phase(&f.bs, -1e-9F));
    EXPECT_LT(f.bs.phase, 1.0F);
    EXPECT_GE(f.bs.phase, 0.0F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blendspace, accelerating_from_walk_to_run_moves_smoothly)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, true));
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, 1.5F));
    float prev[3];
    tip(&f, prev);
    float max_step = 0.0F;
    for (int i = 1; i <= 180; i++) {
        const float param = 1.5F + (3.0F * (float)i / 180.0F);
        const float phase_before = f.bs.phase;
        ASSERT_TRUE(eng_blendspace_update(&f.bs, 1.0F / 60.0F, param));
        float p[3];
        tip(&f, p);
        const float d =
            sqrtf(((p[0] - prev[0]) * (p[0] - prev[0])) + ((p[1] - prev[1]) * (p[1] - prev[1])));
        /* les clips de ce test sont des rampes 0 -> 90 degrés : le retour à 0 au bouclage est un
         * saut du CLIP, pas du mélange ; on ne mesure donc pas l'image du bouclage */
        if (f.bs.phase >= phase_before) {
            max_step = fmaxf(max_step, d);
        }
        memcpy(prev, p, sizeof p);
    }
    EXPECT_LT(max_step, 0.12F); /* mouvement propre des clips + dérive de la racine (3 m en 3 s) */
    EXPECT_GT(max_step, 0.01F); /* et ça bouge bien */
    EXPECT_NEAR(f.bs.parameter, 4.5F, 1e-5F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blendspace, nothing_is_allocated_after_init)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, true));
    const size_t used = f.arena.used;
    for (int i = 0; i < 300; i++) {
        ASSERT_TRUE(eng_blendspace_update(&f.bs, 1.0F / 60.0F, (float)(i % 50) / 10.0F));
    }
    EXPECT_EQ(f.arena.used, used);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blendspace, a_first_update_without_a_valid_parameter_starts_on_the_first_sample)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, false));
    ASSERT_TRUE(eng_blendspace_add(&f.bs, &f.walk, 1.5F));
    ASSERT_TRUE(eng_blendspace_add(&f.bs, &f.run, 4.5F));
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, NAN));
    EXPECT_NEAR(f.bs.parameter, 1.5F, 1e-7F);
    EXPECT_NEAR(f.bs.pose.translation[0], 0.0F, 1e-7F); /* marche pure, pas de course */
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_blendspace, clear_empties_the_space_but_keeps_it_usable)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, true));
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.3F, 3.0F));
    EXPECT_GT(f.bs.phase, 0.0F);
    const size_t used = f.arena.used;
    eng_blendspace_clear(&f.bs);
    EXPECT_EQ(f.bs.count, 0U);
    EXPECT_NEAR(f.bs.phase, 0.0F, 1e-7F);
    EXPECT_NEAR(f.bs.parameter, 0.0F, 1e-7F);
    EXPECT_FALSE(eng_blendspace_update(&f.bs, 0.1F, 1.0F));
    EXPECT_TRUE(eng_blendspace_add(&f.bs, &f.run, 2.0F)); /* repart de zéro, positions libres */
    EXPECT_TRUE(eng_blendspace_add(&f.bs, &f.walk, 3.0F));
    EXPECT_EQ(f.arena.used, used);
    eng_blendspace_clear(NULL); /* sans effet */
    eng_arena_shutdown(&f.arena);
}

/* clips avec root motion : la racine avance de 1,5 m (marche, 1,2 s) et 2,7 m (course, 0,6 s) */
static EngAnimChannel g_wm_ch[1] = {{0, ENG_ANIM_TRANSLATION, 2, 0}};
static float g_wm_t[2] = {0.0F, 1.2F};
static float g_wm_v[6] = {0, 0, 0, 0, 0, -1.5F};
static uint32_t g_wm_f[1] = {0};
static float g_rm_t[2] = {0.0F, 0.6F};
static float g_rm_v[6] = {0, 0, 0, 0, 0, -2.7F};

UTEST(anim_blendspace, root_motion_blends_the_strides_and_keeps_the_pose_in_place)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, false));
    EngAnimClip walk = make_clip(1.2F, 1, g_wm_ch, 2, g_wm_t, g_wm_v, g_wm_f);
    EngAnimClip run = make_clip(0.6F, 1, g_wm_ch, 2, g_rm_t, g_rm_v, g_wm_f);
    ASSERT_TRUE(eng_blendspace_add(&f.bs, &walk, 1.5F));
    ASSERT_TRUE(eng_blendspace_add(&f.bs, &run, 4.5F));
    EXPECT_EQ(f.bs.root_joint, UINT32_MAX); /* désactivé par défaut */
    EXPECT_FALSE(eng_blendspace_set_root_joint(&f.bs, 3));
    EXPECT_FALSE(eng_blendspace_set_root_joint(NULL, 0));
    ASSERT_TRUE(eng_blendspace_set_root_joint(&f.bs, 0));

    /* paramètre 3,0 : cycle 0,9 s, foulée 2,1 m ; un cycle entier (54 images) = 2,1 m */
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, 3.0F));
    float sum = 0.0F;
    for (int i = 0; i < 54; i++) {
        ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.9F / 54.0F, 3.0F));
        sum += f.bs.motion.translation[2];
        EXPECT_NEAR(f.bs.pose.translation[2], 0.0F, 1e-6F); /* en place */
        EXPECT_NEAR(f.bs.pose.translation[0], 0.0F, 1e-6F);
    }
    EXPECT_NEAR(sum, -2.1F, 1e-3F);
    /* marche pure : 1,5 m par cycle de 1,2 s -> 1,25 m/s ; course pure : 2,7 m / 0,6 s = 4,5 m/s */
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.1F, 1.5F));
    EXPECT_NEAR(f.bs.motion.translation[2] / 0.1F, -1.25F, 1e-3F);
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.1F, 4.5F));
    EXPECT_NEAR(f.bs.motion.translation[2] / 0.1F, -4.5F, 1e-3F);
    /* pas de temps nul : pas de déplacement */
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, 3.0F));
    EXPECT_NEAR(f.bs.motion.translation[2], 0.0F, 1e-7F);
    /* un gros pas (2 cycles de marche) : déplacement de 2 foulées, pas d'erreur de bouclage */
    ASSERT_TRUE(eng_blendspace_set_phase(&f.bs, 0.3F));
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 2.4F, 1.5F));
    EXPECT_NEAR(f.bs.motion.translation[2], -3.0F, 1e-3F);
    /* désactivé : pose non modifiée, déplacement nul */
    ASSERT_TRUE(eng_blendspace_set_root_joint(&f.bs, UINT32_MAX));
    ASSERT_TRUE(eng_blendspace_set_phase(&f.bs, 0.5F));
    ASSERT_TRUE(eng_blendspace_update(&f.bs, 0.0F, 1.5F));
    EXPECT_NEAR(f.bs.pose.translation[2], -0.75F, 1e-5F);
    EXPECT_NEAR(f.bs.motion.translation[2], 0.0F, 1e-7F);
    eng_arena_shutdown(&f.arena);
}
