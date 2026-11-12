#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Ce que le JEU voit des entrées : des actions, jamais des touches. Le même code de jeu marche
 * donc au clavier, à la souris et à la manette : c'est le moteur (input.h) qui relie chaque
 * action à ses touches/boutons/sticks. Cet en-tête ne dépend de rien (ni SDL, ni le moteur), le
 * jeu peut l'inclure. */

typedef enum EngAction {
    ENG_ACTION_ATTACK,
    ENG_ACTION_HEAVY_ATTACK,
    ENG_ACTION_DODGE,
    ENG_ACTION_JUMP,
    ENG_ACTION_INTERACT,
    ENG_ACTION_SPRINT,
    ENG_ACTION_LOCK_ON,
    ENG_ACTION_PAUSE,
    ENG_ACTION_COUNT, /* au plus 32 : les états sont des masques de bits */
} EngAction;

typedef enum EngAxis {
    ENG_AXIS_MOVE, /* x : droite +, y : avant + ; longueur <= 1 */
    ENG_AXIS_LOOK, /* x : droite +, y : haut + ; en « unités de regard » par pas */
    ENG_AXIS_COUNT,
} EngAxis;

typedef struct EngAxisValue {
    float x;
    float y;
} EngAxisValue;

/* Photographie des entrées pour UN pas de simulation.
 *   pressed  : l'action est passée de relâchée à enfoncée depuis le pas précédent
 *   released : l'inverse, depuis le pas précédent
 *   held     : l'action est enfoncée maintenant
 * Une pression plus courte qu'un pas apparaît quand même : pressed ET released sont alors
 * vrais ensemble, held est faux. Aucune pression n'est perdue. */
typedef struct EngInputState {
    uint32_t pressed;
    uint32_t held;
    uint32_t released;
    EngAxisValue axis[ENG_AXIS_COUNT];
    float wheel; /* crans de molette depuis le pas précédent : vers l'avant (haut) > 0 */
} EngInputState;

static inline bool eng_action_pressed(const EngInputState *in, EngAction action)
{
    return (in->pressed >> action & 1u) != 0;
}

static inline bool eng_action_held(const EngInputState *in, EngAction action)
{
    return (in->held >> action & 1u) != 0;
}

static inline bool eng_action_released(const EngInputState *in, EngAction action)
{
    return (in->released >> action & 1u) != 0;
}
