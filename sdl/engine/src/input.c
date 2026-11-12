#include "engine/input.h"

#include <math.h>
#include <string.h>

#include "engine/log.h"

void eng_input_init(EngInput *input)
{
    *input = (EngInput){0};
}

bool eng_input_bind_button(EngInput *input, EngAction action, EngBindKind kind, int code)
{
    if ((int)action < 0 || action >= ENG_ACTION_COUNT || kind == ENG_BIND_NONE) {
        return false;
    }
    for (int i = 0; i < ENG_MAX_BUTTON_BINDINGS; i++) {
        if (input->buttons[action][i].kind == ENG_BIND_NONE) {
            input->buttons[action][i] = (EngButtonBinding){kind, code};
            return true;
        }
    }
    ENG_LOG_WARN("action %d : trop de liaisons (maximum %d)", (int)action, ENG_MAX_BUTTON_BINDINGS);
    return false;
}

void eng_input_clear_action(EngInput *input, EngAction action)
{
    if ((int)action >= 0 && action < ENG_ACTION_COUNT) {
        memset(input->buttons[action], 0, sizeof input->buttons[action]);
    }
}

bool eng_input_bind_axis(EngInput *input, EngAxis axis, const EngAxisBinding *binding)
{
    if ((int)axis < 0 || axis >= ENG_AXIS_COUNT || !(binding->deadzone >= 0.0F) ||
        !(binding->deadzone < 1.0F)) {
        return false;
    }
    input->axes[axis] = *binding;
    return true;
}

/* ── État des actions boutons ── */

static bool source_down(const EngInput *input, EngButtonBinding binding)
{
    switch (binding.kind) {
    case ENG_BIND_KEY:
        return binding.code >= 0 && binding.code < ENG_KEY_CODE_COUNT &&
               input->key_down[binding.code];
    case ENG_BIND_MOUSE_BUTTON:
        return binding.code >= 0 && binding.code < ENG_MOUSE_BUTTON_COUNT &&
               input->mouse_down[binding.code];
    case ENG_BIND_PAD_BUTTON:
        return binding.code >= 0 && binding.code < ENG_PAD_BUTTON_COUNT &&
               input->pad_down[binding.code];
    case ENG_BIND_PAD_TRIGGER:
        return binding.code >= 0 && binding.code < ENG_PAD_AXIS_COUNT &&
               input->pad_axis[binding.code] > ENG_TRIGGER_THRESHOLD;
    case ENG_BIND_NONE:
        break;
    }
    return false;
}

/* Recalcule l'état de chaque action et mémorise les transitions : c'est ici qu'une pression
 * est « enregistrée », même si elle est relâchée avant le prochain pas. */
static void refresh(EngInput *input)
{
    uint32_t now = 0;
    for (int a = 0; a < ENG_ACTION_COUNT; a++) {
        for (int i = 0; i < ENG_MAX_BUTTON_BINDINGS; i++) {
            if (source_down(input, input->buttons[a][i])) {
                now |= 1u << a;
                break;
            }
        }
    }
    input->pressed_latch |= now & ~input->held;
    input->released_latch |= input->held & ~now;
    input->held = now;
}

void eng_input_key(EngInput *input, int scancode, bool down)
{
    if (scancode < 0 || scancode >= ENG_KEY_CODE_COUNT) {
        return;
    }
    input->key_down[scancode] = down;
    refresh(input);
}

void eng_input_mouse_button(EngInput *input, int button, bool down)
{
    if (button < 0 || button >= ENG_MOUSE_BUTTON_COUNT) {
        return;
    }
    input->mouse_down[button] = down;
    refresh(input);
}

void eng_input_mouse_motion(EngInput *input, float dx, float dy)
{
    input->mouse_dx += dx;
    input->mouse_dy += dy;
}

void eng_input_mouse_wheel(EngInput *input, float notches)
{
    input->wheel += notches;
}

void eng_input_pad_button(EngInput *input, int button, bool down)
{
    if (button < 0 || button >= ENG_PAD_BUTTON_COUNT) {
        return;
    }
    input->pad_down[button] = down;
    refresh(input);
}

void eng_input_pad_axis(EngInput *input, int axis, int16_t value)
{
    if (axis < 0 || axis >= ENG_PAD_AXIS_COUNT) {
        return;
    }
    float normalized = value < 0 ? (float)value / 32768.0F : (float)value / 32767.0F;
    input->pad_axis[axis] = normalized;
    refresh(input); /* une gâchette peut être liée à une action */
}

void eng_input_release_all(EngInput *input)
{
    memset(input->key_down, 0, sizeof input->key_down);
    memset(input->mouse_down, 0, sizeof input->mouse_down);
    memset(input->pad_down, 0, sizeof input->pad_down);
    memset(input->pad_axis, 0, sizeof input->pad_axis);
    refresh(input);
}

void eng_input_pad_reset(EngInput *input)
{
    memset(input->pad_down, 0, sizeof input->pad_down);
    memset(input->pad_axis, 0, sizeof input->pad_axis);
    refresh(input);
}

/* ── Axes ── */

EngAxisValue eng_apply_deadzone(float x, float y, float deadzone)
{
    float magnitude = sqrtf(x * x + y * y);
    if (magnitude <= deadzone || magnitude == 0.0F) {
        return (EngAxisValue){0.0F, 0.0F};
    }
    float clamped = magnitude > 1.0F ? 1.0F : magnitude;
    float scale = (clamped - deadzone) / (1.0F - deadzone) / magnitude;
    return (EngAxisValue){x * scale, y * scale};
}

static EngAxisValue clamp_length(EngAxisValue v)
{
    float length = sqrtf(v.x * v.x + v.y * v.y);
    if (length > 1.0F) {
        v.x /= length;
        v.y /= length;
    }
    return v;
}

static EngAxisValue compute_axis(const EngInput *input, const EngAxisBinding *binding)
{
    EngAxisValue v = {0.0F, 0.0F};

    /* Touches : composition de 4 directions */
    if (binding->key_right > 0 && binding->key_right < ENG_KEY_CODE_COUNT &&
        input->key_down[binding->key_right]) {
        v.x += 1.0F;
    }
    if (binding->key_left > 0 && binding->key_left < ENG_KEY_CODE_COUNT &&
        input->key_down[binding->key_left]) {
        v.x -= 1.0F;
    }
    if (binding->key_up > 0 && binding->key_up < ENG_KEY_CODE_COUNT &&
        input->key_down[binding->key_up]) {
        v.y += 1.0F;
    }
    if (binding->key_down > 0 && binding->key_down < ENG_KEY_CODE_COUNT &&
        input->key_down[binding->key_down]) {
        v.y -= 1.0F;
    }

    /* Stick : zone morte radiale ; l'axe Y de SDL pointe vers le BAS, on l'inverse. */
    if (binding->stick != ENG_STICK_NONE) {
        int ax = binding->stick == ENG_STICK_LEFT ? ENG_PAD_AXIS_LEFT_X : ENG_PAD_AXIS_RIGHT_X;
        EngAxisValue stick =
            eng_apply_deadzone(input->pad_axis[ax], input->pad_axis[ax + 1], binding->deadzone);
        v.x += stick.x;
        v.y -= stick.y;
    }

    v = clamp_length(v);

    /* Souris : déplacement cumulé (pas une vitesse), donc hors du plafond de longueur. L'axe Y
     * de l'écran pointe vers le bas : monter la souris regarde vers le haut. */
    if (binding->mouse_sensitivity != 0.0F) {
        v.x += input->mouse_dx * binding->mouse_sensitivity;
        v.y -= input->mouse_dy * binding->mouse_sensitivity;
    }
    return v;
}

void eng_input_begin_tick(EngInput *input, EngInputState *out)
{
    out->pressed = input->pressed_latch;
    out->released = input->released_latch;
    out->held = input->held;
    for (int i = 0; i < ENG_AXIS_COUNT; i++) {
        out->axis[i] = compute_axis(input, &input->axes[i]);
    }

    input->pressed_latch = 0;
    input->released_latch = 0;
    input->mouse_dx = 0.0F;
    input->mouse_dy = 0.0F;
    out->wheel = input->wheel;
    input->wheel = 0.0F;
}
