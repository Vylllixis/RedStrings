#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Buffer d'entrées de combat (M5-09). Les actions pressées sont mémorisées `window` ticks (10 par
 * défaut = 167 ms) : une attaque pressée PENDANT la recovery part dès que la fenêtre d'annulation
 * s'ouvre, au lieu d'être perdue. Module pur, en ticks.
 *
 * Priorités entre actions simultanées (la plus haute gagne) : esquive > garde > lourde > légère.
 * eng_ibuf_take(buffer, now, allowed) rend et CONSOMME l'action de plus haute priorité parmi les
 * `allowed` encore valides ; les autres restent en attente jusqu'à expiration (age > window). */

#define ENG_IBUF_DEFAULT_WINDOW 10U
#define ENG_IBUF_SIZE 8U

typedef enum EngCombatAction {
    ENG_ACT_NONE = 0,
    ENG_ACT_LIGHT,
    ENG_ACT_HEAVY,
    ENG_ACT_BLOCK,
    ENG_ACT_DODGE,
    ENG_ACT_COUNT
} EngCombatAction;

#define ENG_ACT_MASK(a) (1U << (unsigned)(a))
#define ENG_ACT_ALL                                                                                \
    (ENG_ACT_MASK(ENG_ACT_LIGHT) | ENG_ACT_MASK(ENG_ACT_HEAVY) | ENG_ACT_MASK(ENG_ACT_BLOCK) |     \
     ENG_ACT_MASK(ENG_ACT_DODGE))

typedef struct EngIbufEntry {
    EngCombatAction action;
    uint32_t tick; /* tick de la pression */
} EngIbufEntry;

typedef struct EngInputBuffer {
    uint32_t window;
    EngIbufEntry entries[ENG_IBUF_SIZE];
    uint32_t count;
} EngInputBuffer;

void eng_ibuf_init(EngInputBuffer *b, uint32_t window);

/* Mémorise `action` pressée au tick `tick`. Buffer plein : la plus ancienne entrée est écrasée.
 * Action hors [LIGHT ; DODGE] : ignorée (false). */
bool eng_ibuf_push(EngInputBuffer *b, EngCombatAction action, uint32_t tick);

/* Priorité (plus grand = plus prioritaire) ; 0 pour NONE / hors bornes. */
uint32_t eng_ibuf_priority(EngCombatAction action);

/* Rend et consomme l'action la plus prioritaire (à priorité égale : la plus ancienne) parmi
 * `allowed` (masque ENG_ACT_MASK) dont l'âge `now - tick` est <= window. NONE si aucune. Les
 * entrées expirées sont supprimées au passage. */
EngCombatAction eng_ibuf_take(EngInputBuffer *b, uint32_t now, uint32_t allowed);

/* Regarde sans consommer. */
EngCombatAction eng_ibuf_peek(const EngInputBuffer *b, uint32_t now, uint32_t allowed);

void eng_ibuf_clear(EngInputBuffer *b);
