#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/input_state.h"

/* Système d'entrées par actions (côté moteur).
 *
 * Le moteur reçoit les événements bruts (touche, bouton, mouvement de souris, stick) dans
 * l'ordre où ils arrivent, tient à jour l'état des actions et mémorise « a été pressée » /
 * « a été relâchée » depuis le dernier pas de simulation. eng_input_begin_tick() remet ces
 * mémoires à zéro et fournit la photographie du pas :
 *
 *     eng_platform_pump(&platform);                       // les événements arrivent ici
 *     steps = eng_loop_advance(...);
 *     for each step:
 *         eng_input_begin_tick(&platform.input, &state);  // UNE fois par pas
 *         game->update(game_state, &state, dt);
 *
 * Conséquences voulues :
 *  - affichage plus rapide que la simulation (144 FPS, pas à 60 Hz) : plusieurs images sans pas,
 *    les pressions s'accumulent et sont livrées au pas suivant ;
 *  - plusieurs pas dans la même image (rattrapage) : `pressed` et `released` ne sont livrés
 *    qu'au PREMIER pas, sinon une pression compterait plusieurs fois ; `held` et les axes
 *    restent valables pour tous ;
 *  - une pression qui commence et finit entre deux pas est livrée (pressed et released, held à 0).
 *
 * Codes bruts : ceux de SDL (SDL_Scancode, SDL_BUTTON_*, SDL_GamepadButton, SDL_GamepadAxis).
 * Les liaisons par défaut sont dans input_defaults.c ; elles viendront d'un fichier en M0-20. */

#define ENG_MAX_BUTTON_BINDINGS 4
#define ENG_KEY_CODE_COUNT 512
#define ENG_MOUSE_BUTTON_COUNT 16
#define ENG_PAD_BUTTON_COUNT 32
#define ENG_PAD_AXIS_COUNT 6

/* Indices des axes de manette (valeurs de SDL_GamepadAxis). */
#define ENG_PAD_AXIS_LEFT_X 0
#define ENG_PAD_AXIS_LEFT_Y 1
#define ENG_PAD_AXIS_RIGHT_X 2
#define ENG_PAD_AXIS_RIGHT_Y 3
#define ENG_PAD_AXIS_LEFT_TRIGGER 4
#define ENG_PAD_AXIS_RIGHT_TRIGGER 5

#define ENG_TRIGGER_THRESHOLD 0.5f

typedef enum EngBindKind {
    ENG_BIND_NONE,
    ENG_BIND_KEY,          /* code = SDL_Scancode (position PHYSIQUE : ZQSD d'un AZERTY = WASD) */
    ENG_BIND_MOUSE_BUTTON, /* code = SDL_BUTTON_* */
    ENG_BIND_PAD_BUTTON,   /* code = SDL_GamepadButton */
    ENG_BIND_PAD_TRIGGER,  /* code = ENG_PAD_AXIS_LEFT_TRIGGER ou ..._RIGHT_TRIGGER */
} EngBindKind;

typedef struct EngButtonBinding {
    EngBindKind kind;
    int code;
} EngButtonBinding;

typedef enum EngStick {
    ENG_STICK_NONE,
    ENG_STICK_LEFT,
    ENG_STICK_RIGHT,
} EngStick;

/* Liaison d'un axe : touches (0 = aucune), stick, souris. Les sources s'additionnent ; les
 * touches et le stick sont ramenés à une longueur <= 1 (pas de diagonale plus rapide). */
typedef struct EngAxisBinding {
    int key_left;
    int key_right;
    int key_down;
    int key_up;
    EngStick stick;
    float deadzone;          /* rayon de la zone morte du stick, dans [0, 1) */
    float mouse_sensitivity; /* 0 = la souris n'agit pas sur cet axe */
} EngAxisBinding;

typedef struct EngInput {
    EngButtonBinding buttons[ENG_ACTION_COUNT][ENG_MAX_BUTTON_BINDINGS];
    EngAxisBinding axes[ENG_AXIS_COUNT];

    bool key_down[ENG_KEY_CODE_COUNT];
    bool mouse_down[ENG_MOUSE_BUTTON_COUNT];
    bool pad_down[ENG_PAD_BUTTON_COUNT];
    float pad_axis[ENG_PAD_AXIS_COUNT]; /* sticks dans [-1, 1], gâchettes dans [0, 1] */
    float mouse_dx;                     /* cumulés depuis le dernier pas */
    float mouse_dy;
    float wheel; /* crans de molette cumulés depuis le dernier pas */

    uint32_t held;
    uint32_t pressed_latch;
    uint32_t released_latch;
} EngInput;

void eng_input_init(EngInput *input); /* aucune liaison */
void eng_input_set_default_bindings(EngInput *input);

/* false si l'action a déjà ENG_MAX_BUTTON_BINDINGS liaisons ou si les paramètres sont invalides */
bool eng_input_bind_button(EngInput *input, EngAction action, EngBindKind kind, int code);
bool eng_input_bind_axis(EngInput *input, EngAxis axis, const EngAxisBinding *binding);
/* Retire toutes les liaisons de boutons d'une action (avant de la relier autrement). */
void eng_input_clear_action(EngInput *input, EngAction action);

/* Événements bruts. Les codes hors limites sont ignorés. */
void eng_input_key(EngInput *input, int scancode, bool down);
void eng_input_mouse_button(EngInput *input, int button, bool down);
void eng_input_mouse_motion(EngInput *input, float dx, float dy);
/* Crans de molette (vers l'avant/le haut > 0), cumulés jusqu'au prochain pas. */
void eng_input_mouse_wheel(EngInput *input, float notches);
void eng_input_pad_button(EngInput *input, int button, bool down);
/* `value` dans l'échelle de SDL : -32768..32767 (sticks), 0..32767 (gâchettes). */
void eng_input_pad_axis(EngInput *input, int axis, int16_t value);

/* Fenêtre qui perd le focus : tout est relâché (sinon une touche resterait « bloquée »). */
void eng_input_release_all(EngInput *input);
/* Manette débranchée : ses boutons et axes sont remis à zéro. */
void eng_input_pad_reset(EngInput *input);

/* Photographie pour un pas de simulation, puis remise à zéro des mémoires. */
void eng_input_begin_tick(EngInput *input, EngInputState *out);

/* Zone morte radiale avec remise à l'échelle : 0 sous le rayon, puis croissance continue de 0 à
 * 1 (pas de saut à la sortie de la zone morte). */
EngAxisValue eng_apply_deadzone(float x, float y, float deadzone);
