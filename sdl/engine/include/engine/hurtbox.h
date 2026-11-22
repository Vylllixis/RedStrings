#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/combat_geom.h"
#include "engine/debug_lines.h"
#include "engine/math.h"
#include "engine/skeleton.h"

/* Hurtboxes (M5-05) : capsules attachées aux os d'un personnage, par zone (tête, torse, membres).
 * Une capsule est définie dans le repère de son articulation SANS échelle (comme les sockets, M4-22
 * : l'échelle 0,01 d'`Armature` d'un import Mixamo est repliée dans la racine et ne doit pas
 * rétrécir la capsule) ; son rayon est en mètres, multiplié par l'échelle uniforme du placement
 * `model`.
 *
 * Mise à jour à chaque tick : eng_hurtboxset_world(set, eng_pose_world(...), model) rend les
 * capsules dans le monde, qui suivent l'animation. Fichier texte :
 *
 *   # hurtboxes.txt
 *   hurtbox head  head  Head      0 0.08 0   0 0.20 0   0.11
 *   hurtbox torso torso Spine1    0 0 0      0 0.35 0   0.18
 *   hurtbox arm_l limb  LeftArm   0 0 0      0.28 0 0   0.07
 *
 *   hurtbox NOM ZONE ARTICULATION ax ay az bx by bz RAYON     (ZONE : head | torso | limb)
 *
 * Tout ou rien (« ligne N : message »). 16 hurtboxes au plus par combattant. */

#define ENG_HURTBOX_MAX 16U
#define ENG_HURTBOX_NAME_MAX 32U

typedef enum EngHitZone {
    ENG_ZONE_HEAD = 0,
    ENG_ZONE_TORSO,
    ENG_ZONE_LIMB,
    ENG_ZONE_COUNT
} EngHitZone;

typedef struct EngHurtbox {
    char name[ENG_HURTBOX_NAME_MAX];
    EngHitZone zone;
    uint32_t joint; /* indice dans le squelette */
    float a[3];     /* extrémités de l'axe, repère de l'articulation (m) */
    float b[3];
    float radius; /* m, > 0 */
} EngHurtbox;

typedef struct EngHurtboxSet {
    uint32_t count;
    EngHurtbox boxes[ENG_HURTBOX_MAX];
} EngHurtboxSet;

const char *eng_zone_name(EngHitZone z);

void eng_hurtboxset_init(EngHurtboxSet *set);

/* Ajoute une hurtbox. Refusé (false, ensemble inchangé) : argument NULL, nom invalide / déjà pris,
 * zone hors bornes, articulation inconnue, valeur non finie, rayon <= 0 ou > 10, ensemble plein. */
bool eng_hurtboxset_add(EngHurtboxSet *set, const EngSkeleton *skeleton, const char *name,
                        EngHitZone zone, const char *joint_name, const float a[3], const float b[3],
                        float radius);

/* Indice de la hurtbox `name` ou UINT32_MAX. */
uint32_t eng_hurtboxset_find(const EngHurtboxSet *set, const char *name);

/* Remplace `set` (échec : inchangé, message dans `error`). */
bool eng_hurtboxset_parse(const char *text, const EngSkeleton *skeleton, EngHurtboxSet *set,
                          char *error, size_t error_size);

/* Capsule dans le monde d'une extrémité-à-extrémité attachée à `joint`, avec les mêmes arguments
 * que eng_socket_world (`joint_world` : joint_count matrices de eng_pose_world ; `model` :
 * placement). Sert aussi aux hitboxes d'arme (attack_data). false si l'articulation est hors
 * bornes, une valeur est non finie ou une matrice est dégénérée (`out` inchangé). */
bool eng_capsule_on_joint(uint32_t joint, const float a[3], const float b[3], float radius,
                          mat4 *joint_world, uint32_t joint_count, mat4 model, EngCapsule *out);

/* Capsules monde de toutes les hurtboxes (out : `set->count` éléments). false si l'une d'elles
 * échoue ; `out` contient alors les capsules calculées jusque-là, les autres sont mises à rayon -1
 * (invalides : jamais touchées). */
bool eng_hurtboxset_world(const EngHurtboxSet *set, mat4 *joint_world, uint32_t joint_count,
                          mat4 model, EngCapsule *out);

/* Dessine une capsule (deux sphères + 4 lignes). false si refusée. */
bool eng_capsule_debug_draw(EngDebugLines *lines, const EngCapsule *c, EngDebugColor color);

/* Couleur de la zone : tête magenta, torse jaune, membres cyan. */
EngDebugColor eng_zone_color(EngHitZone z);
