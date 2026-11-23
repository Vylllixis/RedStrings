#include "engine/anim_blendspace.h"

#include <math.h>
#include <string.h>

bool eng_blendspace_init(EngBlendSpace1D *bs, EngArena *arena, const EngSkeleton *skeleton)
{
    if (bs == NULL) {
        return false;
    }
    memset(bs, 0, sizeof *bs);
    if (arena == NULL || skeleton == NULL) {
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    if (!eng_pose_init(arena, skeleton, &bs->pose) ||
        !eng_pose_init(arena, skeleton, &bs->scratch)) {
        eng_arena_reset_to(arena, mark);
        memset(bs, 0, sizeof *bs);
        return false;
    }
    bs->skeleton = skeleton;
    bs->root_joint = UINT32_MAX;
    return true;
}

bool eng_blendspace_set_root_joint(EngBlendSpace1D *bs, uint32_t joint)
{
    if (bs == NULL || bs->skeleton == NULL ||
        (joint != UINT32_MAX && joint >= bs->skeleton->joint_count)) {
        return false;
    }
    bs->root_joint = joint;
    return true;
}

void eng_blendspace_clear(EngBlendSpace1D *bs)
{
    if (bs == NULL) {
        return;
    }
    memset(bs->samples, 0, sizeof bs->samples);
    bs->count = 0;
    bs->parameter = 0.0F;
    bs->phase = 0.0F;
}

bool eng_blendspace_add(EngBlendSpace1D *bs, const EngAnimClip *clip, float position)
{
    if (bs == NULL || bs->skeleton == NULL || clip == NULL || !isfinite(position) ||
        bs->count >= ENG_BLENDSPACE_MAX_SAMPLES || !isfinite(clip->duration) ||
        !(clip->duration > 0.0F) || (clip->channel_count > 0 && clip->channels == NULL)) {
        return false;
    }
    if (bs->count > 0 && !(position > bs->samples[bs->count - 1U].position)) {
        return false;
    }
    for (uint32_t i = 0; i < clip->channel_count; i++) {
        if (clip->channels[i].joint >= bs->skeleton->joint_count) {
            return false;
        }
    }
    bs->samples[bs->count++] = (EngBlendSample){.clip = clip, .position = position};
    return true;
}

bool eng_blendspace_segment(const EngBlendSpace1D *bs, float parameter, uint32_t *lo, uint32_t *hi,
                            float *t)
{
    if (bs == NULL || bs->count == 0 || lo == NULL || hi == NULL || t == NULL) {
        return false;
    }
    const uint32_t last = bs->count - 1U;
    if (!(parameter > bs->samples[0].position)) { /* aussi pour NaN : premier échantillon */
        *lo = 0;
        *hi = 0;
        *t = 0.0F;
        return true;
    }
    if (parameter >= bs->samples[last].position) {
        *lo = last;
        *hi = last;
        *t = 0.0F;
        return true;
    }
    uint32_t i = 0;
    while (i + 1U < last && parameter >= bs->samples[i + 1U].position) {
        i++;
    }
    const float a = bs->samples[i].position;
    const float b = bs->samples[i + 1U].position;
    *lo = i;
    *hi = i + 1U;
    *t = (parameter - a) / (b - a);
    return true;
}

float eng_blendspace_cycle(const EngBlendSpace1D *bs, float parameter)
{
    uint32_t lo = 0;
    uint32_t hi = 0;
    float t = 0.0F;
    if (!eng_blendspace_segment(bs, parameter, &lo, &hi, &t)) {
        return 0.0F;
    }
    const float da = bs->samples[lo].clip->duration;
    const float db = bs->samples[hi].clip->duration;
    return da + ((db - da) * t);
}

static float wrap_phase(float phase)
{
    float p = fmodf(phase, 1.0F);
    if (p < 0.0F) {
        p += 1.0F;
    }
    return p >= 1.0F ? 0.0F : p;
}

bool eng_blendspace_set_phase(EngBlendSpace1D *bs, float phase)
{
    if (bs == NULL || !isfinite(phase)) {
        return false;
    }
    bs->phase = wrap_phase(phase);
    return true;
}

bool eng_blendspace_update(EngBlendSpace1D *bs, float dt, float parameter)
{
    if (bs == NULL || bs->skeleton == NULL || bs->count == 0) {
        return false;
    }
    if (isfinite(parameter)) {
        const float lowest = bs->samples[0].position;
        const float highest = bs->samples[bs->count - 1U].position;
        bs->parameter = fminf(fmaxf(parameter, lowest), highest);
    } else if (bs->parameter < bs->samples[0].position) {
        bs->parameter = bs->samples[0].position; /* première utilisation sans paramètre valide */
    }
    uint32_t lo = 0;
    uint32_t hi = 0;
    float t = 0.0F;
    if (!eng_blendspace_segment(bs, bs->parameter, &lo, &hi, &t)) {
        return false;
    }
    const float phase_before = bs->phase;
    float advance = 0.0F;
    if (isfinite(dt) && dt > 0.0F) {
        advance = dt / eng_blendspace_cycle(bs, bs->parameter);
        bs->phase = wrap_phase(bs->phase + advance);
    }
    const bool rooted = bs->root_joint != UINT32_MAX;
    EngRootMotion motion_a;
    EngRootMotion motion_b;
    memset(&motion_a, 0, sizeof motion_a);
    memset(&motion_b, 0, sizeof motion_b);
    const EngAnimClip *ca = bs->samples[lo].clip;
    if (!eng_pose_sample(&bs->pose, bs->skeleton, ca, bs->phase * ca->duration,
                         ENG_ANIM_WRAP_CLAMP)) {
        return false;
    }
    if (rooted) {
        const float t0 = phase_before * ca->duration;
        (void)eng_rootmotion_delta(bs->skeleton, ca, bs->root_joint, t0,
                                   t0 + (advance * ca->duration), true, &motion_a);
        (void)eng_rootmotion_strip(&bs->pose, bs->skeleton, ca, bs->root_joint);
    }
    if (hi == lo) {
        bs->motion = motion_a;
        return true;
    }
    const EngAnimClip *cb = bs->samples[hi].clip;
    if (!eng_pose_sample(&bs->scratch, bs->skeleton, cb, bs->phase * cb->duration,
                         ENG_ANIM_WRAP_CLAMP)) {
        return false;
    }
    if (rooted) {
        const float t0 = phase_before * cb->duration;
        (void)eng_rootmotion_delta(bs->skeleton, cb, bs->root_joint, t0,
                                   t0 + (advance * cb->duration), true, &motion_b);
        (void)eng_rootmotion_strip(&bs->scratch, bs->skeleton, cb, bs->root_joint);
    }
    bs->motion = eng_rootmotion_lerp(&motion_a, &motion_b, t);
    return eng_pose_blend(&bs->pose, &bs->pose, &bs->scratch, t);
}
