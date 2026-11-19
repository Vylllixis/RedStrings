#ifndef ENG_LOCKON_H
#define ENG_LOCKON_H

#include <stdbool.h>
#include <stdint.h>

/* Verrouillage de cible (M5-15), sur le plan (x, z). Fonctions pures, sans état caché.
 *
 * Règles (docs/combat-design.md §7) :
 *  - acquisition : ennemi vivant, ≠ soi, à ≤ 20 m, dans un cône de 60° d'ouverture TOTALE devant la
 *    caméra (±30° autour de son axe), le plus proche de l'axe de visée ; à égalité d'angle le plus
 *    proche, puis le plus petit indice ;
 *  - bascule : voisin à gauche / à droite dans l'ordre angulaire autour de l'axe de la caméra,
 *    parmi les cibles valides dans le demi-plan avant (±90°) ; aucun voisin → on garde la cible ;
 *  - libération : cible morte, indice invalide, ou distance > 25 m au joueur (hystérésis 20/25).
 * Convention : un cap `yaw` regarde vers (-sin yaw, -cos yaw) ; angle signé positif = à DROITE. */

#define ENG_LOCK_NONE (-1)
#define ENG_LOCK_ACQUIRE_RANGE 20.0F
#define ENG_LOCK_RELEASE_RANGE 25.0F
#define ENG_LOCK_CONE_HALF_ANGLE 0.52359878F /* 30° */

typedef struct EngLockCandidate {
    float pos[2];
    bool alive;
} EngLockCandidate;

/* Indice de la cible à verrouiller, ou ENG_LOCK_NONE. */
int32_t eng_lockon_acquire(const float eye[2], float cam_yaw, const EngLockCandidate *c, uint32_t n,
                           int32_t self);

/* `dir` > 0 : vers la droite ; < 0 : vers la gauche. Retourne la nouvelle cible (ou `current`). */
int32_t eng_lockon_switch(const float eye[2], float cam_yaw, const EngLockCandidate *c, uint32_t n,
                          int32_t self, int32_t current, int dir);

/* Toujours valide ? (sinon l'appelant libère) */
bool eng_lockon_valid(const float player[2], const EngLockCandidate *c, uint32_t n, int32_t target);

/* Cadrage : cap de la caméra et distance pour voir joueur + cible. */
bool eng_lockon_frame(const float player[2], const float target[2], float min_dist, float max_dist,
                      float *yaw, float *distance);

/* Déplacement en « strafe » : `stick` = (droite, avant) ∈ [-1 ; 1]², l'avant étant la direction du
 * joueur vers la cible. Sortie : vecteur monde (x, z) de même norme que le stick (≤ 1). */
bool eng_lockon_move_dir(const float player[2], const float target[2], const float stick[2],
                         float out[2]);

#endif
