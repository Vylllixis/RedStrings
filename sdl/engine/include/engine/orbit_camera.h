#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/camera.h"
#include "engine/input_state.h"
#include "engine/physics.h"

/* Caméra à la troisième personne : orbite autour d'un point (le pivot : le haut du dos du
 * personnage), à `distance` mètres. Module pur (ni SDL ni GPU), il ne lit que EngInputState.
 *
 * Conventions (docs/conventions.md) : yaw = 0, pitch = 0 : la caméra regarde vers -Z. Yaw
 * positif : tourne vers la gauche. Pitch POSITIF : la caméra regarde vers le haut, donc elle est
 * SOUS le pivot ; pitch négatif : elle regarde vers le bas depuis le dessus. Souris vers le haut :
 * pitch positif (comme à la première personne) ; `invert_y` l'inverse.
 *
 * Fluidité : la caméra avance par pas FIXES, comme la simulation (eng_orbit_camera_update à chaque
 * pas, jamais à chaque image). Pour l'affichage on garde l'état du pas précédent et celui du pas
 * courant et on les mélange avec eng_camera_lerp (alpha de la boucle) : le mouvement est le même à
 * 30, 60 ou 144 images par seconde, sans à-coup. Les deux lissages ci-dessous sont des filtres
 * exponentiels exacts en temps (1 - exp(-dt / tau)), pas des « lerp par image ».
 *
 *     EngOrbitCamera orbit;  eng_orbit_camera_init(&orbit, pivot, 0.0F, -0.3F, 4.0F);
 *     par pas :   eng_orbit_camera_update(&orbit, &input, dt, pivot);
 *     au rendu :  eng_orbit_camera_apply(&orbit, &state);   // EngCamera du pas courant */

#define ENG_ORBIT_DISTANCE_DEFAULT 4.0F /* m */
#define ENG_ORBIT_DISTANCE_MIN 1.0F
#define ENG_ORBIT_DISTANCE_MAX 12.0F
#define ENG_ORBIT_ZOOM_STEP 1.15F          /* facteur par cran de molette */
#define ENG_ORBIT_PITCH_MIN (-1.309F)      /* -75 degrés : vue de dessus */
#define ENG_ORBIT_PITCH_MAX 0.6981F        /* +40 degrés : vue d'en dessous */
#define ENG_ORBIT_FOLLOW_TAU_DEFAULT 0.10F /* s ; 0 : le pivot est suivi sans lissage */
#define ENG_ORBIT_LOOK_TAU_DEFAULT 0.03F   /* s ; 0 : le regard suit la souris sans lissage */
#define ENG_ORBIT_SENSITIVITY_MAX 10.0F
#define ENG_ORBIT_COLLISION_RADIUS 0.25F /* m, rayon de la sphère de la caméra (> plan proche) */
#define ENG_ORBIT_COLLISION_MIN 0.10F    /* m, la caméra ne se colle jamais au pivot */
#define ENG_ORBIT_RECOVER_TAU 0.30F      /* s, retour à la distance voulue après un obstacle */

typedef struct EngOrbitCamera {
    float target[3]; /* pivot suivi (après lissage) */
    float yaw;       /* radians, orientation affichée (lissée) */
    float pitch;     /* radians */
    float want_yaw;  /* orientation demandée par le joueur */
    float want_pitch;
    float distance;  /* m, demandée (molette) */
    float effective; /* m, distance utilisée : `distance`, réduite par la collision (M4-11) */
    float clear; /* m, distance libre lissée (collision) ; recule d'un coup, revient lentement */
    float follow_tau;  /* s */
    float look_tau;    /* s */
    float sensitivity; /* multiplicateur de ENG_CAMERA_LOOK_UNIT, ]0 ; ENG_ORBIT_SENSITIVITY_MAX] */
    bool invert_y;
} EngOrbitCamera;

/* Place la caméra derrière le pivot : yaw, pitch (borné), distance (bornée). false (caméra
 * inchangée) si un argument n'est pas fini. */
bool eng_orbit_camera_init(EngOrbitCamera *camera, const float pivot[3], float yaw, float pitch,
                           float distance);

/* Réglages (refusés, valeur inchangée, si hors limites ou non finis) : sensibilité en
 * multiplicateur de la sensibilité de base (config.ini : camera_sensitivity), sens vertical,
 * constantes de temps des lissages (>= 0, <= 2 s). */
bool eng_orbit_camera_set_sensitivity(EngOrbitCamera *camera, float multiplier);
bool eng_orbit_camera_set_smoothing(EngOrbitCamera *camera, float follow_tau, float look_tau);

/* Un pas de `dt` secondes (0 < dt <= 0,1) : regard (axe LOOK), zoom (molette, vers le haut =
 * plus près), suivi lissé de `pivot`. `effective` est remis à `distance`. false, rien de modifié,
 * si un argument est invalide. */
bool eng_orbit_camera_update(EngOrbitCamera *camera, const EngInputState *input, float dt,
                             const float pivot[3]);

/* Collision (M4-11), à appeler APRÈS eng_orbit_camera_update, dans le même pas : lance une sphère
 * de `radius` du pivot vers l'oeil (à `distance`) contre les couches `layer_mask` du monde
 * physique. Obstacle : `effective` est ramené immédiatement avant lui (la caméra ne traverse
 * jamais un mur) ; plus d'obstacle : elle revient vers `distance` avec la constante
 * ENG_ORBIT_RECOVER_TAU (pas de saut). Passer des couches STATIQUES (1 << ENG_LAYER_STATIC), pas
 * celle du personnage. Sans Jolt (OW_PHYSICS=OFF) : aucun obstacle. `radius` dans ]0 ; 1], dt
 * dans ]0 ; 0,1]. false, rien de modifié, si un argument est invalide. */
bool eng_orbit_camera_collide(EngOrbitCamera *camera, const EngPhysics *world, uint32_t layer_mask,
                              float radius, float dt);

/* Téléportation : le pivot est repris tel quel, sans lissage (changement de zone, réapparition). */
bool eng_orbit_camera_snap(EngOrbitCamera *camera, const float pivot[3]);

/* Position de l'oeil : pivot - direction de regard x effective. */
void eng_orbit_camera_eye(const EngOrbitCamera *camera, float out[3]);

/* Écrit position, yaw et pitch dans `out` ; champ de vision et plans restent ceux de `out`. */
void eng_orbit_camera_apply(const EngOrbitCamera *camera, EngCamera *out);

/* Directions HORIZONTALES unitaires de la vue (avant et droite), pour déplacer le personnage par
 * rapport à la caméra (M4-07). Elles suivent l'orientation affichée. */
void eng_orbit_camera_flat_forward(const EngOrbitCamera *camera, float out[3]);
void eng_orbit_camera_flat_right(const EngOrbitCamera *camera, float out[3]);
