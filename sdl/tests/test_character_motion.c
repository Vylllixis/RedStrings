/* Personnage animé du jeu (M4) vérifié sur les VRAIS fichiers cuits (chevalier KayKit) :
 *   - attack_sync : pour chaque attaque de assets/combat/attacks.txt qui nomme un clip, le passage
 *     de l'arme (pic de vitesse de l'os handslot.r, mesuré tick par tick avec le calage `anim`)
 *     tombe dans ses ticks actifs ;
 *   - pieds sans glissement (M4-18) : pied d'appui immobile au sol, à toutes les vitesses de jeu ;
 *   - transitions sans à-coup (M4-17) : aucun changement d'état ne fait sauter une articulation.
 * Les fichiers viennent du build (cible cooked_characters) : OW_COOKED_DIR, et de la source :
 * OW_ATTACKS_FILE. */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/actor_anim.h"
#include "engine/attack_data.h"
#include "engine/skeleton.h"
#include "utest.h"

#ifndef OW_COOKED_DIR
#error "OW_COOKED_DIR doit désigner bin/assets/cooked"
#endif

#define SCALE 0.8F /* AVATAR_SCALE du lanceur */
#define WALK_SPEED_RIG 0.84F
#define RUN_SPEED_RIG 3.55F
#define MAX_JOINTS 64U

typedef struct Fx {
    EngArena arena;
    EngArena scratch;
    EngSkeleton sk;
    EngAnimClip idle, walk, run, block, hit, death, jump_start;
    EngActorSet set;
    EngActorAnim anim;
    uint32_t hand_r, foot_l, foot_r;
    mat4 world[MAX_JOINTS];
} Fx;

static uint32_t joint(const EngSkeleton *s, const char *name)
{
    for (uint32_t i = 0; i < s->joint_count; i++) {
        if (s->joints[i].name != NULL && strcmp(s->joints[i].name, name) == 0) {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool load_clip(Fx *f, const char *name, EngAnimClip *out)
{
    char path[1024];
    (void)snprintf(path, sizeof path, OW_COOKED_DIR "/characters/knight/%s.anim", name);
    return eng_anim_load(&f->arena, &f->scratch, path, out);
}

static bool fx_init(Fx *f)
{
    memset(f, 0, sizeof *f);
    if (!eng_arena_init(&f->arena, "perso", (size_t)16 << 20) ||
        !eng_arena_init(&f->scratch, "perso-lecture", (size_t)16 << 20)) {
        return false;
    }
    if (!eng_skeleton_load(&f->arena, &f->scratch, OW_COOKED_DIR "/characters/knight.skel",
                           &f->sk) ||
        f->sk.joint_count > MAX_JOINTS) {
        return false;
    }
    f->hand_r = joint(&f->sk, "handslot.r");
    f->foot_l = joint(&f->sk, "foot.l");
    f->foot_r = joint(&f->sk, "foot.r");
    if (f->hand_r == UINT32_MAX || f->foot_l == UINT32_MAX || f->foot_r == UINT32_MAX) {
        return false;
    }
    if (!load_clip(f, "idle", &f->idle) || !load_clip(f, "walk", &f->walk) ||
        !load_clip(f, "run", &f->run) || !load_clip(f, "block", &f->block) ||
        !load_clip(f, "hit_a", &f->hit) || !load_clip(f, "death", &f->death) ||
        !load_clip(f, "jump_start", &f->jump_start)) {
        return false;
    }
    f->set.clips[ENG_ACTOR_IDLE] = &f->idle;
    f->set.clips[ENG_ACTOR_WALK] = &f->walk;
    f->set.clips[ENG_ACTOR_RUN] = &f->run;
    f->set.clips[ENG_ACTOR_BLOCK] = &f->block;
    f->set.clips[ENG_ACTOR_HIT] = &f->hit;
    f->set.clips[ENG_ACTOR_DEATH] = &f->death;
    f->set.clips[ENG_ACTOR_ROLL_POSE] = &f->jump_start;
    f->set.walk_speed = WALK_SPEED_RIG;
    f->set.run_speed = RUN_SPEED_RIG;
    f->set.fade = 0.12F;
    f->set.speed_smooth = 0.1F; /* comme le lanceur (avatar.c) */
    f->set.land_time = 0.3F;
    f->set.roll_pose_time = 0.3F;
    f->set.roll_radius = 3.0F / 6.2831853F / SCALE;
    f->set.root_joint = joint(&f->sk, "root");
    return eng_actor_anim_init(&f->anim, &f->arena, &f->sk, &f->set);
}

static void fx_done(Fx *f)
{
    eng_arena_shutdown(&f->scratch);
    eng_arena_shutdown(&f->arena);
}

/* Position d'une articulation dans le monde : le personnage avance de `travel` mètres vers -Z
 * (lacet 0), modèle tourné d'un demi-tour (KayKit regarde +Z) et mis à l'échelle. */
static void joint_world(const Fx *f, uint32_t j, float travel, float out[3])
{
    const float *p = f->world[j][3];
    out[0] = -p[0] * SCALE; /* demi-tour autour de Y : x -> -x, z -> -z */
    out[1] = p[1] * SCALE;
    out[2] = (-p[2] * SCALE) - travel;
}

UTEST(character_motion, every_attack_swing_peaks_inside_its_active_ticks)
{
    static Fx f;
    ASSERT_TRUE(fx_init(&f));
    char *text = (char *)SDL_LoadFile(OW_ATTACKS_FILE, NULL);
    ASSERT_TRUE(text != NULL);
    static EngAttackSet attacks;
    char err[160];
    const bool parsed = eng_attackset_parse(text, &attacks, err, sizeof err);
    SDL_free(text);
    ASSERT_TRUE(parsed);
    static EngPose pose;
    ASSERT_TRUE(eng_pose_init(&f.arena, &f.sk, &pose));
    uint32_t checked = 0;
    for (uint32_t a = 0; a < attacks.count; a++) {
        const EngAttack *atk = &attacks.attacks[a];
        if (atk->anim[0] == '\0') {
            continue;
        }
        EngAnimClip clip;
        ASSERT_TRUE(load_clip(&f, atk->anim, &clip));
        const uint32_t total = atk->startup + atk->active + atk->recovery;
        float prev[3] = {0, 0, 0};
        float best = -1.0F;
        uint32_t best_tick = 0;
        for (uint32_t k = 0; k <= total; k++) {
            const float t = atk->anim_offset + ((float)k / 60.0F * atk->anim_speed);
            ASSERT_TRUE(eng_pose_sample(&pose, &f.sk, &clip, t, ENG_ANIM_WRAP_CLAMP));
            ASSERT_TRUE(eng_pose_world(&pose, &f.sk, f.world));
            float p[3];
            joint_world(&f, f.hand_r, 0.0F, p);
            if (k > 0) {
                const float v = sqrtf(((p[0] - prev[0]) * (p[0] - prev[0])) +
                                      ((p[1] - prev[1]) * (p[1] - prev[1])) +
                                      ((p[2] - prev[2]) * (p[2] - prev[2])));
                if (v > best) {
                    best = v;
                    best_tick = k;
                }
            }
            memcpy(prev, p, sizeof prev);
        }
        /* pic mesuré sur le déplacement (k-1 -> k) : il appartient à un tick actif */
        const bool inside = best_tick >= atk->startup && best_tick < atk->startup + atk->active;
        if (!inside) {
            (void)fprintf(stderr, "%s (%s) : pic au tick %u, actifs %u à %u\n", atk->name,
                          atk->anim, (unsigned)best_tick, (unsigned)atk->startup,
                          (unsigned)(atk->startup + atk->active - 1U));
        }
        EXPECT_TRUE(inside);
        checked++;
    }
    EXPECT_GE(checked, 11U); /* toutes les attaques du joueur et des ennemis */
    fx_done(&f);
}

/* Glissement du pied d'appui : pour chaque phase d'appui (pied à moins de 2 cm de son plus bas, sur
 * au moins 1/20 s), déplacement horizontal NET du pied entre le début et la fin de la phase, divisé
 * par sa durée (m/s), moyenné sur les phases. Le personnage avance à `speed`. */
static float stance_slide(Fx *f, float speed)
{
    EngActorDrive d = {.ground_speed = speed / SCALE, .grounded = true, .state = ENG_CS_MOVE};
    const float dt = 1.0F / 120.0F;
    float travel = 0.0F;
    for (int i = 0; i < 120; i++) { /* une seconde pour sortir du fondu de départ */
        (void)eng_actor_anim_update(&f->anim, &d, dt);
        travel += speed * dt;
    }
    enum { N = 480 };
    static float pos[2][N][3];
    float min_y[2] = {1e9F, 1e9F};
    for (int i = 0; i < N; i++) {
        (void)eng_actor_anim_update(&f->anim, &d, dt);
        travel += speed * dt;
        (void)eng_pose_world(&f->anim.pose, &f->sk, f->world);
        joint_world(f, f->foot_l, travel, pos[0][i]);
        joint_world(f, f->foot_r, travel, pos[1][i]);
        for (int s = 0; s < 2; s++) {
            min_y[s] = fminf(min_y[s], pos[s][i][1]);
        }
    }
    float sum = 0.0F;
    int phases = 0;
    for (int s = 0; s < 2; s++) {
        int start = -1;
        for (int i = 0; i <= N; i++) {
            const bool down = i < N && pos[s][i][1] < min_y[s] + 0.02F;
            if (down && start < 0) {
                start = i;
            } else if (!down && start >= 0) {
                const int len = i - 1 - start;
                if (len >= 3 && start > 0 && i < N) { /* phase complète, >= 1/40 s */
                    const float dx = pos[s][i - 1][0] - pos[s][start][0];
                    const float dz = pos[s][i - 1][2] - pos[s][start][2];
                    sum += sqrtf((dx * dx) + (dz * dz)) / ((float)len * dt);
                    phases++;
                }
                start = -1;
            }
        }
    }
    return phases > 0 ? sum / (float)phases : 1e9F;
}

UTEST(character_motion, the_planted_foot_does_not_slide_at_walk_run_and_sprint_speeds)
{
    static Fx f;
    ASSERT_TRUE(fx_init(&f));
    /* vitesses du jeu : marche lente, marche (vitesse naturelle du clip à l'échelle), mi-chemin,
     * course maximale (4,5 m/s), sprint (x 1,6) */
    const float speeds[5] = {0.672F, 1.5F, 2.84F, 4.5F, 7.2F};
    for (int i = 0; i < 5; i++) {
        ASSERT_TRUE(eng_actor_anim_init(&f.anim, &f.arena, &f.sk, &f.set));
        const float slide = stance_slide(&f, speeds[i]);
        (void)fprintf(stderr, "  %.2f m/s : pied d'appui à %.3f m/s (%.0f %%)\n", (double)speeds[i],
                      (double)slide, (double)(100.0F * slide / speeds[i]));
        /* moins de 15 % de la vitesse du corps : à l'œil, le pied reste planté */
        EXPECT_LT(slide, 0.15F * speeds[i]);
    }
    fx_done(&f);
}

UTEST(character_motion, state_changes_never_make_a_joint_jump)
{
    static Fx f;
    ASSERT_TRUE(fx_init(&f));
    /* idle -> marche -> course -> garde -> touché -> course -> repos -> mort, à 60 images/s ; on
     * compare le plus grand déplacement d'une articulation sur UNE image au moment des changements
     * avec celui des images ordinaires */
    const struct {
        EngCombatState state;
        float speed;
        int frames;
    } script[] = {{ENG_CS_IDLE, 0.0F, 40},  {ENG_CS_MOVE, 1.5F, 40},    {ENG_CS_MOVE, 4.5F, 40},
                  {ENG_CS_BLOCK, 0.0F, 40}, {ENG_CS_HITSTUN, 0.0F, 30}, {ENG_CS_MOVE, 4.5F, 40},
                  {ENG_CS_IDLE, 0.0F, 40},  {ENG_CS_DEAD, 0.0F, 60}};
    /* la vitesse suit les accélérations du jeu (locomotion.h : 30 m/s² pour accélérer, 40 pour
     * freiner) ; le combat, lui, change d'état d'un tick à l'autre */
    float speed = 0.0F;
    static float prev[MAX_JOINTS][3];
    float worst_change = 0.0F;
    float worst_steady = 0.0F;
    bool first = true;
    for (size_t s = 0; s < sizeof script / sizeof script[0]; s++) {
        for (int i = 0; i < script[s].frames; i++) {
            const float target = script[s].speed;
            const float rate = (target > speed ? 30.0F : 40.0F) / 60.0F;
            speed = target > speed ? fminf(target, speed + rate) : fmaxf(target, speed - rate);
            EngActorDrive d = {.ground_speed = speed / SCALE,
                               .grounded = true,
                               .state = script[s].state,
                               .state_ticks = (uint32_t)i};
            ASSERT_TRUE(eng_actor_anim_update(&f.anim, &d, 1.0F / 60.0F));
            ASSERT_TRUE(eng_pose_world(&f.anim.pose, &f.sk, f.world));
            float step = 0.0F;
            for (uint32_t j = 0; j < f.sk.joint_count; j++) {
                float p[3];
                joint_world(&f, j, 0.0F, p);
                if (!first) {
                    step = fmaxf(step, sqrtf(((p[0] - prev[j][0]) * (p[0] - prev[j][0])) +
                                             ((p[1] - prev[j][1]) * (p[1] - prev[j][1])) +
                                             ((p[2] - prev[j][2]) * (p[2] - prev[j][2]))));
                }
                memcpy(prev[j], p, sizeof prev[j]);
            }
            first = false;
            if (i == 0 && s > 0) {
                worst_change = fmaxf(worst_change, step);
                (void)fprintf(stderr, "  changement %zu (%s) : %.3f m\n", s,
                              eng_combat_state_name(script[s].state), (double)step);
            } else {
                worst_steady = fmaxf(worst_steady, step);
            }
        }
    }
    (void)fprintf(stderr,
                  "  plus grand pas d'une articulation : %.3f m au changement, %.3f m sinon\n",
                  (double)worst_change, (double)worst_steady);
    /* un changement d'état ne bouge pas plus qu'une image ordinaire de l'animation la plus vive */
    EXPECT_LE(worst_change, worst_steady);
    EXPECT_LT(worst_change, 0.1F); /* < 10 cm en 1/60 s */
    fx_done(&f);
}
