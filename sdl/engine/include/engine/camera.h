#pragma once

#include <stdbool.h>

#include "engine/input_state.h"
#include "engine/math.h"

/* Caméra libre de debug : vol dans la scène à la souris + clavier, ou à la manette. Module pur
 * (pas de SDL, pas de GPU) : il ne lit que EngInputState, donc il se teste sans fenêtre.
 *
 * Conventions (docs/conventions.md) : main droite, Y vers le haut, mètres, radians.
 *   yaw = 0, pitch = 0 : la caméra regarde vers -Z. Yaw positif : tourne vers la GAUCHE (sens
 *   antihoraire vu du dessus). Pitch positif : regarde vers le haut. Le pitch est borné à
 *   ±ENG_CAMERA_MAX_PITCH (jamais la verticale exacte : la vue ne se retourne pas). Le yaw n'est
 *   PAS ramené dans ]-π, π] : l'interpolation entre deux états (eng_camera_lerp) reste ainsi
 *   continue.
 *
 * Commandes (via les actions et axes du moteur, donc reliables dans config.ini) :
 *   axe MOVE : avant/arrière/côtés (ZQSD/WASD, stick gauche) ; JUMP : monter ; DODGE : descendre ;
 *   SPRINT (maintenu) : vitesse × ENG_CAMERA_SPRINT_FACTOR ; axe LOOK : regard (souris, stick
 *   droit) ; molette : vitesse × ENG_CAMERA_WHEEL_STEP par cran (vers le haut : plus vite). */

#define ENG_CAMERA_LOOK_UNIT 0.03F    /* radians par « unité de regard » (input_state.h) */
#define ENG_CAMERA_MAX_PITCH 1.5533F  /* 89° */
#define ENG_CAMERA_SPEED_MIN 0.5F     /* m/s */
#define ENG_CAMERA_SPEED_MAX 500.0F   /* m/s */
#define ENG_CAMERA_SPEED_DEFAULT 6.0F /* m/s */
#define ENG_CAMERA_SPRINT_FACTOR 4.0F
#define ENG_CAMERA_WHEEL_STEP 1.25F

typedef struct EngCamera {
    float position[3]; /* mètres */
    float yaw;         /* radians */
    float pitch;       /* radians, dans [-ENG_CAMERA_MAX_PITCH, +ENG_CAMERA_MAX_PITCH] */
    float fov_y;       /* champ vertical, radians */
    float near_z;      /* > 0 */
    float far_z;       /* > near_z */
    float speed;       /* m/s, dans [ENG_CAMERA_SPEED_MIN, ENG_CAMERA_SPEED_MAX] */
} EngCamera;

/* Position, yaw et pitch (bornés) ; champ de vision 60°, plans 0,1 m et 1000 m, vitesse par défaut.
 */
void eng_camera_init(EngCamera *camera, const float position[3], float yaw, float pitch);

/* Tourne de (dyaw, dpitch) radians ; le pitch est borné. */
void eng_camera_rotate(EngCamera *camera, float dyaw, float dpitch);

/* Fixe la vitesse, bornée à [ENG_CAMERA_SPEED_MIN, ENG_CAMERA_SPEED_MAX] (NaN : inchangée). */
void eng_camera_set_speed(EngCamera *camera, float speed);

/* Direction de regard et direction « droite » (unitaires ; right reste horizontale). */
void eng_camera_forward(const EngCamera *camera, float out[3]);
void eng_camera_right(const EngCamera *camera, float out[3]);

/* Matrices pour le renderer : vue (monde → caméra) et projection (aspect = largeur / hauteur). */
void eng_camera_view(const EngCamera *camera, mat4 out);
void eng_camera_projection(const EngCamera *camera, float aspect, mat4 out);

/* Un pas de simulation de `dt` secondes : regard, déplacement, vitesse. */
void eng_camera_update(EngCamera *camera, const EngInputState *input, float dt);

/* Interpolation entre deux états (t dans [0,1], borné) pour l'affichage entre deux pas fixes :
 * position, yaw, pitch ; les autres champs viennent de `to`. */
void eng_camera_lerp(const EngCamera *from, const EngCamera *to, float t, EngCamera *out);
