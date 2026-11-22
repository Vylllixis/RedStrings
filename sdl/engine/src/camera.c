#include "engine/camera.h"

#include <math.h>

static float clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

void eng_camera_init(EngCamera *camera, const float position[3], float yaw, float pitch)
{
    camera->position[0] = position[0];
    camera->position[1] = position[1];
    camera->position[2] = position[2];
    camera->yaw = yaw;
    camera->pitch = clampf(pitch, -ENG_CAMERA_MAX_PITCH, ENG_CAMERA_MAX_PITCH);
    camera->fov_y = glm_rad(60.0F);
    camera->near_z = 0.1F;
    camera->far_z = 1000.0F;
    camera->speed = ENG_CAMERA_SPEED_DEFAULT;
}

void eng_camera_rotate(EngCamera *camera, float dyaw, float dpitch)
{
    camera->yaw += dyaw;
    camera->pitch = clampf(camera->pitch + dpitch, -ENG_CAMERA_MAX_PITCH, ENG_CAMERA_MAX_PITCH);
}

void eng_camera_set_speed(EngCamera *camera, float speed)
{
    if (speed != speed) { /* NaN */
        return;
    }
    camera->speed = clampf(speed, ENG_CAMERA_SPEED_MIN, ENG_CAMERA_SPEED_MAX);
}

void eng_camera_forward(const EngCamera *camera, float out[3])
{
    const float cp = cosf(camera->pitch);
    out[0] = -sinf(camera->yaw) * cp;
    out[1] = sinf(camera->pitch);
    out[2] = -cosf(camera->yaw) * cp;
}

void eng_camera_right(const EngCamera *camera, float out[3])
{
    out[0] = cosf(camera->yaw);
    out[1] = 0.0F;
    out[2] = -sinf(camera->yaw);
}

void eng_camera_view(const EngCamera *camera, mat4 out)
{
    float forward[3];
    eng_camera_forward(camera, forward);
    float eye[3] = {camera->position[0], camera->position[1], camera->position[2]};
    float target[3] = {eye[0] + forward[0], eye[1] + forward[1], eye[2] + forward[2]};
    eng_look_at(eye, target, ENG_AXIS_UP, out);
}

void eng_camera_projection(const EngCamera *camera, float aspect, mat4 out)
{
    eng_perspective(camera->fov_y, aspect, camera->near_z, camera->far_z, out);
}

void eng_camera_update(EngCamera *camera, const EngInputState *input, float dt)
{
    /* Regard : l'axe LOOK est en « unités de regard » (souris : pixels × sensibilité du
     * lanceur ; stick : de -1 à 1 par pas). Vers la droite : le yaw diminue. */
    const EngAxisValue look = input->axis[ENG_AXIS_LOOK];
    eng_camera_rotate(camera, -look.x * ENG_CAMERA_LOOK_UNIT, look.y * ENG_CAMERA_LOOK_UNIT);

    /* Vitesse : un cran de molette multiplie par ENG_CAMERA_WHEEL_STEP. */
    if (input->wheel != 0.0F) {
        eng_camera_set_speed(camera, camera->speed * powf(ENG_CAMERA_WHEEL_STEP, input->wheel));
    }

    /* Déplacement : MOVE dans le repère de la caméra, montée/descente le long de Y monde. La
     * somme est ramenée à une longueur 1 : pas de diagonale plus rapide. */
    const EngAxisValue move = input->axis[ENG_AXIS_MOVE];
    float vertical = 0.0F;
    if (eng_action_held(input, ENG_ACTION_JUMP)) {
        vertical += 1.0F;
    }
    if (eng_action_held(input, ENG_ACTION_DODGE)) {
        vertical -= 1.0F;
    }
    float forward[3];
    float right[3];
    eng_camera_forward(camera, forward);
    eng_camera_right(camera, right);
    float dir[3];
    for (int i = 0; i < 3; i++) {
        dir[i] = (forward[i] * move.y) + (right[i] * move.x);
    }
    dir[1] += vertical;
    const float length = sqrtf((dir[0] * dir[0]) + (dir[1] * dir[1]) + (dir[2] * dir[2]));
    if (length > 1.0F) {
        for (int i = 0; i < 3; i++) {
            dir[i] /= length;
        }
    }
    float speed = camera->speed;
    if (eng_action_held(input, ENG_ACTION_SPRINT)) {
        speed *= ENG_CAMERA_SPRINT_FACTOR;
    }
    for (int i = 0; i < 3; i++) {
        camera->position[i] += dir[i] * speed * dt;
    }
}

void eng_camera_lerp(const EngCamera *from, const EngCamera *to, float t, EngCamera *out)
{
    const float k = clampf(t, 0.0F, 1.0F);
    EngCamera result = *to; /* `out` peut être `from` ou `to` : on n'écrit qu'à la fin */
    for (int i = 0; i < 3; i++) {
        result.position[i] = from->position[i] + ((to->position[i] - from->position[i]) * k);
    }
    result.yaw = from->yaw + ((to->yaw - from->yaw) * k);
    result.pitch = from->pitch + ((to->pitch - from->pitch) * k);
    *out = result;
}
