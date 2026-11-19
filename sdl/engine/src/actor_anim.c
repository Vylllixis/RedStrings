#include "engine/actor_anim.h"

#include <math.h>
#include <string.h>

#include "engine/anim_blend.h"
#include "engine/log.h"
#include "engine/math.h"

#define TICK_SECONDS (1.0F / 60.0F)
#define TWO_PI 6.28318530718F

static const char *const PROGRAM_NAMES[ENG_AP_COUNT] = {
    "locomotion", "jump_start", "jump_air", "jump_land", "attack", "roll",
    "block",      "parry",      "hit",      "stagger",   "death"};

const char *eng_actor_program_name(EngActorProgram program)
{
    return (uint32_t)program < (uint32_t)ENG_AP_COUNT ? PROGRAM_NAMES[program] : "?";
}

static bool clip_fits(const EngAnimClip *clip, const EngSkeleton *skeleton)
{
    if (clip == NULL) {
        return true;
    }
    if (!isfinite(clip->duration) || clip->duration < 0.0F) {
        return false;
    }
    for (uint32_t c = 0; c < clip->channel_count; c++) {
        if (clip->channels[c].joint >= skeleton->joint_count) {
            return false;
        }
    }
    return true;
}

bool eng_actor_anim_init(EngActorAnim *a, EngArena *arena, const EngSkeleton *skeleton,
                         const EngActorSet *set)
{
    if (a == NULL || arena == NULL || skeleton == NULL || set == NULL) {
        return false;
    }
    memset(a, 0, sizeof *a);
    if (!isfinite(set->walk_speed) || !isfinite(set->run_speed) || set->walk_speed <= 0.0F ||
        set->run_speed <= set->walk_speed || !isfinite(set->fade) || set->fade < 0.0F ||
        set->fade > 1.0F || !isfinite(set->speed_smooth) || set->speed_smooth < 0.0F ||
        set->speed_smooth > 1.0F || !isfinite(set->roll_radius) || set->roll_radius < 0.0F ||
        !isfinite(set->roll_pose_time) || !isfinite(set->land_time) || set->land_time < 0.0F ||
        (set->roll_radius > 0.0F && set->root_joint >= skeleton->joint_count)) {
        ENG_LOG_ERROR("acteur : réglages incohérents (vitesses, fondu, roulade)");
        return false;
    }
    for (uint32_t r = 0; r < (uint32_t)ENG_ACTOR_ROLE_COUNT; r++) {
        if (!clip_fits(set->clips[r], skeleton)) {
            ENG_LOG_ERROR("acteur : le clip du rôle %u ne vise pas ce squelette", (unsigned)r);
            return false;
        }
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    if (!eng_pose_init(arena, skeleton, &a->pose) || !eng_pose_init(arena, skeleton, &a->target) ||
        !eng_pose_init(arena, skeleton, &a->source) ||
        !eng_blendspace_init(&a->loco, arena, skeleton)) {
        eng_arena_reset_to(arena, mark);
        memset(a, 0, sizeof *a);
        return false;
    }
    /* repos / marche / course : les positions sont les vitesses au sol (pieds sans glissement) */
    const EngAnimClip *idle = set->clips[ENG_ACTOR_IDLE];
    const EngAnimClip *walk = set->clips[ENG_ACTOR_WALK];
    const EngAnimClip *run = set->clips[ENG_ACTOR_RUN];
    if (idle != NULL && idle->duration > 0.0F) {
        (void)eng_blendspace_add(&a->loco, idle, 0.0F);
    }
    if (walk != NULL && walk->duration > 0.0F) {
        (void)eng_blendspace_add(&a->loco, walk, set->walk_speed);
    }
    if (run != NULL && run->duration > 0.0F) {
        (void)eng_blendspace_add(&a->loco, run, set->run_speed);
    }
    a->skeleton = skeleton;
    a->set = set;
    a->program = ENG_AP_LOCOMOTION;
    a->grounded = true;
    return true;
}

static void copy_pose(EngPose *dst, const EngPose *src)
{
    const size_t n = src->joint_count;
    memcpy(dst->translation, src->translation, n * 3U * sizeof(float));
    memcpy(dst->rotation, src->rotation, n * 4U * sizeof(float));
    memcpy(dst->scale, src->scale, n * 3U * sizeof(float));
}

/* Pose du clip `clip` à `time` dans `out` ; sans clip : repos. */
static void sample_or_rest(EngActorAnim *a, EngPose *out, const EngAnimClip *clip, float time,
                           EngAnimWrap wrap)
{
    if (clip == NULL || !eng_pose_sample(out, a->skeleton, clip, time, wrap)) {
        (void)eng_pose_set_rest(out, a->skeleton);
    }
}

static float clip_duration(const EngAnimClip *clip)
{
    return clip != NULL ? clip->duration : 0.0F;
}

/* Choix du programme selon l'état du jeu (et mise à jour des minuteries de saut). */
static EngActorProgram choose(EngActorAnim *a, const EngActorDrive *d, float dt)
{
    const EngActorSet *s = a->set;
    if (d->grounded) {
        if (!a->grounded && a->air_time > 0.15F) {
            a->land_left = s->land_time; /* vraie retombée, pas un bord de marche */
        }
        a->air_time = 0.0F;
    } else {
        a->air_time += dt;
    }
    a->grounded = d->grounded;
    if (a->land_left > 0.0F) {
        a->land_left = fmaxf(0.0F, a->land_left - dt);
    }
    switch (d->state) {
    case ENG_CS_DEAD:
        return ENG_AP_DEATH;
    case ENG_CS_ATTACK:
        return ENG_AP_ATTACK;
    case ENG_CS_DODGE:
        return ENG_AP_ROLL;
    case ENG_CS_BLOCK:
        return ENG_AP_BLOCK;
    case ENG_CS_PARRY:
        return ENG_AP_PARRY;
    case ENG_CS_HITSTUN:
        return ENG_AP_HIT;
    case ENG_CS_STAGGER:
        return ENG_AP_STAGGER;
    default:
        break;
    }
    if (!d->grounded) {
        return a->air_time < clip_duration(s->clips[ENG_ACTOR_JUMP_START]) ? ENG_AP_JUMP_START
                                                                           : ENG_AP_JUMP_AIR;
    }
    if (a->land_left > 0.0F && d->ground_speed < s->walk_speed) {
        return ENG_AP_JUMP_LAND;
    }
    return ENG_AP_LOCOMOTION;
}

/* Roulade : rotation de la racine de `angle` autour de `axis` (unitaire), autour du pivot situé à
 * `radius` au-dessus de l'origine du squelette : t' = p + R (t - p), q' = R q. */
static void roll_root(EngPose *pose, uint32_t root, const float axis[3], float angle, float radius)
{
    versor r;
    glm_quatv(r, angle, (vec3){axis[0], axis[1], axis[2]});
    float *t = &pose->translation[(size_t)root * 3U];
    float *q = &pose->rotation[(size_t)root * 4U];
    vec3 rel = {t[0], t[1] - radius, t[2]};
    vec3 moved;
    glm_quat_rotatev(r, rel, moved);
    t[0] = moved[0];
    t[1] = moved[1] + radius;
    t[2] = moved[2];
    versor old = {q[0], q[1], q[2], q[3]};
    versor out;
    glm_quat_mul(r, old, out);
    glm_quat_normalize(out);
    memcpy(q, out, sizeof(float) * 4U);
}

static void evaluate(EngActorAnim *a, const EngActorDrive *d, float dt)
{
    const EngActorSet *s = a->set;
    const float tick_time = ((float)d->state_ticks + d->tick_fraction) * TICK_SECONDS;
    switch (a->program) {
    case ENG_AP_LOCOMOTION: {
        float rate = 1.0F;
        if (a->speed > s->run_speed) {
            rate = a->speed / s->run_speed; /* plus vite que la course : cadence accélérée */
        }
        if (a->loco.count == 0U || !eng_blendspace_update(&a->loco, dt * rate, a->speed)) {
            (void)eng_pose_set_rest(&a->target, a->skeleton);
        } else {
            copy_pose(&a->target, &a->loco.pose);
        }
        break;
    }
    case ENG_AP_JUMP_START:
        sample_or_rest(a, &a->target, s->clips[ENG_ACTOR_JUMP_START], a->air_time,
                       ENG_ANIM_WRAP_CLAMP);
        break;
    case ENG_AP_JUMP_AIR:
        sample_or_rest(a, &a->target, s->clips[ENG_ACTOR_JUMP_AIR], a->loop_time,
                       ENG_ANIM_WRAP_LOOP);
        break;
    case ENG_AP_JUMP_LAND:
        sample_or_rest(a, &a->target, s->clips[ENG_ACTOR_JUMP_LAND], s->land_time - a->land_left,
                       ENG_ANIM_WRAP_CLAMP);
        break;
    case ENG_AP_ATTACK: {
        const float speed =
            isfinite(d->attack_speed) && d->attack_speed > 0.0F ? d->attack_speed : 1.0F;
        const float offset = isfinite(d->attack_offset) ? d->attack_offset : 0.0F;
        sample_or_rest(a, &a->target, d->attack_clip, offset + (tick_time * speed),
                       ENG_ANIM_WRAP_CLAMP);
        break;
    }
    case ENG_AP_ROLL: {
        sample_or_rest(a, &a->target, s->clips[ENG_ACTOR_ROLL_POSE], s->roll_pose_time,
                       ENG_ANIM_WRAP_CLAMP);
        const float dx = d->dodge_dir[0];
        const float dz = d->dodge_dir[1];
        const float len = sqrtf((dx * dx) + (dz * dz));
        if (s->roll_radius > 0.0F && len > 1e-4F && isfinite(d->dodge_progress)) {
            /* axe = haut x direction : la tête part vers la direction de la roulade */
            const float axis[3] = {dz / len, 0.0F, -dx / len};
            const float progress = fminf(1.0F, fmaxf(0.0F, d->dodge_progress));
            roll_root(&a->target, s->root_joint, axis, TWO_PI * progress, s->roll_radius);
        }
        break;
    }
    case ENG_AP_BLOCK:
        sample_or_rest(a, &a->target, s->clips[ENG_ACTOR_BLOCK], a->loop_time, ENG_ANIM_WRAP_LOOP);
        break;
    case ENG_AP_PARRY:
        sample_or_rest(a, &a->target, s->clips[ENG_ACTOR_PARRY], tick_time, ENG_ANIM_WRAP_CLAMP);
        break;
    case ENG_AP_HIT:
        sample_or_rest(a, &a->target, s->clips[ENG_ACTOR_HIT], tick_time, ENG_ANIM_WRAP_CLAMP);
        break;
    case ENG_AP_STAGGER:
        sample_or_rest(a, &a->target, s->clips[ENG_ACTOR_STAGGER], tick_time, ENG_ANIM_WRAP_CLAMP);
        break;
    case ENG_AP_DEATH:
    default:
        sample_or_rest(a, &a->target, s->clips[ENG_ACTOR_DEATH], tick_time, ENG_ANIM_WRAP_CLAMP);
        break;
    }
}

bool eng_actor_anim_update(EngActorAnim *a, const EngActorDrive *drive, float dt)
{
    if (a == NULL || drive == NULL || a->set == NULL || !isfinite(dt) || dt < 0.0F ||
        !isfinite(drive->ground_speed) || !isfinite(drive->tick_fraction)) {
        return false;
    }
    /* vitesse lissée (premier ordre) : le mélange repos / marche / course glisse au lieu de sauter
     */
    if (a->set->speed_smooth > 0.0F && a->started) {
        const float k = 1.0F - expf(-dt / a->set->speed_smooth);
        a->speed += (drive->ground_speed - a->speed) * k;
    } else {
        a->speed = drive->ground_speed;
    }
    const EngActorProgram next = choose(a, drive, dt);
    const bool restart = next == ENG_AP_ATTACK && drive->attack_serial != a->serial;
    if (a->started && (next != a->program || restart)) {
        copy_pose(&a->source, &a->pose); /* la pose AFFICHÉE devient la source du fondu */
        /* la mort tombe un peu plus lentement (au moins 0,2 s), sauf si les fondus sont coupés */
        a->fade_total =
            next == ENG_AP_DEATH && a->set->fade > 0.0F ? fmaxf(a->set->fade, 0.2F) : a->set->fade;
        a->fade_left = a->fade_total;
        a->loop_time = 0.0F;
    }
    a->program = next;
    a->serial = drive->attack_serial;
    a->loop_time += dt;
    evaluate(a, drive, dt);
    if (!a->started || a->fade_left <= 0.0F || a->fade_total <= 0.0F) {
        copy_pose(&a->pose, &a->target);
    } else {
        a->fade_left = fmaxf(0.0F, a->fade_left - dt);
        const float w = eng_crossfade_curve(1.0F - (a->fade_left / a->fade_total));
        (void)eng_pose_blend(&a->pose, &a->source, &a->target, w);
    }
    a->started = true;
    return true;
}
