#include "engine/jump.h"

#include <math.h>
#include <stddef.h>

#define EPS 1e-4F /* tolérance sur les durées : 6 pas de 1/60 s valent bien 0,1 s */
#define NEVER 1.0e9F

bool eng_jump_init(EngJump *jump)
{
    if (jump == NULL) {
        return false;
    }
    jump->params = (EngJumpParams){ENG_JUMP_COYOTE_DEFAULT, ENG_JUMP_BUFFER_DEFAULT};
    jump->air_time = 0.0F;
    jump->since_press = NEVER;
    jump->grounded_prev = true;
    jump->armed = true;
    jump->left_since_jump = false;
    return true;
}

bool eng_jump_set_params(EngJump *jump, const EngJumpParams *p)
{
    if (jump == NULL || p == NULL || !isfinite(p->coyote) || !(p->coyote >= 0.0F) ||
        p->coyote > 0.5F || !isfinite(p->buffer) || !(p->buffer >= 0.0F) || p->buffer > 0.5F) {
        return false;
    }
    jump->params = *p;
    return true;
}

bool eng_jump_update(EngJump *jump, bool grounded, bool pressed, float dt, EngJumpEvents *events)
{
    if (jump == NULL || !(dt > 0.0F) || dt > 0.1F) {
        return false;
    }
    EngJumpEvents ev = {false, false, false};
    ev.landed = grounded && !jump->grounded_prev;
    ev.left_ground = !grounded && jump->grounded_prev;

    if (grounded) {
        jump->air_time = 0.0F;
        if (jump->left_since_jump) { /* atterri après un saut : le saut est de nouveau permis */
            jump->armed = true;
            jump->left_since_jump = false;
        }
    } else {
        jump->air_time += dt;
        if (!jump->armed) {
            jump->left_since_jump = true;
        }
    }
    jump->since_press = pressed ? 0.0F : fminf(jump->since_press + dt, NEVER);

    const bool can = jump->armed && (grounded || jump->air_time <= jump->params.coyote + EPS);
    if (can && jump->since_press <= jump->params.buffer + EPS) {
        ev.jump = true;
        jump->armed = false;
        jump->left_since_jump = false;
        jump->since_press = NEVER;
    }
    jump->grounded_prev = grounded;
    if (events != NULL) {
        *events = ev;
    }
    return true;
}
