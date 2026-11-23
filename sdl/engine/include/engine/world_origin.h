#ifndef ENG_WORLD_ORIGIN_H
#define ENG_WORLD_ORIGIN_H

/* Origine flottante (M7-10). Les positions MONDE sont en double ; tout ce qui calcule en float
 * (rendu, physique, IA, combat) travaille en coordonnées LOCALES = monde − origine. Quand le joueur
 * s'éloigne de plus de `rebase_distance` de l'origine, celle-ci saute sur le coin de cellule le
 * plus proche du joueur et chaque système décale ses positions locales du même vecteur (`shift`).
 * Résultat : les floats restent petits (< quelques centaines de mètres) donc précis au dixième de
 * millimètre, même à 10 km ou 100 km de l'origine du monde. */

#include <stdbool.h>
#include <stdint.h>

typedef struct EngWorldOrigin {
    double x;
    double y;
    double z;
    double rebase_distance;
    uint32_t rebases;
} EngWorldOrigin;

void eng_origin_init(EngWorldOrigin *o, double rebase_distance);

/* true si l'origine a bougé ; `shift` (à AJOUTER à toutes les positions locales) est alors rempli,
 * sinon mis à zéro. La hauteur n'est jamais recentrée (le relief reste borné). */
bool eng_origin_update(EngWorldOrigin *o, const double player_world[3], float shift[3]);

void eng_origin_to_local(const EngWorldOrigin *o, const double world[3], float local[3]);
void eng_origin_to_world(const EngWorldOrigin *o, const float local[3], double world[3]);

#endif
