/* Animateur d'acteur de combat (M4) : programmes choisis selon l'état du jeu, calage d'une attaque
 * sur son tick, fondus sans saut, roulade sans glissement, mort tenue, saut et atterrissage.
 *
 * Squelette : racine « r » (0) -> « a » (1). Clips synthétiques : chacun écrit une valeur connue
 * dans la translation x de « a » (une rampe pour l'attaque), ce qui permet de lire directement le
 * clip choisi et l'instant lu. */
#include <math.h>
#include <string.h>

#include "engine/actor_anim.h"
#include "utest.h"

static EngJoint g_joints[2] = {
    {-1, {0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, "r"},
    {0, {0, 1, 0}, {0, 0, 0, 1}, {1, 1, 1}, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1}, "a"},
};

/* Un clip constant : « a » à x = value pendant `dur` secondes. */
typedef struct ConstClip {
    EngAnimChannel ch[1];
    float t[2];
    float v[6];
    uint32_t f[1];
    EngAnimClip clip;
} ConstClip;

static void make_const(ConstClip *c, float dur, float value)
{
    c->ch[0] = (EngAnimChannel){1, ENG_ANIM_TRANSLATION, 2, 0};
    c->t[0] = 0.0F;
    c->t[1] = dur;
    const float v[6] = {value, 1, 0, value, 1, 0};
    memcpy(c->v, v, sizeof v);
    c->f[0] = 0;
    c->clip = (EngAnimClip){.duration = dur,
                            .channel_count = 1,
                            .channels = c->ch,
                            .key_total = 2,
                            .times = c->t,
                            .values = c->v,
                            .value_first = c->f,
                            .skeleton_path = "t.skel"};
}

/* Rampe : x passe de `from` à `to` entre 0 et `dur`. */
static void make_ramp(ConstClip *c, float dur, float from, float to)
{
    make_const(c, dur, from);
    c->v[3] = to;
}

enum {
    C_IDLE,
    C_WALK,
    C_RUN,
    C_JSTART,
    C_JAIR,
    C_JLAND,
    C_ROLL,
    C_BLOCK,
    C_PARRY,
    C_HIT,
    C_STAGGER,
    C_DEATH,
    C_ATTACK,
    C_COUNT
};

typedef struct Fx {
    EngArena arena;
    EngSkeleton sk;
    ConstClip clips[C_COUNT];
    EngActorSet set;
    EngActorAnim anim;
} Fx;

static bool fx_init(Fx *f, float fade)
{
    memset(f, 0, sizeof *f);
    f->sk = (EngSkeleton){.joint_count = 2, .joints = g_joints};
    make_const(&f->clips[C_IDLE], 1.0F, 0.0F);
    make_const(&f->clips[C_WALK], 1.0F, 1.0F);
    make_const(&f->clips[C_RUN], 0.5F, 2.0F);
    make_const(&f->clips[C_JSTART], 0.5F, 10.0F);
    make_const(&f->clips[C_JAIR], 1.0F, 11.0F);
    make_const(&f->clips[C_JLAND], 0.6F, 12.0F);
    make_const(&f->clips[C_ROLL], 1.0F, 13.0F);
    make_const(&f->clips[C_BLOCK], 1.0F, 14.0F);
    make_const(&f->clips[C_PARRY], 1.0F, 15.0F);
    make_const(&f->clips[C_HIT], 1.0F, 16.0F);
    make_const(&f->clips[C_STAGGER], 1.0F, 17.0F);
    make_ramp(&f->clips[C_DEATH], 1.0F, 100.0F, 200.0F);
    make_ramp(&f->clips[C_ATTACK], 2.0F, 0.0F, 2.0F); /* x = instant lu */
    const EngActorRole roles[12] = {ENG_ACTOR_IDLE,       ENG_ACTOR_WALK,     ENG_ACTOR_RUN,
                                    ENG_ACTOR_JUMP_START, ENG_ACTOR_JUMP_AIR, ENG_ACTOR_JUMP_LAND,
                                    ENG_ACTOR_ROLL_POSE,  ENG_ACTOR_BLOCK,    ENG_ACTOR_PARRY,
                                    ENG_ACTOR_HIT,        ENG_ACTOR_STAGGER,  ENG_ACTOR_DEATH};
    for (int i = 0; i < 12; i++) {
        f->set.clips[roles[i]] = &f->clips[i].clip;
    }
    f->set.walk_speed = 1.0F;
    f->set.run_speed = 3.0F;
    f->set.fade = fade;
    f->set.land_time = 0.3F;
    f->set.roll_radius = 0.5F;
    f->set.root_joint = 0;
    f->set.roll_pose_time = 0.0F;
    return eng_arena_init(&f->arena, "actor", (size_t)1 << 16) &&
           eng_actor_anim_init(&f->anim, &f->arena, &f->sk, &f->set);
}

static void fx_done(Fx *f)
{
    eng_arena_shutdown(&f->arena);
}

static float ax(const Fx *f)
{
    return f->anim.pose.translation[3]; /* translation x de l'articulation 1 */
}

static EngActorDrive ground(float speed)
{
    return (EngActorDrive){.ground_speed = speed, .grounded = true, .state = ENG_CS_IDLE};
}

UTEST(actor_anim, locomotion_blends_idle_walk_and_run_by_ground_speed)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, 0.0F));
    EngActorDrive d = ground(0.0F);
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_NEAR(ax(&f), 0.0F, 1e-5F);
    EXPECT_EQ(f.anim.program, ENG_AP_LOCOMOTION);
    d = ground(1.0F);
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_NEAR(ax(&f), 1.0F, 1e-5F); /* marche seule à sa vitesse au sol */
    d = ground(2.0F);
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_NEAR(ax(&f), 1.5F, 1e-5F); /* à mi-chemin marche / course */
    d = ground(6.0F);
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_NEAR(ax(&f), 2.0F, 1e-5F); /* au-delà : course seule (lue plus vite) */
    fx_done(&f);
}

UTEST(actor_anim, beyond_the_run_speed_the_cycle_speeds_up_in_proportion)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, 0.0F));
    EngActorDrive d = ground(3.0F);
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.1F));
    const float p0 = f.anim.loco.phase;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.1F));
    const float step_run = f.anim.loco.phase - p0;
    d = ground(6.0F); /* deux fois la vitesse de course */
    const float p1 = f.anim.loco.phase;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.1F));
    float step_fast = f.anim.loco.phase - p1;
    if (step_fast < 0.0F) {
        step_fast += 1.0F;
    }
    EXPECT_NEAR(step_run, 0.2F, 1e-4F); /* 0,1 s d'un cycle de 0,5 s */
    EXPECT_NEAR(step_fast, 2.0F * step_run, 1e-4F);
    fx_done(&f);
}

UTEST(actor_anim, an_attack_clip_is_read_at_offset_plus_ticks_times_speed)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, 0.0F));
    EngActorDrive d = ground(0.0F);
    d.state = ENG_CS_ATTACK;
    d.attack_clip = &f.clips[C_ATTACK].clip;
    d.attack_speed = 2.0F;
    d.attack_offset = 0.2F;
    d.attack_serial = 1;
    d.state_ticks = 6;
    d.tick_fraction = 0.5F;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_EQ(f.anim.program, ENG_AP_ATTACK);
    /* 0,2 + (6,5 / 60) x 2 = 0,41667 */
    EXPECT_NEAR(ax(&f), 0.2F + (6.5F / 60.0F) * 2.0F, 1e-5F);
    d.state_ticks = 600; /* au-delà du clip : dernière image */
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_NEAR(ax(&f), 2.0F, 1e-5F);
    fx_done(&f);
}

UTEST(actor_anim, a_change_of_program_fades_without_a_jump)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, 0.2F));
    EngActorDrive d = ground(0.0F);
    for (int i = 0; i < 30; i++) {
        ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 1.0F / 60.0F));
    }
    EXPECT_NEAR(ax(&f), 0.0F, 1e-5F);
    d.state = ENG_CS_BLOCK; /* garde : x = 14 */
    float prev = ax(&f);
    float biggest = 0.0F;
    for (int i = 0; i < 12; i++) { /* 12 images de 1/60 s = 0,2 s : fondu complet */
        ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 1.0F / 60.0F));
        biggest = fmaxf(biggest, fabsf(ax(&f) - prev));
        prev = ax(&f);
    }
    EXPECT_NEAR(ax(&f), 14.0F, 1e-4F);
    EXPECT_LT(biggest, 14.0F * 0.25F); /* aucun saut : au plus un quart du chemin par image */
    EXPECT_EQ(f.anim.program, ENG_AP_BLOCK);
    fx_done(&f);
}

UTEST(actor_anim, a_new_attack_of_a_combo_restarts_the_fade)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, 0.1F));
    EngActorDrive d = ground(0.0F);
    d.state = ENG_CS_ATTACK;
    d.attack_clip = &f.clips[C_ATTACK].clip;
    d.attack_speed = 1.0F;
    d.attack_serial = 1;
    for (uint32_t t = 0; t < 40; t++) {
        d.state_ticks = t;
        ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 1.0F / 60.0F));
    }
    const float end_of_first = ax(&f); /* ~0,65 */
    d.attack_serial = 2;               /* coup suivant : son tick repart de 0 */
    d.state_ticks = 0;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 1.0F / 60.0F));
    EXPECT_GT(f.anim.fade_left, 0.0F);
    EXPECT_GT(ax(&f), 0.5F * end_of_first); /* encore proche de la fin du premier coup */
    fx_done(&f);
}

UTEST(actor_anim, the_roll_turns_the_root_by_two_pi_times_progress_about_the_side_axis)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, 0.0F));
    EngActorDrive d = ground(0.0F);
    d.state = ENG_CS_DODGE;
    d.dodge_dir[0] = 0.0F;
    d.dodge_dir[1] = 1.0F; /* vers +Z : axe = haut x (0, 0, 1) = +X */
    d.dodge_progress = 0.25F;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_EQ(f.anim.program, ENG_AP_ROLL);
    const float *q = &f.anim.pose.rotation[0];
    /* quart de tour autour de +X : (sin 45°, 0, 0, cos 45°) */
    EXPECT_NEAR(q[0], 0.70710678F, 1e-5F);
    EXPECT_NEAR(q[1], 0.0F, 1e-5F);
    EXPECT_NEAR(q[3], 0.70710678F, 1e-5F);
    /* la racine (à l'origine) tourne autour du pivot (0, r, 0) : elle passe en (0, r, -r) */
    const float *t = &f.anim.pose.translation[0];
    EXPECT_NEAR(t[1], 0.5F, 1e-5F);
    EXPECT_NEAR(t[2], -0.5F, 1e-5F);
    /* roulage sans glissement : arc parcouru = rayon x angle = distance x avancée */
    EXPECT_NEAR(0.5F * 6.28318530718F * 0.25F, (6.28318530718F * 0.5F) * 0.25F, 1e-6F);
    d.dodge_progress = 1.0F; /* tour complet : rotation identité (au signe près) */
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_NEAR(fabsf(q[3]), 1.0F, 1e-5F);
    EXPECT_NEAR(t[1], 0.0F, 1e-4F);
    fx_done(&f);
}

UTEST(actor_anim, death_holds_its_last_frame_and_reactions_follow_the_state)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, 0.0F));
    EngActorDrive d = ground(0.0F);
    d.state = ENG_CS_HITSTUN;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_NEAR(ax(&f), 16.0F, 1e-5F);
    d.state = ENG_CS_STAGGER;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_NEAR(ax(&f), 17.0F, 1e-5F);
    d.state = ENG_CS_PARRY;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_NEAR(ax(&f), 15.0F, 1e-5F);
    d.state = ENG_CS_DEAD;
    d.state_ticks = 30; /* 0,5 s */
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_NEAR(ax(&f), 150.0F, 1e-3F);
    d.state_ticks = 6000;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_NEAR(ax(&f), 200.0F, 1e-3F); /* dernière image tenue */
    fx_done(&f);
}

UTEST(actor_anim, a_jump_plays_start_then_air_then_a_short_landing)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, 0.0F));
    EngActorDrive d = ground(0.0F);
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    d.grounded = false;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.1F));
    EXPECT_EQ(f.anim.program, ENG_AP_JUMP_START);
    EXPECT_NEAR(ax(&f), 10.0F, 1e-5F);
    for (int i = 0; i < 5; i++) { /* 0,6 s en l'air : le début (0,5 s) est fini */
        ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.1F));
    }
    EXPECT_EQ(f.anim.program, ENG_AP_JUMP_AIR);
    EXPECT_NEAR(ax(&f), 11.0F, 1e-5F);
    d.grounded = true;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.1F));
    EXPECT_EQ(f.anim.program, ENG_AP_JUMP_LAND);
    EXPECT_NEAR(ax(&f), 12.0F, 1e-5F);
    for (int i = 0; i < 3; i++) { /* atterrissage de 0,3 s */
        ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.1F));
    }
    EXPECT_EQ(f.anim.program, ENG_AP_LOCOMOTION);
    /* un pas dans le vide très court (bord de marche) ne déclenche pas d'atterrissage */
    d.grounded = false;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.05F));
    d.grounded = true;
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    EXPECT_EQ(f.anim.program, ENG_AP_LOCOMOTION);
    fx_done(&f);
}

UTEST(actor_anim, bad_settings_and_arguments_are_refused)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, 0.1F));
    EngActorAnim other;
    EngActorSet bad = f.set;
    bad.run_speed = bad.walk_speed; /* course pas plus rapide que la marche */
    EXPECT_FALSE(eng_actor_anim_init(&other, &f.arena, &f.sk, &bad));
    bad = f.set;
    bad.root_joint = 5; /* hors du squelette alors que la roulade tourne la racine */
    EXPECT_FALSE(eng_actor_anim_init(&other, &f.arena, &f.sk, &bad));
    bad = f.set;
    bad.fade = 2.0F;
    EXPECT_FALSE(eng_actor_anim_init(&other, &f.arena, &f.sk, &bad));
    EngActorDrive d = ground(NAN);
    EXPECT_FALSE(eng_actor_anim_update(&f.anim, &d, 0.016F));
    d = ground(0.0F);
    EXPECT_FALSE(eng_actor_anim_update(&f.anim, &d, -1.0F));
    EXPECT_FALSE(eng_actor_anim_update(NULL, &d, 0.016F));
    EXPECT_STREQ(eng_actor_program_name(ENG_AP_ROLL), "roll");
    EXPECT_STREQ(eng_actor_program_name(ENG_AP_COUNT), "?");
    fx_done(&f);
}

UTEST(actor_anim, the_blend_speed_is_smoothed_so_a_start_does_not_pop)
{
    Fx f;
    ASSERT_TRUE(fx_init(&f, 0.0F));
    f.set.speed_smooth = 0.1F;
    ASSERT_TRUE(eng_actor_anim_init(&f.anim, &f.arena, &f.sk, &f.set));
    EngActorDrive d = ground(0.0F);
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 1.0F / 60.0F));
    d = ground(1.0F); /* démarrage brutal : la marche entière d'un coup */
    ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 1.0F / 60.0F));
    /* une image (1/60 s) avec une constante de 0,1 s : 1 - e^(-1/6) = 15 % du chemin */
    EXPECT_NEAR(ax(&f), 1.0F - expf(-1.0F / 6.0F), 1e-4F);
    for (int i = 0; i < 60; i++) {
        ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 1.0F / 60.0F));
    }
    EXPECT_NEAR(ax(&f), 1.0F, 1e-3F); /* rejoint la vitesse demandée */
    f.set.speed_smooth = 1.5F;        /* hors bornes */
    EngActorAnim other;
    EXPECT_FALSE(eng_actor_anim_init(&other, &f.arena, &f.sk, &f.set));
    fx_done(&f);
}
