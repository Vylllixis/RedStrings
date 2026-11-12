#include <SDL3/SDL.h>

#include "engine/input.h"

/* Les codes de input.h sont ceux de SDL : on le vérifie à la compilation. */
_Static_assert(ENG_PAD_AXIS_LEFT_X == SDL_GAMEPAD_AXIS_LEFTX, "axe de manette");
_Static_assert(ENG_PAD_AXIS_LEFT_Y == SDL_GAMEPAD_AXIS_LEFTY, "axe de manette");
_Static_assert(ENG_PAD_AXIS_RIGHT_X == SDL_GAMEPAD_AXIS_RIGHTX, "axe de manette");
_Static_assert(ENG_PAD_AXIS_RIGHT_Y == SDL_GAMEPAD_AXIS_RIGHTY, "axe de manette");
_Static_assert(ENG_PAD_AXIS_LEFT_TRIGGER == SDL_GAMEPAD_AXIS_LEFT_TRIGGER, "axe de manette");
_Static_assert(ENG_PAD_AXIS_RIGHT_TRIGGER == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, "axe de manette");
_Static_assert(SDL_GAMEPAD_AXIS_COUNT == ENG_PAD_AXIS_COUNT, "nombre d'axes de manette");
_Static_assert(SDL_SCANCODE_COUNT <= ENG_KEY_CODE_COUNT, "taille du tableau de touches");
_Static_assert(SDL_GAMEPAD_BUTTON_COUNT <= ENG_PAD_BUTTON_COUNT, "taille du tableau de boutons");

/* Touches désignées par leur position PHYSIQUE (scancodes) : sur un clavier AZERTY, « WASD »
 * est la zone ZQSD, ce qui est le bon comportement. */
void eng_input_set_default_bindings(EngInput *input)
{
    eng_input_init(input);

    const EngAxisBinding move = {
        .key_left = SDL_SCANCODE_A,
        .key_right = SDL_SCANCODE_D,
        .key_down = SDL_SCANCODE_S,
        .key_up = SDL_SCANCODE_W,
        .stick = ENG_STICK_LEFT,
        .deadzone = 0.2F,
    };
    const EngAxisBinding look = {
        .stick = ENG_STICK_RIGHT,
        .deadzone = 0.2F,
        .mouse_sensitivity = 0.002F,
    };
    (void)eng_input_bind_axis(input, ENG_AXIS_MOVE, &move);
    (void)eng_input_bind_axis(input, ENG_AXIS_LOOK, &look);

    (void)eng_input_bind_button(input, ENG_ACTION_ATTACK, ENG_BIND_MOUSE_BUTTON, SDL_BUTTON_LEFT);
    (void)eng_input_bind_button(input, ENG_ACTION_ATTACK, ENG_BIND_PAD_BUTTON,
                                SDL_GAMEPAD_BUTTON_WEST);
    (void)eng_input_bind_button(input, ENG_ACTION_HEAVY_ATTACK, ENG_BIND_MOUSE_BUTTON,
                                SDL_BUTTON_RIGHT);
    (void)eng_input_bind_button(input, ENG_ACTION_HEAVY_ATTACK, ENG_BIND_PAD_TRIGGER,
                                ENG_PAD_AXIS_RIGHT_TRIGGER);
    (void)eng_input_bind_button(input, ENG_ACTION_DODGE, ENG_BIND_KEY, SDL_SCANCODE_LCTRL);
    (void)eng_input_bind_button(input, ENG_ACTION_DODGE, ENG_BIND_PAD_BUTTON,
                                SDL_GAMEPAD_BUTTON_EAST);
    (void)eng_input_bind_button(input, ENG_ACTION_JUMP, ENG_BIND_KEY, SDL_SCANCODE_SPACE);
    (void)eng_input_bind_button(input, ENG_ACTION_JUMP, ENG_BIND_PAD_BUTTON,
                                SDL_GAMEPAD_BUTTON_SOUTH);
    (void)eng_input_bind_button(input, ENG_ACTION_INTERACT, ENG_BIND_KEY, SDL_SCANCODE_E);
    (void)eng_input_bind_button(input, ENG_ACTION_INTERACT, ENG_BIND_PAD_BUTTON,
                                SDL_GAMEPAD_BUTTON_NORTH);
    (void)eng_input_bind_button(input, ENG_ACTION_SPRINT, ENG_BIND_KEY, SDL_SCANCODE_LSHIFT);
    (void)eng_input_bind_button(input, ENG_ACTION_SPRINT, ENG_BIND_PAD_BUTTON,
                                SDL_GAMEPAD_BUTTON_LEFT_STICK);
    (void)eng_input_bind_button(input, ENG_ACTION_LOCK_ON, ENG_BIND_KEY, SDL_SCANCODE_Q);
    (void)eng_input_bind_button(input, ENG_ACTION_LOCK_ON, ENG_BIND_PAD_BUTTON,
                                SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    (void)eng_input_bind_button(input, ENG_ACTION_PAUSE, ENG_BIND_KEY, SDL_SCANCODE_P);
    (void)eng_input_bind_button(input, ENG_ACTION_PAUSE, ENG_BIND_PAD_BUTTON,
                                SDL_GAMEPAD_BUTTON_START);
}
