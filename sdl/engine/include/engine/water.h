#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/heightmap.h"
#include "engine/model.h"
#include "engine/renderer.h"

/* Eau simple (M3-10) : un plan d'eau à l'altitude `level`, dont la couleur dépend de la PROFONDEUR
 * (eau claire près du bord, sombre au large) et dont le reflet du ciel suit Fresnel. Le plan est
 * opaque (le renderer n'a pas de mélange alpha) : la rive est la ligne où le terrain perce le plan.
 *
 *   t       = 1 - exp(-absorption x profondeur)              (0 au bord, tend vers 1 au large)
 *   corps   = mélange(couleur peu profonde, couleur profonde, t)
 *   n       = normale du plan perturbée par deux ondes de rides (amplitude = pente maximale)
 *   F       = f0 + (1 - f0) (1 - cos)^5,   cos = <n, vers la caméra>   (Schlick)
 *   ciel    = mélange(horizon, zénith, sqrt(r_y)),  r = réflexion du regard sur n (r_y borné à [0,
 * 1]) éclairé = corps x (ambiante + soleil x <n, soleil> / pi) couleur = mélange(éclairé, ciel, F)
 * + soleil x F x <r, soleil>^256
 *
 * Le fragment shader water.frag recopie ces formules (test engine.water_gpu) puis applique le
 * brouillard de terrain.frag. Voir docs/water.md. */

typedef struct EngWaterParams {
    float level;            /* altitude du plan (m) */
    float shallow_color[3]; /* couleur de l'eau au bord (linéaire, >= 0) */
    float deep_color[3];    /* couleur de l'eau profonde (linéaire, >= 0) */
    float absorption;       /* 1/m, > 0 : plus il est grand, plus l'eau fonce vite */
    float f0;               /* réflectance à incidence normale, [0, 1] (eau : 0,02) */
    float ripple_amplitude; /* pente maximale des rides, [0, 0,5] ; 0 : plan lisse */
    float ripple_scale;     /* rad/m, > 0 : nombre d'onde de la première ride */
    float ripple_speed;     /* rad/s, >= 0 : vitesse de phase */
} EngWaterParams;

/* Eau claire verdâtre : peu profonde (0,10, 0,32, 0,30), profonde (0,01, 0,06, 0,10), absorption
 * 0,45 /m, f0 0,02, rides de pente 0,05, nombre d'onde 1,3 rad/m, vitesse 0,8 rad/s. */
EngWaterParams eng_water_default(float level);
bool eng_water_valid(const EngWaterParams *params);

/* Couleur du corps de l'eau à la profondeur `depth` (m, >= 0 ; négative : 0). */
void eng_water_body_color(const EngWaterParams *params, float depth, float out[3]);
/* Fresnel de Schlick ; cos borné à [0, 1]. */
float eng_water_fresnel(float f0, float cos_theta);
/* Normale (unitaire) du plan au point (x, z) à l'instant `time_seconds`. Deux ondes planes : la
 * première (direction (0,8, 0,6), nombre d'onde ripple_scale) de pente ripple_amplitude, la seconde
 * (direction (-0,6, 0,8), nombre d'onde 1,7 x ripple_scale, phase 1,3 x plus rapide) de pente
 * moitié moindre ; normale = (-pente x, 1, -pente z) normalisée. */
void eng_water_normal(const EngWaterParams *params, float x, float z, double time_seconds,
                      float out[3]);

/* Ciel réfléchi : le dégradé de eng_atmosphere_to_sky_desc. */
typedef struct EngWaterSky {
    float horizon[3];
    float zenith[3];
} EngWaterSky;

/* Éclairage : soleil (vers le soleil, unitaire ; rayonnement) et ambiante. */
typedef struct EngWaterLight {
    float sun_direction[3];
    float sun_radiance[3];
    float ambient[3];
} EngWaterLight;

/* Référence CPU du fragment shader (sans brouillard) : couleur linéaire de la surface au point
 * monde `world` (y = level), de profondeur `depth`, vue de `camera`. */
void eng_water_shade(const EngWaterParams *params, const EngWaterSky *sky,
                     const EngWaterLight *light, const float camera[3], const float world[3],
                     float depth, double time_seconds, float out[3]);

#define ENG_WATER_UNIFORM_FLOATS 20U
/* Cinq float4 de uniforms de fragment (à placer APRÈS le bloc de brouillard, 16 floats) :
 * [0..2] horizon, [3] f0 ; [4..6] zénith, [7] absorption ; [8..10] eau peu profonde, [11] amplitude
 * des rides ; [12..14] eau profonde, [15] phase de la première ride (vitesse x temps, dans
 * [0, 2 pi[) ; [16] nombre d'onde, [17] phase de la seconde ride (1,3 x vitesse x temps, dans
 * [0, 2 pi[), [18..19] 0. Paramètres invalides : false (journalisé). */
bool eng_water_pack(const EngWaterParams *params, const EngWaterSky *sky, double time_seconds,
                    float out[ENG_WATER_UNIFORM_FLOATS]);

/* Maillage du plan : les cellules du champ dont au moins un coin est sous `level` (le terrain cache
 * le reste). Les sommets sont ceux du champ (dans le rectangle englobant des cellules mouillées),
 * à l'altitude `level` ; uv.x = PROFONDEUR (level - altitude du terrain, bornée à 0), uv.y = 0 ;
 * normale +Y. Positions monde = origine + position du champ. Une partie (matériau par défaut).
 * Aucune cellule mouillée : true, modèle vide (vertex_count 0). Paramètres invalides ou arène
 * pleine : false (journalisé), modèle à zéro, arène restituée. */
bool eng_water_build(EngArena *arena, const EngHeightmap *map, const EngWaterParams *params,
                     float origin_x, float origin_z, EngModel *model);

/* Nombre de cellules mouillées (même critère que eng_water_build). */
uint32_t eng_water_wet_cells(const EngHeightmap *map, const EngWaterParams *params);

/* Pipeline : model.vert + water.frag. */
void eng_water_pipeline_desc(EngPipelineDesc *desc);
