#include "engine/orbit_camera.h"

#include <math.h>
#include <string.h>

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static bool finite3(const float v[3])
{
    return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]);
}

bool eng_orbit_camera_init(EngOrbitCamera *camera, const float pivot[3], float yaw, float pitch,
                           float distance)
{
    if (camera == NULL || pivot == NULL || !finite3(pivot) || !isfinite(yaw) || !isfinite(pitch) ||
        !isfinite(distance)) {
        return false;
    }
    memset(camera, 0, sizeof(*camera));
    memcpy(camera->target, pivot, sizeof(camera->target));
    camera->yaw = yaw;
    camera->want_yaw = yaw;
    camera->pitch = clampf(pitch, ENG_ORBIT_PITCH_MIN, ENG_ORBIT_PITCH_MAX);
    camera->want_pitch = camera->pitch;
    camera->distance = clampf(distance, ENG_ORBIT_DISTANCE_MIN, ENG_ORBIT_DISTANCE_MAX);
    camera->effective = camera->distance;
    camera->clear = camera->distance;
    camera->follow_tau = ENG_ORBIT_FOLLOW_TAU_DEFAULT;
    camera->look_tau = ENG_ORBIT_LOOK_TAU_DEFAULT;
    camera->sensitivity = 1.0F;
    camera->invert_y = false;
    return true;
}

bool eng_orbit_camera_set_sensitivity(EngOrbitCamera *camera, float multiplier)
{
    if (camera == NULL || !(multiplier > 0.0F) || multiplier > ENG_ORBIT_SENSITIVITY_MAX) {
        return false;
    }
    camera->sensitivity = multiplier;
    return true;
}

bool eng_orbit_camera_set_smoothing(EngOrbitCamera *camera, float follow_tau, float look_tau)
{
    if (camera == NULL || !(follow_tau >= 0.0F) || follow_tau > 2.0F || !(look_tau >= 0.0F) ||
        look_tau > 2.0F) {
        return false;
    }
    camera->follow_tau = follow_tau;
    camera->look_tau = look_tau;
    return true;
}

/* Part de l'écart comblée en `dt` par un filtre exponentiel de constante `tau`. */
static float blend(float dt, float tau)
{
    return tau > 0.0F ? 1.0F - expf(-dt / tau) : 1.0F;
}

bool eng_orbit_camera_update(EngOrbitCamera *camera, const EngInputState *input, float dt,
                             const float pivot[3])
{
    if (camera == NULL || input == NULL || pivot == NULL || !finite3(pivot) || !(dt > 0.0F) ||
        dt > 0.1F) {
        return false;
    }
    const EngAxisValue look = input->axis[ENG_AXIS_LOOK];
    if (isfinite(look.x) && isfinite(look.y)) {
        const float unit = ENG_CAMERA_LOOK_UNIT * camera->sensitivity;
        camera->want_yaw -= look.x * unit;
        camera->want_pitch =
            clampf(camera->want_pitch + ((camera->invert_y ? -look.y : look.y) * unit),
                   ENG_ORBIT_PITCH_MIN, ENG_ORBIT_PITCH_MAX);
    }
    if (isfinite(input->wheel) && input->wheel != 0.0F) {
        camera->distance = clampf(camera->distance * powf(ENG_ORBIT_ZOOM_STEP, -input->wheel),
                                  ENG_ORBIT_DISTANCE_MIN, ENG_ORBIT_DISTANCE_MAX);
    }
    camera->effective = camera->distance;

    const float kl = blend(dt, camera->look_tau);
    if (kl >= 1.0F) { /* sans lissage : exactement la valeur demandée (pas d'arrondi du lerp) */
        camera->yaw = camera->want_yaw;
        camera->pitch = camera->want_pitch;
    } else {
        camera->yaw += (camera->want_yaw - camera->yaw) * kl;
        camera->pitch += (camera->want_pitch - camera->pitch) * kl;
    }

    const float kf = blend(dt, camera->follow_tau);
    for (int i = 0; i < 3; i++) {
        camera->target[i] =
            kf >= 1.0F ? pivot[i] : camera->target[i] + ((pivot[i] - camera->target[i]) * kf);
    }
    return true;
}

bool eng_orbit_camera_snap(EngOrbitCamera *camera, const float pivot[3])
{
    if (camera == NULL || pivot == NULL || !finite3(pivot)) {
        return false;
    }
    memcpy(camera->target, pivot, sizeof(camera->target));
    camera->clear = camera->distance;
    return true;
}

bool eng_orbit_camera_collide(EngOrbitCamera *camera, const EngPhysics *world, uint32_t layer_mask,
                              float radius, float dt)
{
    if (camera == NULL || world == NULL || !(radius > 0.0F) || radius > 1.0F || !(dt > 0.0F) ||
        dt > 0.1F) {
        return false;
    }
    float back[3];
    camera->effective = camera->distance;
    eng_orbit_camera_eye(camera, back);
    for (int i = 0; i < 3; i++) {
        back[i] -= camera->target[i]; /* pivot -> oeil, de longueur `distance` */
    }
    float want = camera->distance;
    EngRayHit hit;
    if (eng_physics_spherecast(world, camera->target, back, camera->distance, radius, layer_mask,
                               &hit)) {
        want = clampf(hit.distance, ENG_ORBIT_COLLISION_MIN, camera->distance);
    }
    if (want < camera->clear) {
        camera->clear = want; /* obstacle : tout de suite */
    } else {
        camera->clear += (want - camera->clear) * blend(dt, ENG_ORBIT_RECOVER_TAU);
    }
    camera->clear = clampf(camera->clear, ENG_ORBIT_COLLISION_MIN, camera->distance);
    camera->effective = camera->clear;
    return true;
}

void eng_orbit_camera_eye(const EngOrbitCamera *camera, float out[3])
{
    EngCamera view;
    const float zero[3] = {0.0F, 0.0F, 0.0F};
    eng_camera_init(&view, zero, camera->yaw, camera->pitch);
    float forward[3];
    eng_camera_forward(&view, forward);
    for (int i = 0; i < 3; i++) {
        out[i] = camera->target[i] - (forward[i] * camera->effective);
    }
}

void eng_orbit_camera_apply(const EngOrbitCamera *camera, EngCamera *out)
{
    eng_orbit_camera_eye(camera, out->position);
    out->yaw = camera->yaw;
    out->pitch = camera->pitch;
}

void eng_orbit_camera_flat_forward(const EngOrbitCamera *camera, float out[3])
{
    out[0] = -sinf(camera->yaw);
    out[1] = 0.0F;
    out[2] = -cosf(camera->yaw);
}

void eng_orbit_camera_flat_right(const EngOrbitCamera *camera, float out[3])
{
    out[0] = cosf(camera->yaw);
    out[1] = 0.0F;
    out[2] = -sinf(camera->yaw);
}
