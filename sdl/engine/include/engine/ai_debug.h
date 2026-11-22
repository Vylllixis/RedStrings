#ifndef ENG_AI_DEBUG_H
#define ENG_AI_DEBUG_H

/* Débogueur d'IA (M6-10) : état, chemin, cône de vision, cible et jeton d'attaque de chaque
 * ennemi, sélection d'un ennemi par un rayon (clic dans la vue). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/ai_group.h"
#include "engine/debug_lines.h"

typedef struct EngAiDebugInfo {
    uint32_t agent;
    uint32_t fighter;
    char state[16];
    uint32_t state_ticks;
    float hp;
    float max_hp;
    bool sees;
    bool hears;
    bool has_memory;
    float last_known[2];
    bool has_token;
    int32_t plan;
    uint32_t atk_phase;
    uint32_t cooldown[ENG_ARCH_PLANS];
    uint32_t phase;
    float dist_to_player;
    uint32_t path_n;
    uint32_t path_i;
    char archetype[ENG_ARCH_NAME];
} EngAiDebugInfo;

bool eng_aidbg_info(const EngAiGroup *g, const EngCombatSim *sim, uint32_t agent,
                    EngAiDebugInfo *out);

/* Texte multi-lignes prêt pour un panneau d'UI. Retourne le nombre d'octets écrits (hors NUL). */
size_t eng_aidbg_format(const EngAiDebugInfo *info, char *buf, size_t capacity);

/* Rayon (origine, direction unitaire, longueur max) contre les hurtboxes des ennemis vivants :
 * indice d'agent le plus proche touché, ou -1. */
int32_t eng_aidbg_pick(const EngAiGroup *g, const EngCombatSim *sim, const float origin[3],
                       const float dir[3], float max_dist);

/* Dessin : cône de vision, ligne vers la cible (cyan : vue ; jaune : mémoire), chemin restant,
 * anneau jaune au-dessus d'un porteur de jeton, état en couleur au sol. `selected` (>= 0) est
 * dessiné en blanc avec son chemin ; les autres chemins ne sont dessinés que pour lui. */
uint32_t eng_aidbg_draw(const EngAiGroup *g, const EngCombatSim *sim, int32_t selected,
                        EngDebugLines *lines);

#endif
