#pragma once

#include <stdbool.h>

#include "engine/atmosphere.h"

/* Brouillard de distance et de hauteur (M3-06).
 *
 * Densité exponentielle en hauteur : rho(y) = density x exp(-height_falloff x (y - base_height)),
 * en 1/mètre. Le long du segment caméra -> point, l'épaisseur optique est l'intégrale exacte (pas
 * d'échantillonnage) :
 *
 *   tau = density exp(-falloff (cam.y - base_height)) x d x (1 - exp(-x)) / x,   x = falloff x (p.y
 * - cam.y) (limite d x density exp(...) quand x -> 0) fraction de brouillard f = max_opacity x (1 -
 * exp(-tau)) couleur finale = lerp(couleur éclairée, couleur du brouillard, f)
 *
 * COULEUR LIÉE AU CIEL : la couleur du brouillard est la radiance du ciel à l'horizon, dans
 * l'azimut du point, multipliée par l'échelle d'exposition du ciel (eng_atmosphere_horizon). Un
 * point lointain, entièrement dans le brouillard, a donc exactement la couleur du ciel là où le
 * terrain rejoint l'horizon : « les chunks lointains se fondent dans le ciel ». Le brouillard
 * suit aussi le soleil (halo, coucher, nuit) puisqu'il vient du même modèle.
 *
 * Le fragment shader terrain.frag recopie ces formules ; le test engine.terrain_gpu compare le
 * rendu à eng_fog_apply pixel par pixel. Voir docs/fog.md. */

typedef struct EngFog {
    float density;        /* 1/m au niveau base_height ; 0 : pas de brouillard. [0, 1] */
    float height_falloff; /* 1/m : de combien la densité décroît avec l'altitude ; 0 : uniforme.
                           * [0, 1] */
    float base_height;    /* altitude monde (m) où la densité vaut `density` ; finie */
    float max_opacity;    /* plafond de f, [0, 1] ; 0 : brouillard coupé */
} EngFog;

/* Défauts : densité 0,003 /m, décroissance 0,004 /m (hauteur d'échelle 250 m), niveau 0 m,
 * opacité maximale 1. À 1 km en terrain plat à hauteur du sol, f = 0,95. */
void eng_fog_default(EngFog *fog);
bool eng_fog_valid(const EngFog *fog);

/* Épaisseur optique tau de la caméra au point (>= 0). 0 si le brouillard est invalide ou si les
 * positions ne sont pas finies. */
float eng_fog_optical_depth(const EngFog *fog, const float camera[3], const float point[3]);

/* Fraction de brouillard f dans [0, max_opacity]. */
float eng_fog_factor(const EngFog *fog, const float camera[3], const float point[3]);

/* Couleur éclairée `lit` -> couleur brouillardée `out`. `sun` : direction vers le soleil ;
 * `sky_scale` : échelle d'exposition du ciel affiché (>= 0). Brouillard ou atmosphère invalide :
 * `out` = `lit` et false. */
bool eng_fog_apply(const EngFog *fog, const EngAtmosphere *atmosphere, const float sun[3],
                   float sky_scale, const float camera[3], const float point[3], const float lit[3],
                   float out[3]);

/* Les 16 flottants que terrain.frag lit après le cbuffer d'éclairage (quatre float4) :
 *   [0..3]   density, height_falloff, base_height, max_opacity (0 : pas de brouillard)
 *   [4..7]   rayleigh xyz, mie
 *   [8..11]  night xyz, irradiance
 *   [12..15] mie_g, path_limit, sun_path_limit, sky_scale
 * `fog` NULL ou invalide : tout à 0 (brouillard coupé). */
#define ENG_FOG_UNIFORM_FLOATS 16U
void eng_fog_pack(const EngFog *fog, const EngAtmosphere *atmosphere, float sky_scale,
                  float out[ENG_FOG_UNIFORM_FLOATS]);
