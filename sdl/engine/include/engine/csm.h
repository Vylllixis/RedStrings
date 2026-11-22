#ifndef ENG_CSM_H
#define ENG_CSM_H

/* Ombres en cascade (M7-09), côté CPU : découpage de la profondeur de vue en 2 à 4 cascades et une
 * carte d'ombre (EngShadowMap, M1-17) par cascade.
 *
 * Stabilité (pas de scintillement en déplacement) : chaque cascade englobe sa tranche de frustum
 * par une SPHÈRE dont le rayon ne dépend que des distances de la tranche, du champ de vision et
 * du rapport d'aspect — pas de l'orientation de la caméra : tourner la tête ne change pas la taille
 * des texels. Le centre est ensuite aligné sur la grille des texels (eng_shadow_fit) : avancer de
 * moins d'un texel ne déplace pas l'ombre. Le choix de la cascade par pixel se fait sur la
 * profondeur de vue (eng_csm_pick). */

#include <stdbool.h>
#include <stdint.h>

#include "engine/shadow.h"

#define ENG_CSM_MAX 4U

typedef struct EngCsm {
    uint32_t count;
    float splits[ENG_CSM_MAX + 1U]; /* distances de vue : splits[0] = near, splits[count] = far */
    float radius[ENG_CSM_MAX];
    EngShadowMap maps[ENG_CSM_MAX];
} EngCsm;

typedef struct EngCsmView {
    float position[3];
    float forward[3]; /* unitaire */
    float fov_y;      /* radians */
    float aspect;
    float near_plane;
    float far_plane; /* distance couverte par les ombres (pas forcément le far de la caméra) */
} EngCsmView;

/* Découpage « pratique » : lambda ∈ [0 ; 1] mélange répartition logarithmique (1) et uniforme (0).
 */
bool eng_csm_splits(float near_plane, float far_plane, uint32_t count, float lambda, float *out);

/* Rayon de la sphère englobant la tranche [d0 ; d1] d'un frustum (indépendant de l'orientation),
 * et distance de son centre le long de l'axe de vue. */
void eng_csm_slice_sphere(float d0, float d1, float fov_y, float aspect, float *center_dist,
                          float *radius);

bool eng_csm_update(EngCsm *csm, const EngCsmView *view, const float to_sun[3], uint32_t count,
                    float lambda, uint32_t size);

/* Cascade à utiliser pour un point à la profondeur de vue `view_depth` (count si au-delà). */
uint32_t eng_csm_pick(const EngCsm *csm, float view_depth);

#endif
