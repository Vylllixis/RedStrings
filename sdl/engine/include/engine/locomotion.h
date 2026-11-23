#pragma once

#include <stdbool.h>

#include "engine/input_state.h"

/* Déplacement du personnage RELATIF À LA CAMÉRA : transforme l'axe MOVE (stick ou ZQSD) en une
 * vitesse horizontale voulue, avec accélération, freinage et rotation lissée du personnage vers sa
 * direction de marche. Module pur (ni physique, ni SDL, ni GPU) : il sort une vitesse (m/s) que
 * l'appelant donne à eng_character_update (physics.h), qui gère sol, pentes, marches et gravité.
 *
 *     EngLocomotion loco;  eng_locomotion_init(&loco, 0.0F);
 *     par pas :  eng_locomotion_update(&loco, &input, cam_forward, cam_right, 1.0F, dt);
 *                eng_locomotion_velocity(&loco, v);   // puis eng_character_update(c, dt, v)
 *
 * Conventions : `facing` est un lacet comme celui des caméras (0 = regarde vers -Z, positif vers la
 * gauche). La direction « avant » de l'écran est celle de la caméra aplatie sur le plan horizontal
 * (eng_orbit_camera_flat_forward / flat_right).
 *
 * Trois taux d'accélération, pour que le demi-tour soit net sans glisse : `accel` pour
 * accélérer, `decel` pour ralentir ou s'arrêter (touche relâchée), `reverse_decel` quand la
 * direction voulue s'oppose à la vitesse. Le stick analogique règle l'allure (longueur de MOVE
 * <= 1). `speed_scale` multiplie la vitesse maximale (sprint, M4-09) et `control` multiplie les
 * taux (réduit en l'air, M4-08). */

typedef struct EngLocomotionParams {
    float max_speed;     /* m/s à fond de stick, speed_scale = 1 */
    float accel;         /* m/s² */
    float decel;         /* m/s² */
    float reverse_decel; /* m/s² : direction voulue opposée à la vitesse */
    float turn_tau;      /* s : constante de temps du lissage de la rotation */
    float turn_max;      /* rad/s : vitesse de rotation maximale */
    float deadzone;      /* longueur de MOVE sous laquelle le personnage ne se tourne pas */
} EngLocomotionParams;

#define ENG_LOCOMOTION_MAX_SPEED_DEFAULT 4.5F
#define ENG_LOCOMOTION_ACCEL_DEFAULT 30.0F
#define ENG_LOCOMOTION_DECEL_DEFAULT 40.0F
#define ENG_LOCOMOTION_REVERSE_DECEL_DEFAULT 60.0F
#define ENG_LOCOMOTION_TURN_TAU_DEFAULT 0.06F
#define ENG_LOCOMOTION_TURN_MAX_DEFAULT 18.850F /* 1080 degrés par seconde */
#define ENG_LOCOMOTION_DEADZONE_DEFAULT 0.1F

typedef struct EngLocomotion {
    EngLocomotionParams params;
    float vx; /* vitesse horizontale, m/s (repère du monde) */
    float vz;
    float facing; /* radians, dans ]-pi ; pi] */
} EngLocomotion;

/* Valeurs par défaut, à l'arrêt, tourné vers `facing`. false si `facing` n'est pas fini. */
bool eng_locomotion_init(EngLocomotion *loco, float facing);

/* Remplace les réglages. Refusé (inchangé) si une valeur est non finie, <= 0 (deadzone : dans
 * [0 ; 1[, turn_tau : >= 0). */
bool eng_locomotion_set_params(EngLocomotion *loco, const EngLocomotionParams *params);

/* Un pas de `dt` secondes (0 < dt <= 0,1). `cam_forward` et `cam_right` : directions de la caméra
 * (seules x et z servent, renormalisées ; non nulles). `speed_scale` dans [0 ; 4], `control` dans
 * ]0 ; 1]. false, rien de modifié, si un argument est invalide. */
bool eng_locomotion_update(EngLocomotion *loco, const EngInputState *input,
                           const float cam_forward[3], const float cam_right[3], float speed_scale,
                           float control, float dt);

/* Vitesse voulue (y = 0) et allure (m/s). */
void eng_locomotion_velocity(const EngLocomotion *loco, float out[3]);
float eng_locomotion_speed(const EngLocomotion *loco);

/* Arrête net (téléportation, coup reçu) sans changer l'orientation. */
void eng_locomotion_stop(EngLocomotion *loco);

/* Interpolation d'orientation par le plus court arc, pour l'affichage entre deux pas : le lacet
 * est ramené dans ]-pi ; pi], une interpolation naïve ferait un tour complet au passage de pi. */
float eng_locomotion_facing_lerp(float from, float to, float t);
