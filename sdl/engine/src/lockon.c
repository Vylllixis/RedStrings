#include "engine/lockon.h"

#include <math.h>
#include <stddef.h>

#define PI_F 3.14159265358979323846F

/* angle signé (rad, + = droite) entre l'axe de visée et la direction de (dx, dz) */
static float signed_angle(float cam_yaw, float dx, float dz)
{
    const float fx = -sinf(cam_yaw);
    const float fz = -cosf(cam_yaw);
    const float rx = cosf(cam_yaw);
    const float rz = -sinf(cam_yaw);
    return atan2f(dx * rx + dz * rz, dx * fx + dz * fz);
}

static bool finite2(const float v[2])
{
    return v != NULL && isfinite(v[0]) && isfinite(v[1]);
}

static bool usable(const EngLockCandidate *c, uint32_t i, int32_t self)
{
    return c[i].alive && (int32_t)i != self && isfinite(c[i].pos[0]) && isfinite(c[i].pos[1]);
}

int32_t eng_lockon_acquire(const float eye[2], float cam_yaw, const EngLockCandidate *c, uint32_t n,
                           int32_t self)
{
    if (!finite2(eye) || !isfinite(cam_yaw) || (n > 0U && c == NULL)) {
        return ENG_LOCK_NONE;
    }
    int32_t best = ENG_LOCK_NONE;
    float best_angle = 0.0F;
    float best_dist = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        if (!usable(c, i, self)) {
            continue;
        }
        const float dx = c[i].pos[0] - eye[0];
        const float dz = c[i].pos[1] - eye[1];
        const float dist = sqrtf(dx * dx + dz * dz);
        if (dist > ENG_LOCK_ACQUIRE_RANGE) {
            continue;
        }
        const float ang = fabsf(signed_angle(cam_yaw, dx, dz));
        if (dist > 1e-6F && ang > ENG_LOCK_CONE_HALF_ANGLE) {
            continue;
        }
        const bool better =
            best == ENG_LOCK_NONE || ang < best_angle || (ang == best_angle && dist < best_dist);
        if (better) {
            best = (int32_t)i;
            best_angle = ang;
            best_dist = dist;
        }
    }
    return best;
}

int32_t eng_lockon_switch(const float eye[2], float cam_yaw, const EngLockCandidate *c, uint32_t n,
                          int32_t self, int32_t current, int dir)
{
    if (!finite2(eye) || !isfinite(cam_yaw) || c == NULL || current < 0 || (uint32_t)current >= n ||
        dir == 0 || !usable(c, (uint32_t)current, self)) {
        return current;
    }
    const float cur = signed_angle(cam_yaw, c[current].pos[0] - eye[0], c[current].pos[1] - eye[1]);
    int32_t best = current;
    float best_gap = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        if (!usable(c, i, self) || (int32_t)i == current) {
            continue;
        }
        const float dx = c[i].pos[0] - eye[0];
        const float dz = c[i].pos[1] - eye[1];
        if (sqrtf(dx * dx + dz * dz) > ENG_LOCK_ACQUIRE_RANGE) {
            continue;
        }
        const float ang = signed_angle(cam_yaw, dx, dz);
        if (fabsf(ang) > PI_F / 2.0F) {
            continue; /* derrière la caméra */
        }
        const float gap = dir > 0 ? ang - cur : cur - ang; /* > 0 : du bon côté */
        if (gap <= 0.0F) {
            continue;
        }
        if (best == current || gap < best_gap) {
            best = (int32_t)i;
            best_gap = gap;
        }
    }
    return best;
}

bool eng_lockon_valid(const float player[2], const EngLockCandidate *c, uint32_t n, int32_t target)
{
    if (!finite2(player) || c == NULL || target < 0 || (uint32_t)target >= n || !c[target].alive ||
        !isfinite(c[target].pos[0]) || !isfinite(c[target].pos[1])) {
        return false;
    }
    const float dx = c[target].pos[0] - player[0];
    const float dz = c[target].pos[1] - player[1];
    return sqrtf(dx * dx + dz * dz) <= ENG_LOCK_RELEASE_RANGE;
}

bool eng_lockon_frame(const float player[2], const float target[2], float min_dist, float max_dist,
                      float *yaw, float *distance)
{
    if (!finite2(player) || !finite2(target) || !isfinite(min_dist) || !isfinite(max_dist) ||
        min_dist <= 0.0F || max_dist < min_dist || yaw == NULL || distance == NULL) {
        return false;
    }
    const float dx = target[0] - player[0];
    const float dz = target[1] - player[1];
    const float sep = sqrtf(dx * dx + dz * dz);
    if (sep > 1e-6F) {
        *yaw = atan2f(-dx, -dz);
    }
    float d = min_dist + 0.5F * sep;
    if (d > max_dist) {
        d = max_dist;
    }
    *distance = d;
    return true;
}

bool eng_lockon_move_dir(const float player[2], const float target[2], const float stick[2],
                         float out[2])
{
    if (!finite2(player) || !finite2(target) || !finite2(stick) || out == NULL) {
        return false;
    }
    float fx = target[0] - player[0];
    float fz = target[1] - player[1];
    const float len = sqrtf(fx * fx + fz * fz);
    if (len < 1e-6F) {
        out[0] = stick[0];
        out[1] = -stick[1]; /* pas de cible nette : avant = -Z */
        return true;
    }
    fx /= len;
    fz /= len;
    out[0] = fx * stick[1] + (-fz) * stick[0];
    out[1] = fz * stick[1] + fx * stick[0];
    return true;
}
