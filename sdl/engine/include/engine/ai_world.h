#ifndef ENG_AI_WORLD_H
#define ENG_AI_WORLD_H

/* Obstacles statiques 2D (plan x, z) pour la vision, la navigation et la collision des ennemis
 * (M6-02/03). Cercles (rochers, arbres) et boîtes alignées sur les axes (murs). */

#include <stdbool.h>
#include <stdint.h>

#define ENG_AI_MAX_OBSTACLES 64U

typedef struct EngAiObstacle {
    bool box;
    float c[2];    /* centre */
    float r;       /* rayon (cercle) */
    float half[2]; /* demi-côtés (boîte) */
} EngAiObstacle;

typedef struct EngAiWorld {
    EngAiObstacle obs[ENG_AI_MAX_OBSTACLES];
    uint32_t count;
} EngAiWorld;

void eng_aiw_init(EngAiWorld *w);
bool eng_aiw_add_circle(EngAiWorld *w, float x, float z, float r);
bool eng_aiw_add_box(EngAiWorld *w, float x, float z, float half_x, float half_z);

/* Distance signée au plus proche obstacle (< 0 à l'intérieur) ; INFINITY s'il n'y en a pas. */
float eng_aiw_distance(const EngAiWorld *w, const float p[2]);

/* Vrai si le segment a-b ne traverse aucun obstacle (ligne de vue dégagée). */
bool eng_aiw_los(const EngAiWorld *w, const float a[2], const float b[2]);

/* Repousse `p` hors des obstacles pour qu'un disque de rayon `radius` ne les chevauche pas. */
void eng_aiw_push_out(const EngAiWorld *w, float p[2], float radius);

#endif
