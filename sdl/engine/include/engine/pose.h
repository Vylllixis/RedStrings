#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/anim_clip.h"
#include "engine/arena.h"
#include "engine/math.h"
#include "engine/skeleton.h"

/* Évaluation de pose (M4-14) : transforme (squelette, clip, temps) en matrices prêtes pour le
 * skinning. Trois étapes, chacune testable seule :
 *   1. eng_pose_sample     : pose LOCALE (translation / rotation / échelle par articulation) ;
 *                            les articulations sans canal gardent leur pose de repos.
 *   2. eng_pose_world      : composition le long de la hiérarchie -> matrices MONDE (espace du
 *                            personnage, pas du monde de jeu).
 *   3. eng_pose_skin_matrices : monde x inverse_bind -> matrice à appliquer à un sommet exprimé
 *                            dans la pose de liaison (c'est ce que lira le vertex shader, M4-15).
 * Rien n'est alloué après eng_pose_init : l'évaluation par image ne touche pas l'arène.
 *
 * Conventions : celles de skeleton.h (colonnes d'abord, quaternions x, y, z, w). */

typedef enum EngAnimWrap {
    ENG_ANIM_WRAP_CLAMP = 0, /* hors de [0 ; durée] : première / dernière image */
    ENG_ANIM_WRAP_LOOP = 1   /* le temps tourne modulo la durée */
} EngAnimWrap;

typedef struct EngPose {
    uint32_t joint_count;
    float *translation; /* 3 flottants par articulation */
    float *rotation;    /* 4 flottants (x, y, z, w) */
    float *scale;       /* 3 flottants */
} EngPose;

/* Instant effectivement lu dans un clip de durée `duration` : CLAMP -> borné à [0 ; duration] ;
 * LOOP -> dans [0 ; duration[ (une durée nulle donne 0). Renvoie -1 si `duration` ou `time` n'est
 * pas fini, si `duration` est négative ou si `wrap` est inconnu. */
float eng_anim_wrap_time(float duration, float time, EngAnimWrap wrap);

/* Alloue (dans `arena`) une pose pour `skeleton` et la met au repos. Squelette invalide (NULL,
 * 0 ou plus de ENG_SKELETON_MAX_JOINTS articulations, parent non précédent) ou mémoire
 * insuffisante : false, `out` à zéro, arène restituée. */
bool eng_pose_init(EngArena *arena, const EngSkeleton *skeleton, EngPose *out);

/* Remet `pose` à la pose de repos de `skeleton` (même nombre d'articulations exigé). */
bool eng_pose_set_rest(EngPose *pose, const EngSkeleton *skeleton);

/* Évalue `clip` à l'instant `time` (voir eng_anim_wrap_time) : repos pour tout, puis écrase les
 * articulations animées. `clip` doit viser ce squelette : un canal dont l'articulation dépasse
 * `joint_count` fait échouer l'appel (false, journalisé, pose INCHANGÉE). Même sanction pour un
 * temps non fini ou des arguments NULL. */
bool eng_pose_sample(EngPose *pose, const EngSkeleton *skeleton, const EngAnimClip *clip,
                     float time, EngAnimWrap wrap);

/* Matrices monde (`joint_count` dans `out`, alignées 16 octets comme tout mat4). Racine : sa
 * matrice locale telle quelle. false si un argument est NULL ou le nombre d'articulations diffère.
 */
bool eng_pose_world(const EngPose *pose, const EngSkeleton *skeleton, mat4 *out);

/* skin[i] = world[i] x inverse_bind[i], pour `joint_count` articulations. Avec la pose de
 * liaison, chaque matrice vaut l'identité (à 1e-4 près si le fichier est cohérent). */
bool eng_pose_skin_matrices(const EngSkeleton *skeleton, mat4 *world, mat4 *out);
