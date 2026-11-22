#include "engine/anim_debug.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/log.h"

static bool ready(const EngAnimDebug *d)
{
    return d != NULL && d->skeleton != NULL && d->clip != NULL && d->world != NULL &&
           d->rest_world != NULL && d->trail_world != NULL;
}

bool eng_anim_debug_init(EngAnimDebug *dbg, EngArena *arena, const EngSkeleton *skeleton,
                         const EngAnimClip *clip)
{
    if (dbg == NULL) {
        return false;
    }
    memset(dbg, 0, sizeof *dbg);
    if (arena == NULL || skeleton == NULL || clip == NULL || !isfinite(clip->duration) ||
        clip->duration < 0.0F || (clip->channel_count > 0 && clip->channels == NULL)) {
        ENG_LOG_ERROR("débogueur d'animation : arguments invalides");
        return false;
    }
    for (uint32_t c = 0; c < clip->channel_count; c++) {
        if (clip->channels[c].joint >= skeleton->joint_count) {
            ENG_LOG_ERROR("débogueur d'animation : le clip anime l'articulation %u, le squelette "
                          "n'en a que %u",
                          clip->channels[c].joint, skeleton->joint_count);
            return false;
        }
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    EngAnimDebug d = {.skeleton = skeleton,
                      .clip = clip,
                      .speed = 1.0F,
                      .frame_rate = ENG_ANIM_DEBUG_DEFAULT_RATE,
                      .loop = true,
                      .show_trail = true,
                      .selected = -1};
    const size_t n = skeleton->joint_count;
    if (!eng_pose_init(arena, skeleton, &d.pose) ||
        !eng_pose_init(arena, skeleton, &d.trail_pose)) {
        eng_arena_reset_to(arena, mark);
        return false;
    }
    d.world = eng_arena_alloc_array(arena, sizeof(mat4), n, 16);
    d.rest_world = eng_arena_alloc_array(arena, sizeof(mat4), n, 16);
    d.trail_world = eng_arena_alloc_array(arena, sizeof(mat4), n, 16);
    if (d.world == NULL || d.rest_world == NULL || d.trail_world == NULL ||
        !eng_skeleton_rest_world(skeleton, d.rest_world)) {
        eng_arena_reset_to(arena, mark);
        ENG_LOG_ERROR("débogueur d'animation : mémoire insuffisante");
        return false;
    }
    *dbg = d;
    return eng_anim_debug_update(dbg, 0.0F);
}

/* Ramène les réglages dans leur plage (une valeur saisie à la main peut être n'importe quoi). */
static void sanitize(EngAnimDebug *d)
{
    const float duration = d->clip->duration;
    if (!isfinite(d->time)) {
        d->time = 0.0F;
    }
    d->time = fminf(fmaxf(d->time, 0.0F), duration);
    if (!isfinite(d->speed)) {
        d->speed = 1.0F;
    }
    d->speed = fminf(fmaxf(d->speed, -ENG_ANIM_DEBUG_MAX_SPEED), ENG_ANIM_DEBUG_MAX_SPEED);
    if (!isfinite(d->frame_rate) || d->frame_rate <= 0.0F) {
        d->frame_rate = ENG_ANIM_DEBUG_DEFAULT_RATE;
    }
    const int32_t last = (int32_t)d->skeleton->joint_count - 1;
    d->selected = d->selected < -1 ? -1 : (d->selected > last ? last : d->selected);
}

/* Applique un déplacement `delta` (secondes) au temps : boucle ou butée. Retourne true si on a
 * buté (sans boucle). */
static bool move_time(EngAnimDebug *d, float delta)
{
    const float duration = d->clip->duration;
    float t = d->time + delta;
    if (d->loop) {
        d->time = eng_anim_wrap_time(duration, t, ENG_ANIM_WRAP_LOOP);
        return false;
    }
    const bool hit = (t >= duration && delta > 0.0F) || (t <= 0.0F && delta < 0.0F);
    t = fminf(fmaxf(t, 0.0F), duration);
    d->time = t;
    return hit;
}

static bool evaluate(EngAnimDebug *d)
{
    return eng_pose_sample(&d->pose, d->skeleton, d->clip, d->time, ENG_ANIM_WRAP_CLAMP) &&
           eng_pose_world(&d->pose, d->skeleton, d->world);
}

bool eng_anim_debug_update(EngAnimDebug *dbg, float dt)
{
    if (!ready(dbg)) {
        return false;
    }
    sanitize(dbg);
    if (!dbg->paused && isfinite(dt) && dt > 0.0F && dbg->speed != 0.0F &&
        move_time(dbg, dt * dbg->speed)) {
        dbg->paused = true; /* arrivé au bout sans boucle */
    }
    return evaluate(dbg);
}

bool eng_anim_debug_step(EngAnimDebug *dbg, int32_t frames)
{
    if (!ready(dbg)) {
        return false;
    }
    sanitize(dbg);
    dbg->paused = true;
    (void)move_time(dbg, (float)frames / dbg->frame_rate);
    return evaluate(dbg);
}

bool eng_anim_debug_seek(EngAnimDebug *dbg, float time)
{
    if (!ready(dbg) || !isfinite(time)) {
        return false;
    }
    dbg->time = time;
    sanitize(dbg);
    return evaluate(dbg);
}

uint32_t eng_anim_debug_frame(const EngAnimDebug *dbg)
{
    if (!ready(dbg) || !(dbg->frame_rate > 0.0F)) {
        return 0;
    }
    return (uint32_t)floorf((dbg->time * dbg->frame_rate) + 1.0e-4F);
}

uint32_t eng_anim_debug_frame_count(const EngAnimDebug *dbg)
{
    if (!ready(dbg) || !(dbg->frame_rate > 0.0F)) {
        return 0;
    }
    return (uint32_t)floorf((dbg->clip->duration * dbg->frame_rate) + 1.0e-4F) + 1U;
}

static void bone_line(EngDebugLines *lines, mat4 root, mat4 *world, uint32_t joint, int32_t parent,
                      EngDebugColor color)
{
    vec3 a;
    vec3 b;
    glm_mat4_mulv3(root, (vec3){world[parent][3][0], world[parent][3][1], world[parent][3][2]},
                   1.0F, a);
    glm_mat4_mulv3(root, (vec3){world[joint][3][0], world[joint][3][1], world[joint][3][2]}, 1.0F,
                   b);
    (void)eng_debug_line(lines, a, b, color);
}

uint32_t eng_anim_debug_draw(EngAnimDebug *dbg, mat4 root, EngDebugLines *lines)
{
    if (!ready(dbg) || root == NULL || lines == NULL) {
        return 0;
    }
    const bool was_overlay = lines->overlay_mode;
    eng_debug_lines_set_overlay(lines, true);
    if (dbg->show_rest) {
        (void)eng_skeleton_debug_draw(dbg->skeleton, dbg->rest_world, root, lines, ENG_DEBUG_GRAY);
    }
    const uint32_t bones =
        eng_skeleton_debug_draw(dbg->skeleton, dbg->world, root, lines, ENG_DEBUG_CYAN);
    if (dbg->selected >= 0) {
        const uint32_t sel = (uint32_t)dbg->selected;
        const int32_t parent = dbg->skeleton->joints[sel].parent;
        if (parent >= 0) {
            bone_line(lines, root, dbg->world, sel, parent, ENG_DEBUG_YELLOW);
        }
        mat4 axes;
        glm_mat4_mul(root, dbg->world[sel], axes);
        (void)eng_debug_axes(lines, axes, 0.15F);
        if (dbg->show_trail && dbg->clip->duration > 0.0F) {
            vec3 previous = {0};
            for (uint32_t s = 0; s <= ENG_ANIM_DEBUG_TRAIL_SEGMENTS; s++) {
                const float t =
                    dbg->clip->duration * (float)s / (float)ENG_ANIM_DEBUG_TRAIL_SEGMENTS;
                if (!eng_pose_sample(&dbg->trail_pose, dbg->skeleton, dbg->clip, t,
                                     ENG_ANIM_WRAP_CLAMP) ||
                    !eng_pose_world(&dbg->trail_pose, dbg->skeleton, dbg->trail_world)) {
                    break;
                }
                vec3 point;
                glm_mat4_mulv3(root,
                               (vec3){dbg->trail_world[sel][3][0], dbg->trail_world[sel][3][1],
                                      dbg->trail_world[sel][3][2]},
                               1.0F, point);
                if (s > 0) {
                    (void)eng_debug_line(lines, previous, point, ENG_DEBUG_MAGENTA);
                }
                glm_vec3_copy(point, previous);
            }
        }
    }
    eng_debug_lines_set_overlay(lines, was_overlay);
    return bones;
}

size_t eng_anim_debug_describe(const EngAnimDebug *dbg, char *out, size_t capacity)
{
    if (out == NULL || capacity == 0) {
        return 0;
    }
    out[0] = '\0';
    if (!ready(dbg)) {
        return 0;
    }
    char follow[96] = "aucun";
    if (dbg->selected >= 0 && (uint32_t)dbg->selected < dbg->skeleton->joint_count) {
        const char *name = dbg->skeleton->joints[dbg->selected].name;
        (void)snprintf(follow, sizeof follow, "%d %s", dbg->selected, name != NULL ? name : "");
    }
    const int n =
        snprintf(out, capacity, "t %.2f / %.2f s | image %u / %u | x%.2f | %s | %s | suivi : %s",
                 (double)dbg->time, (double)dbg->clip->duration, eng_anim_debug_frame(dbg),
                 eng_anim_debug_frame_count(dbg), (double)dbg->speed,
                 dbg->paused ? "pause" : "lecture", dbg->loop ? "boucle" : "sans boucle", follow);
    if (n < 0) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)n >= capacity ? capacity - 1U : (size_t)n;
}
