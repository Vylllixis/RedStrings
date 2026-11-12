#pragma once

/* Mathématiques du moteur : cglm avec les conventions du projet (docs/conventions.md, « Maths »).
 *
 *   - repère main droite, Y vers le haut, une unité = 1 mètre
 *   - la caméra regarde vers -Z, +X est à sa droite
 *   - matrices en colonne-majeur : m[colonne][ligne], translation dans m[3]
 *   - espace de découpage SDL_GPU : x et y dans [-1, 1] (y vers le haut), profondeur z dans [0, 1]
 *
 * Toujours inclure ce fichier plutôt que <cglm/cglm.h> : il vérifie à la compilation que la
 * configuration de cglm est la bonne. La définition CGLM_FORCE_DEPTH_ZERO_TO_ONE vient de CMake
 * (cible engine) pour que tous les fichiers voient la même configuration. */

#ifndef CGLM_FORCE_DEPTH_ZERO_TO_ONE
#error "cglm doit être compilé avec CGLM_FORCE_DEPTH_ZERO_TO_ONE (profondeur [0,1] de SDL_GPU)"
#endif
#ifdef CGLM_FORCE_LEFT_HANDED
#error "le projet utilise un repère main droite : ne pas définir CGLM_FORCE_LEFT_HANDED"
#endif

#include <stdbool.h>

#include <cglm/cglm.h>

/* Axes du monde. */
#define ENG_AXIS_RIGHT ((vec3){1.0F, 0.0F, 0.0F})
#define ENG_AXIS_UP ((vec3){0.0F, 1.0F, 0.0F})
#define ENG_AXIS_FORWARD ((vec3){0.0F, 0.0F, -1.0F})

/* Projection perspective : fovy en radians, aspect = largeur / hauteur, 0 < near < far. */
static inline void eng_perspective(float fovy, float aspect, float near_z, float far_z, mat4 dest)
{
    glm_perspective(fovy, aspect, near_z, far_z, dest);
}

/* Matrice de vue : caméra en `eye`, orientée vers `target`, `up` = verticale approximative. */
static inline void eng_look_at(vec3 eye, vec3 target, vec3 up, mat4 dest)
{
    glm_lookat(eye, target, up, dest);
}

/* Projette un point (espace de la matrice `m`) : renvoie false si le point est derrière la caméra
 * (w <= 0), sinon écrit ses coordonnées normalisées (x, y dans [-1,1], z dans [0,1] si visible). */
static inline bool eng_project_point(mat4 m, const float point[3], vec3 ndc)
{
    vec4 p = {point[0], point[1], point[2], 1.0F};
    vec4 clip;
    glm_mat4_mulv(m, p, clip);
    if (clip[3] <= 0.0F) {
        return false;
    }
    ndc[0] = clip[0] / clip[3];
    ndc[1] = clip[1] / clip[3];
    ndc[2] = clip[2] / clip[3];
    return true;
}
