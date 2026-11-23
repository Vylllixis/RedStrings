/* Exécution de la machine à états d'animation (M4-19). Chaîne de 3 articulations ; clips : idle
 * (racine à z = 5), walk (rotation 0 -> 90 degrés en 1,2 s), run (idem en 0,6 s, racine à x = 3) et
 * jump (une fois, 1 s, racine qui monte de 0 à y = 3). */
#include <math.h>
#include <string.h>

#include "engine/anim_machine.h"
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
static EngAnimChannel g_j_ch[1] = {{0, ENG_ANIM_TRANSLATION, 2, 0}};
static float g_j_t[2] = {0.0F, 1.0F};
static float g_j_v[6] = {0, 0, 0, 0, 3, 0};
static uint32_t g_j_f[1] = {0};

static const char *g_loco = "param speed\n"
                            "param jump\n"
                            "clip idle idle.anim\n"
                            "clip walk walk.anim\n"
                            "clip run run.anim\n"
                            "clip jump jump.anim once\n"
                            "state idle clip idle\n"
                            "state move blend speed idle:0 walk:1.5 run:4.5\n"
                            "state jump clip jump\n"
                            "transition idle -> move when speed > 0.1 fade 0.2\n"
                            "transition move -> idle when speed < 0.1 fade 0.3\n"
                            "transition any -> jump when jump > 0.5 fade 0.1\n"
                            "transition jump -> idle when ended fade 0.2\n"
                            "start idle\n";

typedef struct Fx {
    EngArena arena;
    EngSkeleton sk;
    EngAnimClip idle, walk, run, jump;
    EngAnimMachine m;
    EngAnimGraphDef def;
    bool hide_run; /* le finder « perd » le clip run */
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

static const EngAnimClip *finder(void *user, const char *name, const char *path)
{
    (void)path;
    Fx *f = user;
    if (strcmp(name, "idle") == 0) {
        return &f->idle;
    }
    if (strcmp(name, "walk") == 0) {
        return &f->walk;
    }
    if (strcmp(name, "run") == 0) {
        return f->hide_run ? NULL : &f->run;
    }
    if (strcmp(name, "jump") == 0) {
        return &f->jump;
    }
    return NULL;
}

static bool fx_init(Fx *f, const char *text)
{
    f->sk = (EngSkeleton){.joint_count = 3, .joints = g_joints};
    f->idle = make_clip(2.0F, 1, g_i_ch, 2, g_i_t, g_i_v, g_i_f);
    f->walk = make_clip(1.2F, 1, g_w_ch, 2, g_w_t, g_w_v, g_w_f);
    f->run = make_clip(0.6F, 2, g_r_ch, 4, g_r_t, g_r_v, g_r_f);
    f->jump = make_clip(1.0F, 1, g_j_ch, 2, g_j_t, g_j_v, g_j_f);
    f->hide_run = false;
    char err[200];
    if (!eng_arena_init(&f->arena, "machine", (size_t)1 << 17) ||
        !eng_animmachine_init(&f->m, &f->arena, &f->sk) ||
        !eng_animgraph_parse(text, &f->def, err, sizeof err)) {
        return false;
    }
    return eng_animmachine_set_def(&f->m, &f->def, finder, f);
}

static void step(Fx *f, float seconds, float dt)
{
    const int frames = (int)lroundf(seconds / dt);
    for (int i = 0; i < frames; i++) {
        (void)eng_animmachine_update(&f->m, dt);
    }
}

UTEST(anim_machine, starts_in_the_start_state_on_its_first_pose)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "idle");
    EXPECT_FALSE(eng_animmachine_in_transition(&f.m));
    EXPECT_NEAR(f.m.pose.translation[2], 5.0F, 1e-6F);
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.5F));
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "idle"); /* aucune condition vraie */
    EXPECT_EQ(f.m.transitions_taken, 0U);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, parameters_are_set_by_name_or_index_and_refuse_bad_values)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    EXPECT_TRUE(eng_animmachine_set_param(&f.m, "speed", 2.0F));
    EXPECT_NEAR(f.m.params[0], 2.0F, 1e-7F);
    EXPECT_TRUE(eng_animmachine_set_param_index(&f.m, 1, 1.0F));
    EXPECT_NEAR(f.m.params[1], 1.0F, 1e-7F);
    EXPECT_FALSE(eng_animmachine_set_param(&f.m, "nope", 1.0F));
    EXPECT_FALSE(eng_animmachine_set_param(&f.m, "speed", NAN));
    EXPECT_FALSE(eng_animmachine_set_param(&f.m, "speed", INFINITY));
    EXPECT_FALSE(eng_animmachine_set_param_index(&f.m, 2, 1.0F));
    EXPECT_FALSE(eng_animmachine_set_param(NULL, "speed", 1.0F));
    EXPECT_NEAR(f.m.params[0], 2.0F, 1e-7F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, a_transition_fades_from_the_old_state_to_the_new_one)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 1.5F));
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.0F));
    EXPECT_TRUE(eng_animmachine_in_transition(&f.m));
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "move");
    EXPECT_EQ(f.m.transitions_taken, 1U);
    EXPECT_NEAR(f.m.pose.translation[2], 5.0F, 1e-5F); /* début du fondu : encore l'ancienne pose */
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.1F));   /* mi-fondu (0,1 / 0,2) : poids 0,5 */
    EXPECT_TRUE(eng_animmachine_in_transition(&f.m));
    EXPECT_NEAR(f.m.pose.translation[2], 2.5F, 1e-4F); /* idle (z = 5) et marche (z = 0) */
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.1F));
    EXPECT_FALSE(eng_animmachine_in_transition(&f.m));
    EXPECT_NEAR(f.m.pose.translation[2], 0.0F, 1e-5F); /* marche pure */
    EXPECT_EQ(f.m.transitions_taken, 1U);              /* et pas de rebond */
    /* retour au repos */
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 0.0F));
    step(&f, 0.4F, 0.01F);
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "idle");
    EXPECT_NEAR(f.m.pose.translation[2], 5.0F, 1e-4F);
    EXPECT_EQ(f.m.transitions_taken, 2U);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, a_blend_state_follows_its_parameter)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 3.0F));
    step(&f, 0.3F, 0.01F); /* fondu terminé */
    EXPECT_FALSE(eng_animmachine_in_transition(&f.m));
    EXPECT_NEAR(f.m.pose.translation[0], 1.5F, 1e-4F); /* mi-marche, mi-course */
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 4.5F));
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.0F));
    EXPECT_NEAR(f.m.pose.translation[0], 3.0F, 1e-5F); /* course pure */
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, any_transitions_and_ended_conditions_work_and_loops_never_end)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    step(&f, 5.0F, 0.05F); /* idle boucle pendant 2,5 cycles : « ended » ne se déclenche pas */
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "idle");
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "jump", 1.0F));
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.0F));
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "jump");
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "jump", 0.0F));
    step(&f, 0.5F, 0.01F);
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "jump"); /* pas fini (0,5 s sur 1 s) */
    EXPECT_NEAR(f.m.pose.translation[1], 1.5F, 0.05F);      /* à mi-saut */
    step(&f, 0.6F, 0.01F);                                  /* la fin est atteinte */
    EXPECT_TRUE(eng_animmachine_in_transition(&f.m) ||
                strcmp(eng_animmachine_state_name(&f.m), "idle") == 0);
    step(&f, 0.4F, 0.01F);
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "idle");
    EXPECT_FALSE(eng_animmachine_in_transition(&f.m));
    /* « any » ne rebondit pas sur l'état où l'on est déjà */
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "jump", 1.0F));
    step(&f, 0.3F, 0.01F);
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "jump");
    const uint32_t taken = f.m.transitions_taken;
    step(&f, 0.3F, 0.01F);
    EXPECT_EQ(f.m.transitions_taken, taken);
    eng_arena_shutdown(&f.arena);
}

static const EngAnimClip *finder_all(void *user, const char *name, const char *path)
{
    (void)name;
    (void)path;
    return user;
}

UTEST(anim_machine, the_first_true_transition_in_file_order_is_the_one_taken)
{
    static const char *text = "param p\nclip a a.anim\n"
                              "state s clip a\nstate t clip a\nstate u clip a\n"
                              "transition s -> t when p > 0\n"
                              "transition s -> u when p > 0\n"
                              "start s\n";
    Fx f;
    f.sk = (EngSkeleton){.joint_count = 3, .joints = g_joints};
    f.idle = make_clip(2.0F, 1, g_i_ch, 2, g_i_t, g_i_v, g_i_f);
    ASSERT_TRUE(eng_arena_init(&f.arena, "machine", (size_t)1 << 17));
    ASSERT_TRUE(eng_animmachine_init(&f.m, &f.arena, &f.sk));
    char err[100];
    ASSERT_TRUE(eng_animgraph_parse(text, &f.def, err, sizeof err));
    ASSERT_TRUE(eng_animmachine_set_def(&f.m, &f.def, finder_all, &f.idle));
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "p", 1.0F));
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.0F));
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "t");
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, changing_state_during_a_fade_keeps_the_displayed_pose_continuous)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 1.5F));
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.0F));
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.1F)); /* mi-fondu idle -> move */
    EXPECT_TRUE(eng_animmachine_in_transition(&f.m));
    float before[9];
    memcpy(before, f.m.pose.translation, sizeof before);
    float rot_before[12];
    memcpy(rot_before, f.m.pose.rotation, sizeof rot_before);
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 0.0F)); /* move -> idle, en plein fondu */
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.0F));
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "idle");
    EXPECT_TRUE(f.m.from_frozen);
    for (int i = 0; i < 9; i++) {
        EXPECT_NEAR(f.m.pose.translation[i], before[i], 1e-5F);
    }
    for (int i = 0; i < 12; i++) {
        EXPECT_NEAR(f.m.pose.rotation[i], rot_before[i], 1e-5F);
    }
    step(&f, 0.4F, 0.01F);
    EXPECT_FALSE(eng_animmachine_in_transition(&f.m));
    EXPECT_NEAR(f.m.pose.translation[2], 5.0F, 1e-4F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, no_pose_jumps_across_a_whole_idle_walk_run_jump_sequence)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    float prev[3] = {0, 0, 0};
    memcpy(prev, f.m.pose.translation, sizeof prev);
    float max_step = 0.0F;
    float speed = 0.0F;
    for (int i = 0; i < 600; i++) {
        const float time = (float)i / 60.0F;
        const float target = time < 1.0F ? 0.0F : (time < 3.0F ? 1.5F : 4.5F);
        /* le paramètre est lissé (3 m/s^2) comme le ferait la locomotion : la machine ne doit pas
         * avoir à masquer un saut d'entrée */
        speed += fminf(fmaxf(target - speed, -0.05F), 0.05F);
        ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", speed));
        ASSERT_TRUE(eng_animmachine_set_param(&f.m, "jump", (i == 450) ? 1.0F : 0.0F));
        ASSERT_TRUE(eng_animmachine_update(&f.m, 1.0F / 60.0F));
        float d2 = 0.0F;
        for (int c = 0; c < 3; c++) {
            const float d = f.m.pose.translation[c] - prev[c];
            d2 += d * d;
        }
        /* racine seule. Les fondus sont des smoothstep : pente max = 1,5 x distance / images ; ici
         * idle -> marche : 5 m en 12 images = 0,63 m/image, marche -> saut : 3 m en 6 images =
         * 0,75 m/image. Une transition sans fondu ferait 5 m ou 3 m en UNE image. */
        max_step = fmaxf(max_step, sqrtf(d2));
        memcpy(prev, f.m.pose.translation, sizeof prev);
    }
    EXPECT_LT(max_step, 0.8F);
    EXPECT_GT(max_step, 0.3F); /* et le test voit bien les fondus */
    EXPECT_GE(f.m.transitions_taken, 3U);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, set_def_refuses_a_missing_clip_and_keeps_the_machine_running)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 1.5F));
    step(&f, 0.3F, 0.01F);
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "move");
    f.hide_run = true;
    EXPECT_FALSE(eng_animmachine_set_def(&f.m, &f.def, finder, &f));
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "move");
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.016F));
    f.hide_run = false;
    /* clip qui vise une articulation absente */
    EngAnimChannel far[1] = {{7, ENG_ANIM_ROTATION, 2, 0}};
    EngAnimClip bad = f.walk;
    bad.channels = far;
    EngAnimClip good = f.walk;
    f.walk = bad;
    EXPECT_FALSE(eng_animmachine_set_def(&f.m, &f.def, finder, &f));
    f.walk = good;
    /* clip de blend space à durée nulle */
    f.walk.duration = 0.0F;
    EXPECT_FALSE(eng_animmachine_set_def(&f.m, &f.def, finder, &f));
    f.walk = good;
    EXPECT_FALSE(eng_animmachine_set_def(NULL, &f.def, finder, &f));
    EXPECT_FALSE(eng_animmachine_set_def(&f.m, NULL, finder, &f));
    EXPECT_FALSE(eng_animmachine_set_def(&f.m, &f.def, NULL, &f));
    EngAnimGraphDef empty;
    memset(&empty, 0, sizeof empty);
    EXPECT_FALSE(eng_animmachine_set_def(&f.m, &empty, finder, &f));
    EXPECT_TRUE(eng_animmachine_set_def(&f.m, &f.def, finder, &f)); /* et la bonne passe */
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, hot_reload_keeps_the_current_state_its_phase_and_the_parameters)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 1.5F));
    step(&f, 0.7F, 0.01F); /* dans « move », phase ~ 0,58 */
    const float phase = f.m.slots[f.m.cur].bs.phase;
    EXPECT_GT(phase, 0.3F);
    /* nouvelle version : fondu plus long, un état en plus ; « move » existe encore */
    char text[1024];
    (void)snprintf(text, sizeof text, "%s", g_loco);
    char *fade = strstr(text, "fade 0.2");
    ASSERT_TRUE(fade != NULL);
    fade[5] = '0';
    fade[6] = '.';
    fade[7] = '9';
    EngAnimGraphDef next;
    char err[100];
    ASSERT_TRUE(eng_animgraph_parse(text, &next, err, sizeof err));
    ASSERT_TRUE(eng_animmachine_set_def(&f.m, &next, finder, &f));
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "move");
    EXPECT_NEAR(f.m.slots[f.m.cur].bs.phase, phase, 1e-5F);
    EXPECT_NEAR(f.m.params[0], 1.5F, 1e-7F);
    EXPECT_NEAR(f.m.def.transitions[0].fade, 0.9F, 1e-6F);
    EXPECT_FALSE(eng_animmachine_in_transition(&f.m));
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.016F));
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "move");

    /* version sans « move » : on repart de l'état initial ; les paramètres gardés par nom */
    static const char *small =
        "param jump\nparam speed\nclip idle idle.anim\nstate idle clip idle\nstart idle\n";
    ASSERT_TRUE(eng_animgraph_parse(small, &next, err, sizeof err));
    ASSERT_TRUE(eng_animmachine_set_def(&f.m, &next, finder, &f));
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "idle");
    EXPECT_NEAR(f.m.params[1], 1.5F, 1e-7F); /* « speed » a changé d'indice, pas de valeur */
    EXPECT_NEAR(f.m.params[0], 0.0F, 1e-7F);
    EXPECT_NEAR(f.m.pose.translation[2], 5.0F, 1e-5F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, hot_reload_in_the_middle_of_a_fade_drops_the_fade_cleanly)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 1.5F));
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.0F));
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.05F));
    EXPECT_TRUE(eng_animmachine_in_transition(&f.m));
    ASSERT_TRUE(eng_animmachine_set_def(&f.m, &f.def, finder, &f));
    EXPECT_FALSE(eng_animmachine_in_transition(&f.m));
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "move");
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.1F));
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, bad_arguments_and_unusable_machines_are_refused)
{
    Fx f;
    f.sk = (EngSkeleton){.joint_count = 3, .joints = g_joints};
    ASSERT_TRUE(eng_arena_init(&f.arena, "machine", (size_t)1 << 17));
    EngAnimMachine m;
    EXPECT_FALSE(eng_animmachine_init(NULL, &f.arena, &f.sk));
    EXPECT_FALSE(eng_animmachine_init(&m, NULL, &f.sk));
    EXPECT_FALSE(eng_animmachine_init(&m, &f.arena, NULL));
    EngArena tiny;
    ASSERT_TRUE(eng_arena_init(&tiny, "tiny", 300));
    EXPECT_FALSE(eng_animmachine_init(&m, &tiny, &f.sk));
    EXPECT_EQ(tiny.used, 0U);
    eng_arena_shutdown(&tiny);
    ASSERT_TRUE(eng_animmachine_init(&m, &f.arena, &f.sk));
    EXPECT_FALSE(eng_animmachine_update(&m, 0.1F)); /* sans définition */
    EXPECT_FALSE(eng_animmachine_update(NULL, 0.1F));
    EXPECT_STREQ(eng_animmachine_state_name(&m), "");
    EXPECT_STREQ(eng_animmachine_state_name(NULL), "");
    EXPECT_FALSE(eng_animmachine_in_transition(&m));
    EXPECT_FALSE(eng_animmachine_in_transition(NULL));
    EXPECT_FALSE(eng_animmachine_set_param(&m, "x", 1.0F));
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, bad_time_steps_are_ignored_and_nothing_is_allocated_after_init)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    const size_t used = f.arena.used;
    ASSERT_TRUE(eng_animmachine_update(&f.m, -1.0F));
    ASSERT_TRUE(eng_animmachine_update(&f.m, NAN));
    EXPECT_NEAR(f.m.slots[f.m.cur].layer.time, 0.0F, 1e-7F);
    for (int i = 0; i < 400; i++) {
        ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", (float)(i % 60) / 10.0F));
        ASSERT_TRUE(eng_animmachine_set_param(&f.m, "jump", (i % 97 == 0) ? 1.0F : 0.0F));
        ASSERT_TRUE(eng_animmachine_update(&f.m, 1.0F / 60.0F));
    }
    EXPECT_EQ(f.arena.used, used);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, comparison_operators_are_exact_at_the_threshold)
{
    static const char *ops[4] = {">", "<", ">=", "<="};
    /* la machine part de « s » ; à p = 1 (seuil exact) : > et < ne passent pas, >= et <= passent */
    const bool fires[4] = {false, false, true, true};
    for (int k = 0; k < 4; k++) {
        char text[256];
        (void)snprintf(text, sizeof text,
                       "param p\nclip a a.anim\nstate s clip a\nstate t clip a\n"
                       "transition s -> t when p %s 1\nstart s\n",
                       ops[k]);
        Fx f;
        f.sk = (EngSkeleton){.joint_count = 3, .joints = g_joints};
        f.idle = make_clip(2.0F, 1, g_i_ch, 2, g_i_t, g_i_v, g_i_f);
        ASSERT_TRUE(eng_arena_init(&f.arena, "machine", (size_t)1 << 17));
        ASSERT_TRUE(eng_animmachine_init(&f.m, &f.arena, &f.sk));
        char err[100];
        ASSERT_TRUE(eng_animgraph_parse(text, &f.def, err, sizeof err));
        ASSERT_TRUE(eng_animmachine_set_def(&f.m, &f.def, finder_all, &f.idle));
        ASSERT_TRUE(eng_animmachine_set_param(&f.m, "p", 1.0F));
        ASSERT_TRUE(eng_animmachine_update(&f.m, 0.0F));
        EXPECT_EQ(strcmp(eng_animmachine_state_name(&f.m), "t") == 0, fires[k]);
        /* juste de part et d'autre du seuil */
        const bool above = k == 0 || k == 2;
        ASSERT_TRUE(eng_animmachine_set_def(&f.m, &f.def, finder_all, &f.idle));
        ASSERT_TRUE(eng_animmachine_set_param(&f.m, "p", above ? 1.5F : 0.5F));
        ASSERT_TRUE(eng_animmachine_update(&f.m, 0.0F));
        EXPECT_STREQ(eng_animmachine_state_name(&f.m), "t");
        eng_arena_shutdown(&f.arena);
    }
}

UTEST(anim_machine, the_frozen_source_keeps_every_joint_and_property)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 1.5F));
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.0F));
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.05F)); /* en plein fondu */
    /* on pose une pose reconnaissable (rotations unitaires distinctes) dans la sortie affichée */
    for (size_t j = 0; j < 3; j++) {
        const float a = 0.2F * (float)(j + 1U);
        const float q[4] = {sinf(a), 0.0F, 0.0F, cosf(a)};
        memcpy(&f.m.pose.rotation[4U * j], q, sizeof q);
        for (size_t c = 0; c < 3; c++) {
            f.m.pose.translation[(3U * j) + c] = 10.0F + (float)((3U * j) + c);
            f.m.pose.scale[(3U * j) + c] = 2.0F + (float)((3U * j) + c);
        }
    }
    float rot[12];
    memcpy(rot, f.m.pose.rotation, sizeof rot);
    float tr[9];
    memcpy(tr, f.m.pose.translation, sizeof tr);
    float sc[9];
    memcpy(sc, f.m.pose.scale, sizeof sc);
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 0.0F));
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.0F)); /* move -> idle : on fige */
    EXPECT_TRUE(f.m.from_frozen);
    for (uint32_t i = 0; i < 12; i++) {
        EXPECT_NEAR(f.m.snapshot.rotation[i], rot[i], 1e-7F);
    }
    for (uint32_t i = 0; i < 9; i++) {
        EXPECT_NEAR(f.m.snapshot.translation[i], tr[i], 1e-7F);
        EXPECT_NEAR(f.m.snapshot.scale[i], sc[i], 1e-7F);
    }
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, set_def_validation_edges)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, g_loco));
    EngAnimGraphDef d = f.def;
    d.start = d.state_count; /* indice hors tableau */
    EXPECT_FALSE(eng_animmachine_set_def(&f.m, &d, finder, &f));
    /* un clip dont l'articulation vaut exactement joint_count : refusé */
    EngAnimChannel edge[1] = {{3, ENG_ANIM_ROTATION, 2, 0}};
    EngAnimClip bad = f.walk;
    bad.channels = edge;
    EngAnimClip good = f.walk;
    f.walk = bad;
    EXPECT_FALSE(eng_animmachine_set_def(&f.m, &f.def, finder, &f));
    f.walk = good;
    /* un clip de durée nulle est accepté dans un état clip (seul le blend space l'interdit) */
    f.jump.duration = 0.0F;
    EXPECT_TRUE(eng_animmachine_set_def(&f.m, &f.def, finder, &f));
    EXPECT_TRUE(eng_animmachine_update(&f.m, 0.1F));
    eng_arena_shutdown(&f.arena);
}

/* root motion : walkm avance de 1,5 m en 1,2 s, runm de 2,7 m en 0,6 s */
static EngAnimChannel g_wm_ch[1] = {{0, ENG_ANIM_TRANSLATION, 2, 0}};
static float g_wm_t[2] = {0.0F, 1.2F};
static float g_wm_v[6] = {0, 0, 0, 0, 0, -1.5F};
static uint32_t g_wm_f[1] = {0};
static float g_rm_t[2] = {0.0F, 0.6F};
static float g_rm_v[6] = {0, 0, 0, 0, 0, -2.7F};

static const char *g_rooted = "param speed\n"
                              "clip idle idle.anim\n"
                              "clip walk walk.anim\n"
                              "clip run run.anim\n"
                              "state idle clip idle\n"
                              "state move blend speed walk:1.5 run:4.5\n"
                              "transition idle -> move when speed > 0.1 fade 0.2\n"
                              "transition move -> idle when speed < 0.1 fade 0.2\n"
                              "root_motion r\n"
                              "start idle\n";

typedef struct MotionFx {
    EngAnimClip idle, walk, run;
} MotionFx;

static const EngAnimClip *motion_finder(void *user, const char *name, const char *path)
{
    (void)path;
    MotionFx *m = user;
    if (strcmp(name, "idle") == 0) {
        return &m->idle;
    }
    return strcmp(name, "walk") == 0 ? &m->walk : &m->run;
}

static bool motion_fx(Fx *f, MotionFx *mf, const char *text)
{
    f->sk = (EngSkeleton){.joint_count = 3, .joints = g_joints};
    mf->idle = make_clip(2.0F, 1, g_i_ch, 2, g_i_t, g_i_v, g_i_f);
    mf->walk = make_clip(1.2F, 1, g_wm_ch, 2, g_wm_t, g_wm_v, g_wm_f);
    mf->run = make_clip(0.6F, 1, g_wm_ch, 2, g_rm_t, g_rm_v, g_wm_f);
    char err[200];
    if (!eng_arena_init(&f->arena, "machine", (size_t)1 << 17) ||
        !eng_animmachine_init(&f->m, &f->arena, &f->sk) ||
        !eng_animgraph_parse(text, &f->def, err, sizeof err)) {
        return false;
    }
    return eng_animmachine_set_def(&f->m, &f->def, motion_finder, mf);
}

UTEST(anim_machine, root_motion_drives_the_character_and_keeps_the_pose_in_place)
{
    Fx f;
    MotionFx mf;
    ASSERT_TRUE(motion_fx(&f, &mf, g_rooted));
    EXPECT_EQ(f.m.root_joint, 0U);
    /* au repos : aucun déplacement */
    for (int i = 0; i < 30; i++) {
        ASSERT_TRUE(eng_animmachine_update(&f.m, 1.0F / 60.0F));
        EXPECT_NEAR(f.m.motion.translation[2], 0.0F, 1e-7F);
    }
    /* en mouvement à 3 m/s : après le fondu, un cycle (0,9 s) = une foulée de 2,1 m */
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 3.0F));
    float previous = 0.0F;
    float first = 0.0F;
    for (int i = 0; i < 12; i++) { /* le fondu : 0,2 s */
        ASSERT_TRUE(eng_animmachine_update(&f.m, 1.0F / 60.0F));
        const float v = f.m.motion.translation[2];
        if (i == 0) {
            first = v;
        }
        EXPECT_LE(v, previous + 1e-6F); /* de plus en plus négatif : la vitesse monte */
        previous = v;
    }
    EXPECT_FALSE(eng_animmachine_in_transition(&f.m));
    EXPECT_GT(fabsf(first), 0.0F);
    EXPECT_LT(fabsf(first), 0.1F * fabsf(previous)); /* démarre doucement */
    float sum = 0.0F;
    for (int i = 0; i < 54; i++) {
        ASSERT_TRUE(eng_animmachine_update(&f.m, 0.9F / 54.0F));
        sum += f.m.motion.translation[2];
        EXPECT_NEAR(f.m.pose.translation[2], 0.0F, 1e-6F);
    }
    EXPECT_NEAR(sum, -2.1F, 1e-3F);
    /* retour au repos : le déplacement s'éteint avec le fondu */
    ASSERT_TRUE(eng_animmachine_set_param(&f.m, "speed", 0.0F));
    for (int i = 0; i < 30; i++) {
        ASSERT_TRUE(eng_animmachine_update(&f.m, 1.0F / 60.0F));
    }
    EXPECT_STREQ(eng_animmachine_state_name(&f.m), "idle");
    EXPECT_NEAR(f.m.motion.translation[2], 0.0F, 1e-7F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, a_clip_state_applies_its_motion_once_and_stops_at_its_end)
{
    static const char *text =
        "clip walk walk.anim once\nstate s clip walk\nroot_motion r\nstart s\n";
    Fx f;
    MotionFx mf;
    ASSERT_TRUE(motion_fx(&f, &mf, text));
    float sum = 0.0F;
    for (int i = 0; i < 90; i++) { /* 1,5 s pour un clip de 1,2 s */
        ASSERT_TRUE(eng_animmachine_update(&f.m, 1.0F / 60.0F));
        sum += f.m.motion.translation[2];
    }
    EXPECT_NEAR(sum, -1.5F, 1e-4F); /* exactement une foulée de clip */
    EXPECT_NEAR(f.m.motion.translation[2], 0.0F, 1e-7F);
    EXPECT_NEAR(f.m.pose.translation[2], 0.0F, 1e-7F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, without_a_root_motion_line_the_pose_keeps_the_root_and_the_motion_is_zero)
{
    static const char *text = "clip walk walk.anim\nstate s clip walk\nstart s\n";
    Fx f;
    MotionFx mf;
    ASSERT_TRUE(motion_fx(&f, &mf, text));
    EXPECT_EQ(f.m.root_joint, UINT32_MAX);
    ASSERT_TRUE(eng_animmachine_update(&f.m, 0.6F));
    EXPECT_NEAR(f.m.pose.translation[2], -0.75F, 1e-5F);
    EXPECT_NEAR(f.m.motion.translation[2], 0.0F, 1e-7F);
    eng_arena_shutdown(&f.arena);
}

UTEST(anim_machine, root_motion_needs_an_existing_root_joint)
{
    Fx f;
    MotionFx mf;
    static const char *no_such =
        "clip walk walk.anim\nstate s clip walk\nroot_motion nowhere\nstart s\n";
    EXPECT_FALSE(motion_fx(&f, &mf, no_such));
    eng_arena_shutdown(&f.arena);
    /* « a » existe mais a un parent : refusé */
    static const char *child = "clip walk walk.anim\nstate s clip walk\nroot_motion a\nstart s\n";
    EXPECT_FALSE(motion_fx(&f, &mf, child));
    eng_arena_shutdown(&f.arena);
    /* un rechargement qui échoue laisse l'ancienne définition (et son root motion) */
    ASSERT_TRUE(motion_fx(&f, &mf, g_rooted));
    EngAnimGraphDef bad;
    char err[100];
    ASSERT_TRUE(eng_animgraph_parse(no_such, &bad, err, sizeof err));
    EXPECT_FALSE(eng_animmachine_set_def(&f.m, &bad, motion_finder, &mf));
    EXPECT_EQ(f.m.root_joint, 0U);
    eng_arena_shutdown(&f.arena);
}
