#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Géométrie de combat (M5-05/06/07) : capsules (segment + rayon), distances et balayage. Module
 * pur en flottants simple précision, sans allocation. Unités : mètres.
 *
 * Choix : les zones de touche sont des capsules testées ANALYTIQUEMENT plutôt que par requêtes de
 * chevauchement Jolt. Raisons : résultat déterministe et identique sur toutes les machines
 * (replay M5-18), testable sans monde physique, et les hurtboxes sont cinématiques (suivent
 * l'animation) ; Jolt ne sert qu'à la locomotion. Voir docs/combat-geometry.md. */

typedef struct EngCapsule {
    float a[3];
    float b[3];
    float radius; /* >= 0 */
} EngCapsule;

/* Carré de la distance entre les segments [p1 ; q1] et [p2 ; q2] (dégénérés en points acceptés).
 * Si `c1` / `c2` ne sont pas NULL : les points les plus proches. */
float eng_segseg_dist2(const float p1[3], const float q1[3], const float p2[3], const float q2[3],
                       float c1[3], float c2[3]);

/* Les capsules se touchent-elles (distance des axes <= somme des rayons) ? false si une valeur est
 * non finie ou un rayon négatif. `point` (si non NULL) : milieu des deux points les plus proches.
 */
bool eng_capsule_overlap(const EngCapsule *x, const EngCapsule *y, float point[3]);

/* Balayage : entre deux ticks, `w` va de w0 à w1 et `t` de t0 à t1 (interpolation linéaire des
 * extrémités). Teste l'intersection à des instants assez serrés pour qu'AUCUN passage ne soit
 * manqué : le pas d'un point ne dépasse jamais le plus petit rayon cumulé (rayon w + rayon t), ou
 * 1 cm s'il est nul. Retourne true et l'instant (0 à 1) du premier contact dans *time (si non NULL)
 * et le point de contact dans `point` (si non NULL). Les rayons sont ceux de w0 et t0 (constants).
 * Au plus 256 sous-pas : un déplacement plus grand que 256 fois le rayon peut sauter une cible
 * (cas pathologique, téléportation). */
bool eng_capsule_sweep(const EngCapsule *w0, const EngCapsule *w1, const EngCapsule *t0,
                       const EngCapsule *t1, float *time, float point[3]);

/* Nombre de sous-pas qu'utilisera eng_capsule_sweep (pour les tests et l'affichage de debug). */
uint32_t eng_capsule_sweep_steps(const EngCapsule *w0, const EngCapsule *w1, const EngCapsule *t0,
                                 const EngCapsule *t1);
