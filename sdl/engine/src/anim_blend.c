#include "engine/anim_blend.h"

#include <math.h>
#include <string.h>

#include "engine/log.h"

static float clamp01(float v)
{
    if (!(v > 0.0F)) {
        return 0.0F; /* NaN et négatifs */
    }
    return v > 1.0F ? 1.0F : v;
}

float eng_crossfade_curve(float t)
{
    const float x = clamp01(t);
    return x * x * (3.0F - (2.0F * x));
}

static bool pose_usable(const EngPose *p)
{
    return p != NULL && p->translation != NULL && p->rotation != NULL && p->scale != NULL;
}

bool eng_pose_blend(EngPose *out, const EngPose *a, const EngPose *b, float weight)
{
    if (!pose_usable(out) || !pose_usable(a) || !pose_usable(b) ||
        out->joint_count != a->joint_count || a->joint_count != b->joint_count) {
        return false;
    }
    const float w = clamp01(weight);
    const float k = 1.0F - w;
    for (size_t i = 0; i < a->joint_count; i++) {
        for (uint32_t c = 0; c < 3; c++) {
            out->translation[(3 * i) + c] =
                (a->translation[(3 * i) + c] * k) + (b->translation[(3 * i) + c] * w);
            out->scale[(3 * i) + c] = (a->scale[(3 * i) + c] * k) + (b->scale[(3 * i) + c] * w);
        }
        float qa[4];
        float qb[4];
        memcpy(qa, &a->rotation[4 * i], sizeof qa);
        memcpy(qb, &b->rotation[4 * i], sizeof qb);
        const float dot = (qa[0] * qb[0]) + (qa[1] * qb[1]) + (qa[2] * qb[2]) + (qa[3] * qb[3]);
        const float sign = dot < 0.0F ? -1.0F : 1.0F; /* chemin le plus court */
        float q[4];
        float len2 = 0.0F;
        for (uint32_t c = 0; c < 4; c++) {
            q[c] = (qa[c] * k) + (sign * qb[c] * w);
            len2 += q[c] * q[c];
        }
        if (len2 > 1e-12F) {
            const float inv = 1.0F / sqrtf(len2);
            for (uint32_t c = 0; c < 4; c++) {
                out->rotation[(4 * i) + c] = q[c] * inv;
            }
        } else {
            memcpy(&out->rotation[4 * i], qa, sizeof qa); /* quaternions opposés à w = 0,5 */
        }
    }
    return true;
}

static bool pose_copy(EngPose *dst, const EngPose *src)
{
    if (!pose_usable(dst) || !pose_usable(src) || dst->joint_count != src->joint_count) {
        return false;
    }
    memcpy(dst->translation, src->translation, sizeof(float) * 3U * src->joint_count);
    memcpy(dst->rotation, src->rotation, sizeof(float) * 4U * src->joint_count);
    memcpy(dst->scale, src->scale, sizeof(float) * 3U * src->joint_count);
    return true;
}

bool eng_crossfade_init(EngCrossfade *cf, EngArena *arena, const EngSkeleton *skeleton)
{
    if (cf == NULL) {
        return false;
    }
    memset(cf, 0, sizeof *cf);
    if (arena == NULL || skeleton == NULL) {
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    if (!eng_pose_init(arena, skeleton, &cf->pose) ||
        !eng_pose_init(arena, skeleton, &cf->snapshot) ||
        !eng_pose_init(arena, skeleton, &cf->scratch)) {
        eng_arena_reset_to(arena, mark);
        memset(cf, 0, sizeof *cf);
        return false;
    }
    cf->skeleton = skeleton;
    return true;
}

static bool clip_fits(const EngCrossfade *cf, const EngAnimClip *clip)
{
    if (clip == NULL || !isfinite(clip->duration) || clip->duration < 0.0F ||
        (clip->channel_count > 0 && clip->channels == NULL)) {
        return false;
    }
    for (uint32_t i = 0; i < clip->channel_count; i++) {
        if (clip->channels[i].joint >= cf->skeleton->joint_count) {
            return false;
        }
    }
    return true;
}

bool eng_crossfade_play(EngCrossfade *cf, const EngAnimClip *clip, float fade_seconds, bool loop,
                        float speed, float start_time)
{
    if (cf == NULL || cf->skeleton == NULL || !clip_fits(cf, clip) || !isfinite(fade_seconds) ||
        fade_seconds < 0.0F || !isfinite(speed) || !isfinite(start_time)) {
        return false;
    }
    const bool fading = cf->has_from && cf->elapsed < cf->duration;
    if (cf->to.clip == NULL || fade_seconds == 0.0F) {
        cf->has_from = false; /* rien dont se fondre, ou changement immédiat */
    } else if (fading) {
        (void)pose_copy(&cf->snapshot, &cf->pose); /* continuité : on fige ce qui est affiché */
        cf->from_frozen = true;
        cf->has_from = true;
    } else {
        cf->from = cf->to;
        cf->from_frozen = false;
        cf->has_from = true;
    }
    cf->to = (EngAnimLayer){.clip = clip, .time = start_time, .speed = speed, .loop = loop};
    cf->duration = fade_seconds;
    cf->elapsed = 0.0F;
    cf->finished = false;
    return true;
}

bool eng_anim_layer_advance(EngAnimLayer *l, float dt)
{
    if (l == NULL || l->clip == NULL) {
        return false;
    }
    l->time += dt * l->speed;
    const float d = l->clip->duration;
    if (l->loop) {
        const float t = eng_anim_wrap_time(d, l->time, ENG_ANIM_WRAP_LOOP);
        l->time = t < 0.0F ? 0.0F : t;
        return false;
    }
    if (l->time >= d && l->speed > 0.0F) {
        l->time = d;
        return true;
    }
    if (l->time <= 0.0F && l->speed < 0.0F) {
        l->time = 0.0F;
        return true;
    }
    return false;
}

bool eng_crossfade_update(EngCrossfade *cf, float dt)
{
    if (cf == NULL || cf->skeleton == NULL) {
        return false;
    }
    if (!isfinite(dt) || dt < 0.0F) {
        dt = 0.0F;
    }
    if (cf->to.clip == NULL) {
        return true; /* rien à jouer : la pose reste au repos */
    }
    cf->finished = eng_anim_layer_advance(&cf->to, dt) || (cf->finished && !cf->to.loop);
    if (cf->has_from) {
        if (!cf->from_frozen) {
            (void)eng_anim_layer_advance(&cf->from, dt);
        }
        cf->elapsed += dt;
        if (cf->elapsed >= cf->duration) {
            cf->has_from = false;
        }
    }
    if (!eng_pose_sample(&cf->pose, cf->skeleton, cf->to.clip, cf->to.time, ENG_ANIM_WRAP_CLAMP)) {
        return false;
    }
    if (!cf->has_from) {
        return true;
    }
    const EngPose *src = &cf->snapshot;
    if (!cf->from_frozen) {
        if (!eng_pose_sample(&cf->scratch, cf->skeleton, cf->from.clip, cf->from.time,
                             ENG_ANIM_WRAP_CLAMP)) {
            return false;
        }
        src = &cf->scratch;
    }
    return eng_pose_blend(&cf->pose, src, &cf->pose, eng_crossfade_weight(cf));
}

bool eng_crossfade_active(const EngCrossfade *cf)
{
    return cf != NULL && cf->has_from;
}

float eng_crossfade_weight(const EngCrossfade *cf)
{
    if (cf == NULL || !cf->has_from || !(cf->duration > 0.0F)) {
        return 1.0F;
    }
    return eng_crossfade_curve(cf->elapsed / cf->duration);
}
