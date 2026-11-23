#include "engine/anim_rootmotion.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265358979F

float eng_rootmotion_yaw(const float q[4])
{
    /* avant (0, 0, -1) tourné par q : f = (-2(xz + wy), -2(yz - wx), -(1 - 2(x² + y²))) */
    const float fx = -2.0F * ((q[0] * q[2]) + (q[3] * q[1]));
    const float fz = -(1.0F - (2.0F * ((q[0] * q[0]) + (q[1] * q[1]))));
    if (fabsf(fx) < 1e-9F && fabsf(fz) < 1e-9F) {
        return 0.0F; /* avant vertical : cap indéfini */
    }
    return atan2f(-fx, -fz);
}

static float wrap_pi(float a)
{
    float r = fmodf(a + PI_F, 2.0F * PI_F);
    if (r <= 0.0F) {
        r += 2.0F * PI_F;
    }
    return r - PI_F; /* dans (-pi ; pi] */
}

/* rotation d'un vecteur (x, z) par `angle` autour de +Y */
static void rotate_xz(float angle, float x, float z, float *ox, float *oz)
{
    const float c = cosf(angle);
    const float s = sinf(angle);
    *ox = (x * c) + (z * s);
    *oz = (-x * s) + (z * c);
}

EngRootMotion eng_rootmotion_compose(const EngRootMotion *a, const EngRootMotion *b)
{
    EngRootMotion r;
    memset(&r, 0, sizeof r);
    float x = 0.0F;
    float z = 0.0F;
    rotate_xz(a->yaw, b->translation[0], b->translation[2], &x, &z);
    r.translation[0] = a->translation[0] + x;
    r.translation[2] = a->translation[2] + z;
    r.yaw = wrap_pi(a->yaw + b->yaw);
    return r;
}

EngRootMotion eng_rootmotion_lerp(const EngRootMotion *a, const EngRootMotion *b, float weight)
{
    const float w = (weight > 0.0F) ? fminf(weight, 1.0F) : 0.0F;
    EngRootMotion r;
    memset(&r, 0, sizeof r);
    r.translation[0] = (a->translation[0] * (1.0F - w)) + (b->translation[0] * w);
    r.translation[2] = (a->translation[2] * (1.0F - w)) + (b->translation[2] * w);
    r.yaw = a->yaw + (wrap_pi(b->yaw - a->yaw) * w); /* chemin le plus court */
    return r;
}

/* translation (x, z) et cap de la racine à l'instant `t` */
static void root_at(const EngSkeleton *sk, const EngAnimClip *clip, uint32_t joint, float t,
                    float *x, float *z, float *yaw)
{
    float v[4];
    const EngJoint *rest = &sk->joints[joint];
    const uint32_t tc = eng_anim_find_channel(clip, joint, ENG_ANIM_TRANSLATION);
    if (tc != UINT32_MAX && eng_anim_sample(clip, tc, t, v) == 3) {
        *x = v[0];
        *z = v[2];
    } else {
        *x = rest->translation[0];
        *z = rest->translation[2];
    }
    const uint32_t rc = eng_anim_find_channel(clip, joint, ENG_ANIM_ROTATION);
    if (rc != UINT32_MAX && eng_anim_sample(clip, rc, t, v) == 4) {
        *yaw = eng_rootmotion_yaw(v);
    } else {
        *yaw = eng_rootmotion_yaw(rest->rotation);
    }
}

/* delta entre deux instants d'un même passage (sans traverser la fin du clip) */
static EngRootMotion segment(const EngSkeleton *sk, const EngAnimClip *clip, uint32_t joint,
                             float a, float b)
{
    float x0 = 0.0F;
    float z0 = 0.0F;
    float y0 = 0.0F;
    float x1 = 0.0F;
    float z1 = 0.0F;
    float y1 = 0.0F;
    root_at(sk, clip, joint, a, &x0, &z0, &y0);
    root_at(sk, clip, joint, b, &x1, &z1, &y1);
    EngRootMotion r;
    memset(&r, 0, sizeof r);
    rotate_xz(-y0, x1 - x0, z1 - z0, &r.translation[0], &r.translation[2]);
    r.yaw = wrap_pi(y1 - y0);
    return r;
}

bool eng_rootmotion_delta(const EngSkeleton *skeleton, const EngAnimClip *clip, uint32_t joint,
                          float t0, float t1, bool loop, EngRootMotion *out)
{
    if (skeleton == NULL || skeleton->joints == NULL || clip == NULL || out == NULL ||
        joint >= skeleton->joint_count || !isfinite(t0) || !isfinite(t1) || t1 < t0 ||
        !isfinite(clip->duration) || clip->duration < 0.0F || (loop && !(clip->duration > 0.0F))) {
        return false;
    }
    const float d = clip->duration;
    EngRootMotion total;
    memset(&total, 0, sizeof total);
    if (!loop) {
        const float a = fminf(fmaxf(t0, 0.0F), d);
        const float b = fminf(fmaxf(t1, 0.0F), d);
        *out = segment(skeleton, clip, joint, a, b);
        return true;
    }
    /* passages par la fin du clip : on enchaîne fin -> début */
    const float c0 = floorf(t0 / d);
    const float c1 = floorf(t1 / d);
    const float a = t0 - (c0 * d);
    const float b = t1 - (c1 * d);
    if (c1 <= c0) {
        *out = segment(skeleton, clip, joint, a, b);
        return true;
    }
    total = segment(skeleton, clip, joint, a, d);
    const EngRootMotion cycle = segment(skeleton, clip, joint, 0.0F, d);
    float whole = c1 - c0 - 1.0F;
    while (whole > 0.0F) {
        total = eng_rootmotion_compose(&total, &cycle);
        whole -= 1.0F;
    }
    const EngRootMotion tail = segment(skeleton, clip, joint, 0.0F, b);
    *out = eng_rootmotion_compose(&total, &tail);
    return true;
}

bool eng_rootmotion_strip(EngPose *pose, const EngSkeleton *skeleton, const EngAnimClip *clip,
                          uint32_t joint)
{
    if (pose == NULL || skeleton == NULL || skeleton->joints == NULL || clip == NULL ||
        pose->translation == NULL || pose->rotation == NULL || joint >= skeleton->joint_count ||
        pose->joint_count != skeleton->joint_count) {
        return false;
    }
    float x = 0.0F;
    float z = 0.0F;
    float yaw0 = 0.0F;
    root_at(skeleton, clip, joint, 0.0F, &x, &z, &yaw0);
    pose->translation[(3U * (size_t)joint) + 0U] = x;
    pose->translation[(3U * (size_t)joint) + 2U] = z;
    /* cap : on retire (yaw courant - yaw initial) en tournant autour de Y, par la gauche */
    float *q = &pose->rotation[4U * (size_t)joint];
    const float yaw_now = eng_rootmotion_yaw(q);
    const float correction = wrap_pi(yaw0 - yaw_now);
    const float s = sinf(correction * 0.5F);
    const float c = cosf(correction * 0.5F);
    /* q' = (0, s, 0, c) * q */
    const float nx = (c * q[0]) + (s * q[2]);
    const float ny = (c * q[1]) + (s * q[3]);
    const float nz = (c * q[2]) - (s * q[0]);
    const float nw = (c * q[3]) - (s * q[1]);
    const float len = sqrtf((nx * nx) + (ny * ny) + (nz * nz) + (nw * nw));
    if (len > 1e-9F) {
        q[0] = nx / len;
        q[1] = ny / len;
        q[2] = nz / len;
        q[3] = nw / len;
    }
    return true;
}
