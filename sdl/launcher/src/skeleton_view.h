/* Affichage de debug d'un squelette .skel (M4-12) : `launcher --skeleton fichier.skel`. Les os
 * sont dessinés SUPERPOSÉS (visibles à travers la matière) avec la matrice du modèle central de la
 * scène : le squelette tourne avec le maillage et doit lui coller (critère « aligné au mesh »). */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/anim_clip.h"
#include "engine/anim_debug.h"
#include "engine/arena.h"
#include "engine/debug_lines.h"
#include "engine/skeleton.h"
#include "engine/socket.h"

typedef struct SkeletonView {
    EngArena arena;
    EngSkeleton skeleton;
    mat4 rest[ENG_SKELETON_MAX_JOINTS]; /* pose repos monde, calculée une fois */
    float mismatch;                     /* écart pose de liaison / pose repos, mètres */
    bool ready;
    EngAnimClip clip; /* M4-16 : clip lu (--anim) et son lecteur */
    EngAnimDebug anim;
    bool animated;
    EngSocketSet sockets; /* M4-22 : points d'attache affichés (--sockets) */
    bool prev_keys[6];    /* détection de front des raccourcis */
    char status[160];     /* ligne d'état du lecteur, mise à jour à chaque image */
} SkeletonView;

/* Lit `path` ; false (journalisé) si le fichier est absent ou invalide. */
bool skeleton_view_load(SkeletonView *view, const char *path);
/* M4-16 : charge le clip `anim_path` pour l'animer. `skel_path` NULL : le squelette est celui que
 * le clip désigne (skeleton_path). false (journalisé) si un fichier manque ou ne va pas ensemble.
 */
bool skeleton_view_load_anim(SkeletonView *view, const char *skel_path, const char *anim_path);

/* M4-22 : lit un fichier de sockets (docs/sockets.md) et les affiche sur le squelette. false
 * (journalisé avec la ligne fautive) si le fichier est absent ou invalide ; les sockets déjà
 * chargés sont alors conservés. Le squelette doit être chargé. */
bool skeleton_view_load_sockets(SkeletonView *view, const char *path);

/* Une fois par image : raccourcis clavier (P pause, `,` `.` image précédente / suivante, J K
 * articulation précédente / suivante, T trajectoire) puis avance du lecteur de `dt` secondes. */
void skeleton_view_update(SkeletonView *view, float dt, bool keyboard_free);

void skeleton_view_shutdown(SkeletonView *view);

/* Ajoute les os à `lines`, transformés par `model` ; retourne le nombre d'os dessinés. */
uint32_t skeleton_view_draw(SkeletonView *view, EngDebugLines *lines, mat4 model);
