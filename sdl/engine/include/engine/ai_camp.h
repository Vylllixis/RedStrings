#ifndef ENG_AI_CAMP_H
#define ENG_AI_CAMP_H

/* Camps et apparitions (M6-11) : un camp est une liste d'ennemis (archétype, poste, ronde)
 * créés ensemble et remis à neuf au repos du joueur. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/ai_group.h"

typedef struct EngCampMember {
    uint32_t arch;
    float pos[2];
    float yaw;
    float patrol[ENG_AI_PATROL_MAX][2];
    uint32_t patrol_n;
} EngCampMember;

typedef struct EngCamp {
    EngCampMember m[ENG_AI_MAX_AGENTS];
    uint32_t count;
} EngCamp;

void eng_camp_init(EngCamp *c);
bool eng_camp_add(EngCamp *c, uint32_t arch, float x, float z, float yaw, const float (*patrol)[2],
                  uint32_t patrol_n);

/* Crée les ennemis du camp dans le groupe : combattants `first`, `first + 1`, ... */
bool eng_camp_spawn(const EngCamp *c, EngAiGroup *g, EngCombatSim *sim, uint32_t first);

/* Repos du joueur : remet TOUT le camp à neuf (morts compris : ils réapparaissent). Refusé
 * (false, rien ne change) tant qu'un ennemi vivant poursuit, engage ou fuit. */
bool eng_camp_rest(const EngCamp *c, EngAiGroup *g, EngCombatSim *sim);

/* Tous les ennemis du camp sont morts ? */
bool eng_camp_cleared(const EngAiGroup *g, const EngCombatSim *sim);

/* Arène rectangulaire fermée (4 murs d'épaisseur 1 m) centrée en (cx, cz) : demi-côtés hx, hz
 * (M6-09 : arène dédiée du boss). */
bool eng_arena_walls(EngAiWorld *w, float cx, float cz, float hx, float hz);

#endif
