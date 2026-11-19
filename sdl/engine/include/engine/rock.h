#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/model.h"

/* Rocher procédural (M3-07) : une icosphère subdivisée dont chaque sommet est déplacé radialement
 * par un bruit de valeur 3D, puis aplatie. Un maillage FERMÉ (variété à 2 faces par arête, sommets
 * partagés), étoilé par rapport à l'origine, aux normales lissées : une base pour l'instancing
 * (engine/instancing.h) ; la variété vient des instances (échelle, cap, teinte), pas de milliers de
 * maillages.
 *
 * Avec `subdivisions` = n : 10 x 4^n + 2 sommets et 20 x 4^n triangles (n = 1 : 42 / 80, n = 2 :
 * 162 / 320, n = 3 : 642 / 1280). Même graine, mêmes paramètres : mêmes octets. */

typedef struct EngRockParams {
    uint32_t seed;
    uint32_t subdivisions; /* 0 à ENG_ROCK_MAX_SUBDIVISIONS */
    float radius;          /* mètres, > 0 : rayon moyen avant aplatissement */
    float roughness;       /* [0, 1[ : amplitude relative du déplacement (0 : sphère) */
    float flatness;        /* ]0, 1] : échelle verticale (1 : pas d'aplatissement) */
    /* Matériau ; NULL : gris moyen rugueux, non métallique (voir eng_rock_default_material). */
    const EngModelMaterial *material;
} EngRockParams;

#define ENG_ROCK_MAX_SUBDIVISIONS 5U

/* subdivisions 2, rayon 1 m, rugosité 0,25, aplatissement 0,7, graine 1. */
EngRockParams eng_rock_default_params(void);
EngModelMaterial eng_rock_default_material(void);
bool eng_rock_params_valid(const EngRockParams *params);

uint32_t eng_rock_vertex_count(uint32_t subdivisions);
uint32_t eng_rock_triangle_count(uint32_t subdivisions);

/* Construit le modèle (une partie) dans `arena`. Paramètres invalides ou arène trop petite : false
 * (journalisé), `model` est mis à zéro et l'arène restituée. */
bool eng_rock_build(EngArena *arena, const EngRockParams *params, EngModel *model);
