#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/anim_clip.h"
#include "engine/pose.h"
#include "engine/skeleton.h"

/* Root motion (M4-20) : le déplacement du personnage vient de l'animation au lieu d'être inventé
 * par le code (les pieds ne glissent plus puisque la distance parcourue est celle du clip).
 *
 * Principe : une articulation RACINE (celle qui porte le déplacement ; chez Mixamo, « Hips ») a une
 * translation et une rotation qui dérivent pendant le clip. On sépare :
 *   - le DELTA de racine entre deux instants (EngRootMotion) : translation horizontale (x, z) et
 *     lacet (rotation autour de Y), exprimés dans le repère DU PERSONNAGE à l'instant de départ
 *     (x = droite, z = arrière, comme la caméra : avancer, c'est z < 0). Le contrôleur de
 *     personnage l'applique : déplacement monde = rotation du personnage x delta.translation.
 *   - la pose « en place » (eng_rootmotion_strip) : la racine est ramenée à sa position / son cap
 *     de la première image du clip, pour que le maillage ne se déplace pas une 2e fois.
 *
 * La hauteur (y) et l'inclinaison (tangage, roulis) de la racine RESTENT dans la pose : le
 * balancement du bassin, un saut, ne sont pas du déplacement de personnage. Le delta a donc
 * toujours translation[1] = 0.
 *
 * Hypothèse (non vérifiée) : l'articulation racine n'a pas de parent (-1). Un clip qui boucle est
 * supposé « cyclique » : la fin enchaîne sur le début, le delta d'un cycle est le delta du début à
 * la fin du clip. */

typedef struct EngRootMotion {
    float translation[3]; /* x, 0, z ; mètres */
    float yaw;            /* radians, rotation autour de +Y (anti-horaire vue d'en haut) */
} EngRootMotion;

/* Cap (lacet) d'un quaternion (x, y, z, w) : angle autour de Y qui amène l'avant (0, 0, -1) sur
 * l'avant tourné, dans (-pi ; pi]. Le quaternion doit être unitaire. */
float eng_rootmotion_yaw(const float q[4]);

/* Enchaîne deux déplacements : `a` PUIS `b` (b est exprimé dans le repère d'arrivée de a). */
EngRootMotion eng_rootmotion_compose(const EngRootMotion *a, const EngRootMotion *b);

/* (1 - w) a + w b, w borné à [0 ; 1] (NaN : 0). */
EngRootMotion eng_rootmotion_lerp(const EngRootMotion *a, const EngRootMotion *b, float w);

/* Delta de la racine `joint` entre les instants t0 et t1 (secondes, t0 <= t1). Sans boucle, les
 * deux sont bornés à [0 ; durée]. Avec boucle, ils sont « déroulés » : t1 peut dépasser la durée
 * (de plusieurs cycles) ; les passages par la fin sont enchaînés (fin du clip -> début). Un clip
 * sans canal sur la racine donne un delta nul. false (out inchangé) si un argument est invalide
 * (NULL, articulation hors squelette, temps non finis, t1 < t0, durée nulle avec boucle). */
bool eng_rootmotion_delta(const EngSkeleton *skeleton, const EngAnimClip *clip, uint32_t joint,
                          float t0, float t1, bool loop, EngRootMotion *out);

/* Pose « en place » : translation x, z et cap de la racine fixés à leur valeur à l'instant 0 de
 * `clip` (la hauteur et le reste sont intacts). false si un argument est invalide. */
bool eng_rootmotion_strip(EngPose *pose, const EngSkeleton *skeleton, const EngAnimClip *clip,
                          uint32_t joint);
