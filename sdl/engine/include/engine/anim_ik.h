#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/pose.h"
#include "engine/skeleton.h"

/* IK à deux os (M4-23) : place l'extrémité d'une chaîne cuisse-genou-pied (ou épaule-coude-main)
 * sur une cible, en ne modifiant QUE les rotations locales des deux premières articulations de
 * `pose`.
 *
 * Chaîne : `upper` (cuisse) -> `lower` (genou, enfant direct de `upper`) -> `end` (pied, enfant
 * direct de `lower`). Les longueurs des deux os, mesurées dans la pose COURANTE, sont conservées.
 * Tout est exprimé dans l'espace du personnage (celui de eng_pose_world).
 *
 *   - cible hors d'atteinte : la jambe se tend dans la direction de la cible (le pied s'arrête sur
 * la sphère d'atteinte, à 1 mm d'être tout à fait droite pour éviter l'indétermination du genou) ;
 *   - cible trop près : le pied s'arrête à la portée minimale (|l1 - l2|, au moins 1 mm) ;
 *   - sens de pliage du genou : `pole` (position dans l'espace du personnage ; le genou se plie du
 * côté de ce point) ; NULL : on garde le côté où le genou est déjà ; jambe exactement droite sans
 * pôle : vers l'avant (-Z), ou +X si la chaîne est elle-même alignée sur Z ;
 *   - la torsion de chaque os est conservée (rotation minimale entre l'ancienne et la nouvelle
 *     direction) ; l'orientation du pied n'est PAS modifiée dans le monde : elle suit le genou (à
 *     l'appelant de la recaler, par exemple sur la pente du sol, M4-23 ne le fait pas).
 *
 * `weight` dans [0 ; 1] : 0 laisse la pose intacte, 1 résout complètement, entre les deux on
 * mélange les rotations locales (interpolation normalisée, chemin le plus court).
 *
 * Refusé (false, journalisé, `pose` INCHANGÉE) : argument NULL, nombre d'articulations différent,
 * indices hors du squelette ou chaîne qui n'est pas parent-enfant-enfant, valeur non finie,
 * `weight` hors de [0 ; 1], os de longueur nulle. */
bool eng_ik_two_bone(EngPose *pose, const EngSkeleton *skeleton, uint32_t upper, uint32_t lower,
                     uint32_t end, const float target[3], const float pole[3], float weight);
