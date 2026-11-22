#include "engine/locomotion.h"

#include <math.h>
#include <stddef.h>

#define PI_F 3.14159265358979F
#define TWO_PI_F 6.28318530717959F

/* Ramène un angle dans ]-pi ; pi]. */
static float wrap_pi(float a)
{
    a = fmodf(a, TWO_PI_F);
    if (a > PI_F) {
        a -= TWO_PI_F;
    } else if (a <= -PI_F) {
        a += TWO_PI_F;
    }
    return a;
}

bool eng_locomotion_init(EngLocomotion *loco, float facing)
{
    if (loco == NULL || !isfinite(facing)) {
        return false;
    }
    loco->params = (EngLocomotionParams){
        .max_speed = ENG_LOCOMOTION_MAX_SPEED_DEFAULT,
        .accel = ENG_LOCOMOTION_ACCEL_DEFAULT,
        .decel = ENG_LOCOMOTION_DECEL_DEFAULT,
        .reverse_decel = ENG_LOCOMOTION_REVERSE_DECEL_DEFAULT,
        .turn_tau = ENG_LOCOMOTION_TURN_TAU_DEFAULT,
        .turn_max = ENG_LOCOMOTION_TURN_MAX_DEFAULT,
        .deadzone = ENG_LOCOMOTION_DEADZONE_DEFAULT,
    };
    loco->vx = 0.0F;
    loco->vz = 0.0F;
    loco->facing = wrap_pi(facing);
    return true;
}

bool eng_locomotion_set_params(EngLocomotion *loco, const EngLocomotionParams *p)
{
    if (loco == NULL || p == NULL || !isfinite(p->max_speed) || !(p->max_speed > 0.0F) ||
        !isfinite(p->accel) || !(p->accel > 0.0F) || !isfinite(p->decel) || !(p->decel > 0.0F) ||
        !isfinite(p->reverse_decel) || !(p->reverse_decel > 0.0F) || !isfinite(p->turn_tau) ||
        !(p->turn_tau >= 0.0F) || !isfinite(p->turn_max) || !(p->turn_max > 0.0F) ||
        !isfinite(p->deadzone) || !(p->deadzone >= 0.0F) || !(p->deadzone < 1.0F)) {
        return false;
    }
    loco->params = *p;
    return true;
}

/* Direction horizontale unitaire de (x, z) ; false si elle est (presque) nulle. */
static bool flat_unit(const float v[3], float *x, float *z)
{
    const float len = sqrtf((v[0] * v[0]) + (v[2] * v[2]));
    if (!(len > 1e-6F) || !isfinite(len)) {
        return false;
    }
    *x = v[0] / len;
    *z = v[2] / len;
    return true;
}

bool eng_locomotion_update(EngLocomotion *loco, const EngInputState *input,
                           const float cam_forward[3], const float cam_right[3], float speed_scale,
                           float control, float dt)
{
    if (loco == NULL || input == NULL || cam_forward == NULL || cam_right == NULL ||
        !isfinite(speed_scale) || !(speed_scale >= 0.0F) || speed_scale > 4.0F ||
        !isfinite(control) || !(control > 0.0F) || control > 1.0F || !(dt > 0.0F) || dt > 0.1F) {
        return false;
    }
    float fx;
    float fz;
    float rx;
    float rz;
    if (!flat_unit(cam_forward, &fx, &fz) || !flat_unit(cam_right, &rx, &rz)) {
        return false;
    }
    const EngLocomotionParams *p = &loco->params;

    /* Direction voulue dans le monde, longueur = inclinaison du stick (<= 1). */
    EngAxisValue move = input->axis[ENG_AXIS_MOVE];
    if (!isfinite(move.x) || !isfinite(move.y)) {
        move = (EngAxisValue){0.0F, 0.0F};
    }
    float wx = (fx * move.y) + (rx * move.x);
    float wz = (fz * move.y) + (rz * move.x);
    float mag = sqrtf((wx * wx) + (wz * wz));
    if (mag > 1.0F) {
        wx /= mag;
        wz /= mag;
        mag = 1.0F;
    }

    /* Vitesse cible, puis rapprochement de la vitesse actuelle au taux qui convient. */
    const float target_speed = mag * p->max_speed * speed_scale;
    const float tx = mag > 0.0F ? (wx / mag) * target_speed : 0.0F;
    const float tz = mag > 0.0F ? (wz / mag) * target_speed : 0.0F;
    const float speed = sqrtf((loco->vx * loco->vx) + (loco->vz * loco->vz));
    float rate = p->accel;
    if (target_speed > 1e-4F && ((loco->vx * tx) + (loco->vz * tz)) < 0.0F) {
        rate = p->reverse_decel;
    } else if (target_speed <= 1e-4F || target_speed < speed) {
        rate = p->decel; /* relâché, ou allure plus lente que l'actuelle */
    }
    rate *= control;
    const float dx = tx - loco->vx;
    const float dz = tz - loco->vz;
    const float dist = sqrtf((dx * dx) + (dz * dz));
    const float step = rate * dt;
    if (dist <= step) {
        loco->vx = tx;
        loco->vz = tz;
    } else {
        loco->vx += (dx / dist) * step;
        loco->vz += (dz / dist) * step;
    }

    /* Rotation vers la direction voulue : filtre exponentiel, borné en vitesse. */
    if (mag > p->deadzone) {
        const float want = atan2f(-wx, -wz); /* lacet dont l'avant (-sin, -cos) est (wx, wz) */
        const float diff = wrap_pi(want - loco->facing);
        const float k = p->turn_tau > 0.0F ? 1.0F - expf(-dt / p->turn_tau) : 1.0F;
        const float limit = p->turn_max * dt;
        float turn = diff * k;
        turn = turn > limit ? limit : (turn < -limit ? -limit : turn);
        loco->facing = wrap_pi(loco->facing + turn);
    }
    return true;
}

void eng_locomotion_velocity(const EngLocomotion *loco, float out[3])
{
    out[0] = loco->vx;
    out[1] = 0.0F;
    out[2] = loco->vz;
}

float eng_locomotion_speed(const EngLocomotion *loco)
{
    return sqrtf((loco->vx * loco->vx) + (loco->vz * loco->vz));
}

void eng_locomotion_stop(EngLocomotion *loco)
{
    loco->vx = 0.0F;
    loco->vz = 0.0F;
}

float eng_locomotion_facing_lerp(float from, float to, float t)
{
    const float k = t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
    return wrap_pi(from + (wrap_pi(to - from) * k));
}
