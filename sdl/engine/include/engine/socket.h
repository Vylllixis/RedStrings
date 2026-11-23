#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/debug_lines.h"
#include "engine/math.h"
#include "engine/skeleton.h"

/* Sockets (M4-22) : points d'attache nommés sur une articulation (arme dans la main, bouclier au
 * dos, lampe à la tête). Un socket = articulation + décalage local (translation, rotation, échelle
 * propre). Sa matrice dans le monde de jeu suit l'animation :
 *
 *   monde = modèle x CADRE(articulation) x décalage
 *
 * CADRE(articulation) = la position de l'articulation et son orientation, SANS son échelle. C'est
 * indispensable pour un import Mixamo : l'échelle 0,01 d'`Armature` est repliée dans la racine,
 * donc toutes les matrices monde du squelette portent un facteur 0,01 (translations en centimètres
 * converties en mètres) ; un objet attaché avec la matrice brute serait cent fois trop petit. Les
 * trois colonnes de la partie linéaire sont normalisées ; une échelle non uniforme ou un
 * cisaillement dans la hiérarchie n'est donc pas reporté sur l'objet attaché.
 *
 * Rotation du décalage : angles d'Euler en degrés appliqués dans l'ordre X puis Y puis Z autour des
 * axes FIXES de l'articulation (R = Rz x Ry x Rx), comme « XYZ » dans Blender. */

#define ENG_SOCKET_MAX 16U
#define ENG_SOCKET_NAME_MAX 32U /* avec le zéro final */

typedef struct EngSocket {
    char name[ENG_SOCKET_NAME_MAX];
    uint32_t joint; /* indice dans le squelette */
    float translation[3];
    float rotation[4]; /* quaternion x, y, z, w unitaire */
    float scale;       /* échelle propre, > 0 */
} EngSocket;

typedef struct EngSocketSet {
    uint32_t count;
    EngSocket sockets[ENG_SOCKET_MAX];
} EngSocketSet;

void eng_socketset_init(EngSocketSet *set);

/* Ajoute un socket. `joint_name` : nom d'une articulation de `skeleton` (comparaison exacte).
 * `translation` (mètres, repère de l'articulation) et `euler_degrees` : NULL = zéro. `scale` > 0
 * fini. Refusé (false, journalisé, ensemble inchangé) : argument NULL, nom vide / trop long /
 * caractère hors [A-Za-z0-9_], nom déjà pris, articulation inconnue, valeur non finie, échelle <=
 * 0, ensemble plein. */
bool eng_socketset_add(EngSocketSet *set, const EngSkeleton *skeleton, const char *name,
                       const char *joint_name, const float translation[3],
                       const float euler_degrees[3], float scale);

/* Indice du socket `name`, ou UINT32_MAX. */
uint32_t eng_socketset_find(const EngSocketSet *set, const char *name);

/* Matrice monde d'un socket. `joint_world` : `joint_count` matrices de eng_pose_world (espace du
 * personnage) ; `model` : placement du personnage dans le monde. Échec (false, `out` inchangée) :
 * argument NULL, articulation hors de [0 ; joint_count), valeur non finie, ou axe de l'articulation
 * de longueur nulle (matrice dégénérée). `out` peut être `model`. */
bool eng_socket_world(const EngSocket *socket, mat4 *joint_world, uint32_t joint_count, mat4 model,
                      mat4 out);

/* Fichier texte : une définition par ligne, « # » commence un commentaire.
 *   socket NOM ARTICULATION tx ty tz [rx ry rz [échelle]]
 * Remplace le contenu de `set` en cas de succès ; échec : `set` inchangé et `error` reçoit
 * « ligne N : raison » (tronqué à `error_size`). Au plus ENG_SOCKET_MAX sockets. */
bool eng_socketset_parse(const char *text, const EngSkeleton *skeleton, EngSocketSet *set,
                         char *error, size_t error_size);

/* Dessine le repère (X rouge, Y vert, Z bleu, longs de `size` x l'échelle du socket) de chaque
 * socket, SUPERPOSÉ (le mode de la liste est rétabli ensuite). Retourne le nombre de repères
 * dessinés ; un socket dont la matrice est dégénérée est sauté. 0 si un argument est NULL. */
uint32_t eng_socketset_debug_draw(const EngSocketSet *set, mat4 *joint_world, uint32_t joint_count,
                                  mat4 model, EngDebugLines *lines, float size);
