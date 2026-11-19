#pragma once

#include <stdbool.h>

/* Décision de saut avec DÉLAI DE GRÂCE (coyote time) et MÉMORISATION DE LA TOUCHE (jump buffer).
 * Module pur : il ne voit que « est-on au sol ? » et « la touche vient-elle d'être pressée ? », et
 * dit s'il faut sauter ce pas. L'appelant applique le saut (eng_character_jump /
 * eng_character_launch).
 *
 *   coyote : on peut encore sauter `coyote` secondes après avoir quitté un rebord (0,10 s)
 *   buffer : une touche pressée `buffer` secondes AVANT de toucher le sol déclenche le saut à
 *            l'atterrissage (0,12 s)
 *
 *     EngJump jump;  eng_jump_init(&jump);
 *     par pas :  EngJumpEvents ev;
 *                eng_jump_update(&jump, grounded, eng_action_pressed(&in, ENG_ACTION_JUMP), dt,
 * &ev); if (ev.jump) { eng_character_launch(c, speed); }
 *
 * Pas de double saut : après un saut, le délai de grâce est désarmé jusqu'au prochain atterrissage.
 * Les durées sont mesurées en temps de simulation (somme des dt), donc identiques à 30 ou 144 FPS.
 */

#define ENG_JUMP_COYOTE_DEFAULT 0.10F
#define ENG_JUMP_BUFFER_DEFAULT 0.12F

typedef struct EngJumpParams {
    float coyote; /* s, >= 0, <= 0,5 */
    float buffer; /* s, >= 0, <= 0,5 */
} EngJumpParams;

typedef struct EngJump {
    EngJumpParams params;
    float air_time;    /* s depuis la dernière fois au sol */
    float since_press; /* s depuis la dernière pression de la touche */
    bool grounded_prev;
    bool armed; /* un saut est permis (faux entre un saut et l'atterrissage suivant) */
    bool left_since_jump;
} EngJump;

typedef struct EngJumpEvents {
    bool jump;        /* sauter MAINTENANT */
    bool landed;      /* vient de toucher le sol */
    bool left_ground; /* vient de quitter le sol (sans sauter ou en sautant) */
} EngJumpEvents;

/* Réglages par défaut, au sol. */
bool eng_jump_init(EngJump *jump);
/* Refusé (inchangé) si une durée est non finie, négative ou > 0,5 s. */
bool eng_jump_set_params(EngJump *jump, const EngJumpParams *params);

/* Un pas de `dt` secondes (0 < dt <= 0,1). `events` peut être NULL. false, rien de modifié, si un
 * argument est invalide. */
bool eng_jump_update(EngJump *jump, bool grounded, bool pressed, float dt, EngJumpEvents *events);
