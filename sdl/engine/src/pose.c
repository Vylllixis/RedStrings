#include "engine/pose.h"

#include <math.h>
#include <string.h>

#include "engine/log.h"

static bool skeleton_ok(const EngSkeleton *s)
{
    if (s == NULL || s->joints == NULL || s->joint_count == 0 ||
        s->joint_count > ENG_SKELETON_MAX_JOINTS) {
        return false;
    }
    for (uint32_t i = 0; i < s->joint_count; i++) {
        if (s->joints[i].parent >= (int32_t)i || s->joints[i].parent < -1) {
            return false;
        }
    }
    return true;
}

static bool pose_ok(const EngPose *p, const EngSkeleton *s)
{
    return p != NULL && skeleton_ok(s) && p->joint_count == s->joint_count &&
           p->translation != NULL && p->rotation != NULL && p->scale != NULL;
}

float eng_anim_wrap_time(float duration, float time, EngAnimWrap wrap)
{
    if (!isfinite(duration) || !isfinite(time) || duration < 0.0F) {
        return -1.0F;
    }
    if (wrap == ENG_ANIM_WRAP_CLAMP) {
        return fminf(fmaxf(time, 0.0F), duration);
    }
    if (wrap != ENG_ANIM_WRAP_LOOP) {
        return -1.0F;
    }
    if (duration == 0.0F) {
        return 0.0F;
    }
    float t = fmodf(time, duration);
    if (t < 0.0F) {
        t += duration;
    }
    /* fmodf négatif minuscule + duration peut arrondir à duration : on reste dans [0 ; duration[.
     */
    return t >= duration ? 0.0F : t;
}

bool eng_pose_init(EngArena *arena, const EngSkeleton *skeleton, EngPose *out)
{
    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (arena == NULL || !skeleton_ok(skeleton)) {
        ENG_LOG_ERROR("pose: squelette invalide");
        return false;
    }
    const EngArenaMark mark = eng_arena_mark(arena);
    const size_t n = skeleton->joint_count;
    EngPose p = {.joint_count = skeleton->joint_count};
    p.translation = eng_arena_alloc_array(arena, sizeof(float) * 3U, n, 16);
    p.rotation = eng_arena_alloc_array(arena, sizeof(float) * 4U, n, 16);
    p.scale = eng_arena_alloc_array(arena, sizeof(float) * 3U, n, 16);
    if (p.translation == NULL || p.rotation == NULL || p.scale == NULL) {
        eng_arena_reset_to(arena, mark);
        ENG_LOG_ERROR("pose: mémoire insuffisante (%zu articulations)", n);
        return false;
    }
    *out = p;
    return eng_pose_set_rest(out, skeleton);
}

bool eng_pose_set_rest(EngPose *pose, const EngSkeleton *skeleton)
{
    if (!pose_ok(pose, skeleton)) {
        return false;
    }
    for (uint32_t i = 0; i < pose->joint_count; i++) {
        const EngJoint *j = &skeleton->joints[i];
        memcpy(&pose->translation[(size_t)i * 3U], j->translation, sizeof(float) * 3U);
        memcpy(&pose->rotation[(size_t)i * 4U], j->rotation, sizeof(float) * 4U);
        memcpy(&pose->scale[(size_t)i * 3U], j->scale, sizeof(float) * 3U);
    }
    return true;
}

bool eng_pose_sample(EngPose *pose, const EngSkeleton *skeleton, const EngAnimClip *clip,
                     float time, EngAnimWrap wrap)
{
    if (!pose_ok(pose, skeleton) || clip == NULL ||
        (clip->channel_count > 0 && clip->channels == NULL)) {
        return false;
    }
    const float t = eng_anim_wrap_time(clip->duration, time, wrap);
    if (t < 0.0F) {
        ENG_LOG_ERROR("pose: temps %g ou durée %g invalide", (double)time, (double)clip->duration);
        return false;
    }
    /* Valider d'abord : une pose à moitié écrite serait pire qu'un refus net. */
    for (uint32_t c = 0; c < clip->channel_count; c++) {
        if (clip->channels[c].joint >= pose->joint_count ||
            clip->channels[c].path >= (uint32_t)ENG_ANIM_PATH_COUNT) {
            ENG_LOG_ERROR("pose: le clip cible l'articulation %u, le squelette en a %u",
                          clip->channels[c].joint, pose->joint_count);
            return false;
        }
    }
    (void)eng_pose_set_rest(pose, skeleton);
    for (uint32_t c = 0; c < clip->channel_count; c++) {
        const EngAnimChannel *ch = &clip->channels[c];
        float v[4];
        const uint32_t n = eng_anim_sample(clip, c, t, v);
        if (n != eng_anim_path_components(ch->path)) {
            continue; /* clip incohérent : on garde le repos pour ce canal */
        }
        float *dst = ch->path == ENG_ANIM_ROTATION ? &pose->rotation[(size_t)ch->joint * 4U]
                     : ch->path == ENG_ANIM_SCALE  ? &pose->scale[(size_t)ch->joint * 3U]
                                                   : &pose->translation[(size_t)ch->joint * 3U];
        memcpy(dst, v, sizeof(float) * n);
    }
    return true;
}

static void local_matrix(const EngPose *p, uint32_t i, mat4 out)
{
    const float *t = &p->translation[(size_t)i * 3U];
    const float *r = &p->rotation[(size_t)i * 4U];
    const float *s = &p->scale[(size_t)i * 3U];
    mat4 tm;
    mat4 rm;
    mat4 sm;
    mat4 rs;
    glm_translate_make(tm, (vec3){t[0], t[1], t[2]});
    versor q = {r[0], r[1], r[2], r[3]};
    glm_quat_mat4(q, rm);
    glm_scale_make(sm, (vec3){s[0], s[1], s[2]});
    glm_mat4_mul(rm, sm, rs);
    glm_mat4_mul(tm, rs, out);
}

bool eng_pose_world(const EngPose *pose, const EngSkeleton *skeleton, mat4 *out)
{
    if (out == NULL || !pose_ok(pose, skeleton)) {
        return false;
    }
    for (uint32_t i = 0; i < pose->joint_count; i++) {
        mat4 local;
        local_matrix(pose, i, local);
        const int32_t parent = skeleton->joints[i].parent;
        if (parent < 0) {
            glm_mat4_copy(local, out[i]);
        } else {
            glm_mat4_mul(out[parent], local, out[i]);
        }
    }
    return true;
}

bool eng_pose_skin_matrices(const EngSkeleton *skeleton, mat4 *world, mat4 *out)
{
    if (world == NULL || out == NULL || !skeleton_ok(skeleton)) {
        return false;
    }
    for (uint32_t i = 0; i < skeleton->joint_count; i++) {
        mat4 ibm;
        memcpy(ibm, skeleton->joints[i].inverse_bind,
               sizeof(ibm)); /* pas d'alignement SIMD garanti */
        glm_mat4_mul(world[i], ibm, out[i]);
    }
    return true;
}
