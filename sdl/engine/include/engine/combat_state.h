#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Machine à états de gameplay du combat (M5-02). Module pur, sans allocation : il ne connaît ni
 * l'animation ni la physique. Il ne dit pas QUAND changer d'état (c'est le rôle des modules
 * d'attaque, d'esquive, de garde... qui l'utilisent) ; il garantit que le changement est LÉGAL.
 *
 * États : voir docs/combat-design.md §5. La table des transitions légales est une donnée unique
 * (eng_combat_legal) ; tout le reste du combat passe par eng_fsm_request.
 *
 *     EngCombatFsm f;  eng_fsm_init(&f);
 *     if (!eng_fsm_request(&f, ENG_CS_ATTACK)) { ... refusé : état inchangé ... }
 *     eng_fsm_tick(&f);                              // une fois par tick : ticks_in_state++
 *
 * Une transition illégale déclenche ENG_ASSERT (Debug) et est refusée dans tous les cas ; elle est
 * comptée dans `refused` pour les tests et l'affichage de debug. Demander l'état courant n'est ni
 * une transition ni une erreur (retourne true, `ticks` n'est pas remis à 0). */

typedef enum EngCombatState {
    ENG_CS_IDLE = 0,
    ENG_CS_MOVE,
    ENG_CS_ATTACK,
    ENG_CS_DODGE,
    ENG_CS_BLOCK,
    ENG_CS_PARRY,
    ENG_CS_HITSTUN,
    ENG_CS_STAGGER,
    ENG_CS_DEAD,
    ENG_CS_COUNT
} EngCombatState;

typedef struct EngCombatFsm {
    EngCombatState state;
    EngCombatState previous; /* état avant la dernière transition (= state au départ) */
    uint32_t ticks;          /* ticks passés dans l'état courant (0 le tick de l'entrée) */
    uint32_t transitions;    /* nombre de transitions acceptées depuis init */
    uint32_t refused;        /* nombre de transitions illégales refusées */
} EngCombatFsm;

/* Nom stable (« idle », « attack »...) ; « ? » hors bornes. */
const char *eng_combat_state_name(EngCombatState s);

/* La transition from -> to est-elle autorisée ? false si un indice est hors bornes. from == to :
 * false (ce n'est pas une transition ; un « re-déclenchement » d'attaque passe par ATTACK -> ATTACK
 * qui, lui, est autorisé : voir la table). */
bool eng_combat_legal(EngCombatState from, EngCombatState to);

void eng_fsm_init(EngCombatFsm *f);

/* Passe à `to` si la transition est légale (ticks = 0). Même état demandé : true sans effet,
 * sauf ATTACK -> ATTACK (enchaînement de combo) qui est une vraie transition. false et refus
 * compté sinon (+ assertion en Debug). Un état DEAD est définitif : rien n'en sort. */
bool eng_fsm_request(EngCombatFsm *f, EngCombatState to);

/* Un tick passé dans l'état courant. */
void eng_fsm_tick(EngCombatFsm *f);

/* Ensemble des états atteignables depuis `from` (bit i = état i), pour l'affichage de debug. */
uint32_t eng_combat_legal_mask(EngCombatState from);
