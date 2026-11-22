#ifndef ENG_COMBAT_DEBUG_H
#define ENG_COMBAT_DEBUG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/combat_replay.h"
#include "engine/debug_lines.h"

/* Débogueur de combat (M5-16) : rejoue un replay tick par tick, en avant ET en arrière (la
 * simulation étant déterministe, reculer = repartir de zéro et rejouer jusqu'au tick voulu), avec
 * - un tracé des boîtes : hurtboxes par zone (tête magenta, torse jaune, membres cyan ; GRIS quand
 *   le combattant n'est pas touchable : invincibilité d'esquive, après un coup, mort), armes en
 *   ROUGE pendant les ticks actifs, points de contact du dernier tick en blanc ;
 * - une frise temporelle par combattant (une lettre par tick) pour voir les frames de données :
 *     . repos   m marche   s départ   A actif   r récupération   d esquive   D esquive invulnérable
 *     b garde   P parade   h hitstun  S stagger X mort           # gel d'impact
 *   et une frise des touches : * subie, b gardée, p parée, > portée.
 * Historique : les ENG_CDBG_HISTORY derniers ticks. Le module ne possède rien : le replay et le jeu
 * d'attaques doivent vivre aussi longtemps que lui. */

#define ENG_CDBG_HISTORY 256U

typedef struct EngCombatDebugger {
    const EngReplay *replay;
    const EngAttackSet *attacks;
    EngReplaySetup setup;
    EngCombatSim sim;
    bool show_hurtboxes;
    bool show_weapons;
    bool show_contacts;
    char state_rows[ENG_SIM_MAX][ENG_CDBG_HISTORY];
    char hit_rows[ENG_SIM_MAX][ENG_CDBG_HISTORY];
} EngCombatDebugger;

bool eng_cdbg_init(EngCombatDebugger *d, const EngReplay *replay, const EngAttackSet *attacks,
                   const EngReplaySetup *setup);

/* Tick courant (0 = avant le premier) et nombre total de ticks du replay. */
uint32_t eng_cdbg_tick(const EngCombatDebugger *d);
uint32_t eng_cdbg_length(const EngCombatDebugger *d);

bool eng_cdbg_step(EngCombatDebugger *d);                /* false en fin de replay */
bool eng_cdbg_step_back(EngCombatDebugger *d);           /* false au tick 0 */
bool eng_cdbg_seek(EngCombatDebugger *d, uint32_t tick); /* false si tick > longueur */

/* Lettre d'état du combattant pour la frise. */
char eng_cdbg_state_char(const EngFighter *f, const EngAttackSet *attacks, bool frozen);

/* Frise des `window` derniers ticks (au plus l'historique), terminée par '\0'. Retourne le nombre
 * de caractères écrits (hors '\0'), 0 si arguments invalides. Le dernier caractère = tick courant.
 */
uint32_t eng_cdbg_timeline(const EngCombatDebugger *d, uint32_t fighter, uint32_t window, char *out,
                           size_t cap);
uint32_t eng_cdbg_hitline(const EngCombatDebugger *d, uint32_t fighter, uint32_t window, char *out,
                          size_t cap);

/* Trace les boîtes dans `lines` selon les interrupteurs. Retourne le nombre de capsules tracées
 * (contacts non comptés). */
uint32_t eng_cdbg_draw(const EngCombatDebugger *d, EngDebugLines *lines);

#endif
