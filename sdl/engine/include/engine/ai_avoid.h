#ifndef ENG_AI_AVOID_H
#define ENG_AI_AVOID_H

/* Évitement des arbres et des rochers par les ennemis (idée de Théo, 2026-11-21) : sans calcul de
 * chemin, une règle locale sur la direction de marche de chaque ennemi.
 *
 *   - à plus de ENG_AVOID_MARGIN (1 m) de tout obstacle : la direction voulue est gardée ;
 *   - à moins : la part de la direction qui va VERS l'obstacle est réduite en proportion de la
 *     proximité (rien à 1 m, tout au contact) et changée en pas de côté : l'ennemi contourne ;
 *   - au contact : il s'éloigne droit dehors jusqu'à être revenu à 1 m (état `fleeing`), quoi que
 *     veuille son IA.
 * En plus, eng_avoid_push_out garantit qu'un corps n'est jamais DANS un obstacle. Tout est pur et
 * déterministe (aucun aléa). */

#include <stdbool.h>
#include <stdint.h>

#define ENG_AVOID_MARGIN 1.0F /* distance (m) sous laquelle un obstacle influence la marche */

typedef struct EngAvoidCircle {
    float x; /* centre */
    float z;
    float r; /* rayon du tronc ou du rocher (m) */
} EngAvoidCircle;

typedef struct EngAvoidState {
    bool fleeing;  /* a touché un obstacle : s'en éloigne jusqu'à ENG_AVOID_MARGIN */
    float from[2]; /* centre de l'obstacle touché */
    float from_r;
} EngAvoidState;

/* Corrige `dir` (x, z ; la norme est conservée, une direction nulle devient unitaire en fuite) pour
 * un agent de rayon `radius` en `pos`. Renvoie true si la direction a changé. Arguments invalides
 * (NULL, non finis) : false, rien ne change. */
bool eng_avoid_steer(EngAvoidState *state, const float pos[2], float radius,
                     const EngAvoidCircle *circles, uint32_t count, float dir[2]);

/* Sort `pos` de tout obstacle qu'il chevauche (agent de rayon `radius`). true si déplacé. */
bool eng_avoid_push_out(float pos[2], float radius, const EngAvoidCircle *circles, uint32_t count);

/* Écart (m) entre l'agent et l'obstacle le plus proche (négatif : chevauchement) ; INFINITY sans
 * obstacle. `nearest` (peut être NULL) : son indice. */
float eng_avoid_gap(const float pos[2], float radius, const EngAvoidCircle *circles, uint32_t count,
                    uint32_t *nearest);

#endif
