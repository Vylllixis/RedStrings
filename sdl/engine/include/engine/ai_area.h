#ifndef ENG_AI_AREA_H
#define ENG_AI_AREA_H

/* Navigation et obstacles de l'IA dans un monde en cellules (M7-11) : une fenêtre carrée de
 * 2 x `half` mètres autour d'un point de focus (le joueur), en coordonnées LOCALES (origine
 * flottante). Ses obstacles viennent des rochers et arbres des cellules prêtes du streaming ; la
 * grille de navigation est reconstruite quand le focus s'éloigne de plus de half/2 du centre, ou
 * quand des cellules arrivent / partent. Un ennemi garde donc un chemin valable quand le joueur
 * passe d'une cellule à l'autre. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/ai_world.h"
#include "engine/navgrid.h"
#include "engine/world_origin.h"
#include "engine/world_stream.h"

typedef struct EngAiArea {
    EngAiWorld world;
    EngNavGrid nav;
    float center[2]; /* local */
    float half;
    float agent_radius;
    uint32_t rebuilds;
    uint32_t cells_seen; /* cellules prêtes au dernier calcul */
    bool valid;
} EngAiArea;

void eng_ai_area_init(EngAiArea *a, float half, float agent_radius);

/* Reconstruit si nécessaire ; true si la grille a changé (les chemins en cache sont à refaire). */
bool eng_ai_area_update(EngAiArea *a, const EngWorldStream *stream, const EngWorldOrigin *origin,
                        const float focus_local[2]);

/* Après un recentrage de l'origine flottante : décale la fenêtre du même vecteur. */
void eng_ai_area_shift(EngAiArea *a, const float shift[3]);

#endif
