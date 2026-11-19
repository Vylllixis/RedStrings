#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/debug_lines.h"
#include "engine/math.h"

/* Squelette (M4-12) : hiérarchie d'articulations, pose de repos locale et matrices inverses de la
 * pose de liaison (« bind pose »), lu depuis un .skel cuit (docs/asset-formats.md §5) ou importé
 * d'un skin glTF (Mixamo, Blender...).
 *
 * Deux poses à ne pas confondre :
 *   - REPOS : la translation / rotation / échelle LOCALES de chaque articulation ; composées le
 *     long de la hiérarchie, elles donnent la pose de départ d'une animation.
 *   - LIAISON : la pose dans laquelle le maillage a été attaché ; son inverse par articulation
 *     est `inverse_bind`. Un sommet du maillage est exprimé dans cette pose : pour l'afficher
 *     alignée au maillage il faut dessiner la pose de liaison (eng_skeleton_bind_world).
 *   Elles coïncident presque toujours (export depuis la pose de repos) ; quand elles divergent
 *   (maillage lié en T, animation en A), eng_skeleton_bind_mismatch le mesure.
 *
 * Ordre : un parent précède TOUJOURS ses enfants (une passe suffit pour composer les poses). Un
 * skin glTF peut lister ses articulations dans un autre ordre : l'import les range alors parents
 * d'abord et rend la permutation (`remap`), dont le cuiseur de maillages (M4-15) se servira pour
 * recoder les indices JOINTS_0 des sommets.
 *
 * Conventions (docs/conventions.md) : main droite, Y haut, mètres ; matrices en colonnes d'abord
 * (m[colonne][ligne], translation dans la colonne 3) ; quaternions (x, y, z, w) unitaires. */

#define ENG_SKELETON_MAX_JOINTS 256U /* indices d'articulation sur 8 bits côté GPU (M4-15) */
#define ENG_SKELETON_MAX_NAME 63U    /* octets, sans le zéro final */

typedef struct EngJoint {
    int32_t parent;         /* -1 : racine ; sinon un indice STRICTEMENT inférieur à celui-ci */
    float translation[3];   /* repos, local, mètres */
    float rotation[4];      /* repos, local, quaternion x, y, z, w (unitaire à 1e-3 près) */
    float scale[3];         /* repos, local */
    float inverse_bind[16]; /* inverse de la pose de liaison, colonnes d'abord */
    const char *name;       /* UTF-8, terminé par zéro, ou NULL */
} EngJoint;

typedef struct EngSkeleton {
    uint32_t joint_count; /* 1 à ENG_SKELETON_MAX_JOINTS */
    EngJoint *joints;
} EngSkeleton;

/* Construit un .skel depuis `skeleton`. Fichier alloué dans `arena` (valide jusqu'à son reset).
 * Squelette invalide (mêmes règles que le décodeur), mémoire insuffisante : false (journalisé),
 * arène restituée. Sortie déterministe : mêmes entrées, mêmes octets. */
bool eng_skeleton_encode(EngArena *arena, const EngSkeleton *skeleton, const void **out,
                         size_t *out_size);

/* Vérifie puis décode un .skel en mémoire ; les tableaux et les noms sont alloués dans `arena` (le
 * tampon `data` peut être libéré ensuite). `name` : pour le journal. Refusé (false, journalisé) :
 * `out` mis à zéro, `arena` restituée. Vérifié : conteneur, nombre d'articulations, tailles des
 * sections, parents valides (précédents), valeurs finies, quaternions unitaires, échelles non
 * nulles, noms terminés par zéro et de longueur bornée. */
bool eng_skeleton_decode(EngArena *arena, const void *data, size_t size, const char *name,
                         EngSkeleton *out);

/* Lit (chemin relatif à l'exécutable, engine/files.h) et décode. `scratch` reçoit le fichier brut
 * et est restituée. */
bool eng_skeleton_load(EngArena *arena, EngArena *scratch, const char *path, EngSkeleton *out);

/* Importe le skin `skin_index` d'un glTF / GLB en mémoire (`base_path` : pour les tampons externes,
 * NULL si tout est inclus). Chaque articulation devient une ligne ; son parent est l'articulation
 * ancêtre la plus proche (les nœuds intermédiaires qui ne sont pas des articulations sont repliés
 * dans la pose locale) ; la racine reçoit la transformation de ses ancêtres, de sorte que la pose
 * de repos composée soit la transformation GLOBALE du nœud dans le glTF. Matrices inverses : celles
 * du skin, ou l'identité si le skin n'en a pas (règle glTF). `remap` (peut être NULL), de
 * `joint_count` éléments alloués dans `arena` : indice dans le skin -> indice dans le squelette. Le
 * skin contient des articulations en double, plus de ENG_SKELETON_MAX_JOINTS, une échelle négative
 * ou nulle, une matrice non décomposable : false (journalisé), `arena` restituée. */
bool eng_skeleton_from_gltf(EngArena *arena, EngArena *scratch, const void *data, size_t size,
                            const char *base_path, uint32_t skin_index, EngSkeleton *out,
                            uint32_t **remap);

/* Décompose une matrice affine en translation, rotation (quaternion x, y, z, w unitaire) et
 * échelle positive (m = T·R·S). false si l'échelle est nulle ou négative, si la matrice est un
 * miroir ou si un résultat n'est pas fini. Utilisé par l'import des squelettes et des clips. */
bool eng_skeleton_decompose_trs(mat4 m, float translation[3], float rotation[4], float scale[3]);

/* Nombre de skins d'un glTF en mémoire ; 0 si illisible (journalisé). */
uint32_t eng_skeleton_gltf_skin_count(const void *data, size_t size);

/* Poses MONDE (`joint_count` matrices dans `out`). Repos : composition des poses locales (pose de
 * départ des animations). Liaison : inverse de `inverse_bind` (la pose où le maillage est
 * exprimé). false si les arguments sont NULL ou le squelette invalide. */
bool eng_skeleton_rest_world(const EngSkeleton *skeleton, mat4 *out);
bool eng_skeleton_bind_world(const EngSkeleton *skeleton, mat4 *out);

/* Plus grand écart (valeur absolue d'un élément affine, 12 premiers coefficients) entre la pose de
 * repos et la pose de liaison, toutes articulations ; 0 : identiques. Négatif si invalide. */
float eng_skeleton_bind_mismatch(const EngSkeleton *skeleton);

/* Dessine le squelette (debug draw) dans `lines` : un os par articulation non racine, du parent
 * vers l'articulation, plus une croix de 4 cm à chaque articulation. `world` : poses du squelette
 * (eng_skeleton_rest_world ou eng_skeleton_bind_world) ; `root` : placement de l'ensemble dans le
 * monde (identité : tel quel). Renvoie le nombre d'os dessinés ; 0 si un argument est NULL. */
uint32_t eng_skeleton_debug_draw(const EngSkeleton *skeleton, mat4 *world, mat4 root,
                                 EngDebugLines *lines, EngDebugColor color);
