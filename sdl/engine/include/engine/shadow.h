#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/math.h"

/* Ombre du soleil (M1-17), côté CPU : la matrice orthographique qui voit la scène depuis le soleil,
 * le modèle de biais et le filtrage PCF. Le shader (model.frag) refait exactement les mêmes calculs
 * ; ce module est sa référence, testée sans GPU (tests/test_shadow.c) et comparée au shader sur GPU
 * (tests/test_shadow_gpu.c). Voir docs/shadows.md.
 *
 * Repère : docs/conventions-maths.md (main droite, Y vers le haut, mètres, profondeur [0, 1]).
 * `to_sun` est la direction du point éclairé VERS le soleil (comme EngLighting.sun_direction).
 *
 * La « carte d'ombre » est une texture carrée de profondeur : ce que le soleil voit de plus proche,
 * profondeur dans [0, 1] sur toute l'épaisseur de la sphère englobante (0 : côté soleil). La rangée
 * 0 de la texture est le HAUT de l'image rendue (v = 0.5 - 0.5 y_ndc). */

#define ENG_SHADOW_MAX_PCF_RADIUS 3U
#define ENG_SHADOW_MAX_SLOPE 8.0F /* tan de l'angle d'incidence, au-delà : plafonné (82,9°) */

/* Réglages du filtrage et des biais. Les biais sont exprimés en TEXELS de la carte d'ombre : ils
 * restent valables quand on change la résolution ou la taille de la zone couverte. */
typedef struct EngShadowParams {
    /* Décalage fixe de la profondeur comparée, en texels de profondeur (1 texel = 1/size). */
    float constant_bias;
    /* Multiplie le décalage selon l'inclinaison : (rayon_PCF + 1) × tan(angle de la normale au
     * soleil) texels de profondeur. 1 : juste assez pour qu'aucun point du noyau de filtrage ne
     * s'ombrage lui-même sur un plan. 0 : désactivé. */
    float slope_bias;
    /* Décale le point lu le long de la normale géométrique, de normal_bias × sin(angle) texels (en
     * mètres : × texel_world) : supprime l'acné sans « décoller » l'ombre de son objet. */
    float normal_bias;
    /* Rayon du noyau PCF en texels : (2r + 1)^2 prises, chacune filtrée bilinéairement. 0 : une
     * seule prise bilinéaire. Plafonné à ENG_SHADOW_MAX_PCF_RADIUS. */
    uint32_t pcf_radius;
    /* Part de la lumière directe retirée dans l'ombre : 1 = ombre complète, 0 = pas d'ombre. */
    float strength;
} EngShadowParams;

/* constant 1,0 ; pente 1,0 ; normale 1,0 ; rayon 1 ; force 1. */
void eng_shadow_params_default(EngShadowParams *params);

/* Ramène les réglages dans le domaine valide (biais >= 0 et finis, rayon <= le maximum, force dans
 * [0, 1]). Retourne true si rien n'a dû être corrigé. */
bool eng_shadow_params_sanitize(EngShadowParams *params);

typedef struct EngShadowMap {
    mat4 view_proj;     /* monde -> découpage de la passe d'ombre (orthographique) */
    mat4 shadow_matrix; /* monde -> (u, v, profondeur), dans [0, 1] ; pas de division (w = 1) */
    float to_sun[3];    /* unitaire */
    float texel_world;  /* côté d'un texel, en mètres */
    float depth_range;  /* mètres couverts par la profondeur [0, 1] */
    uint32_t size;      /* côté de la carte, en texels */
} EngShadowMap;

/* Sphère englobante d'une boîte alignée sur les axes. */
void eng_shadow_sphere_from_box(const float box_min[3], const float box_max[3], float center[3],
                                float *radius);

/* Oriente la caméra du soleil pour voir la sphère (`center`, `radius`), avec une carte carrée de
 * `size` texels. Une marge de deux texels entoure la sphère (au moins 1,5 après alignement), et le
 * centre est aligné sur la grille des texels : une scène qui bouge de moins d'un texel ne fait pas
 * « nager » les bords des ombres. false (rien d'écrit) si la direction est nulle ou non numérique,
 * si le rayon n'est pas > 0 ou si `size` < 8. */
bool eng_shadow_fit(EngShadowMap *map, const float to_sun[3], const float center[3], float radius,
                    uint32_t size);

/* Position monde -> (u, v, profondeur). */
void eng_shadow_project(const EngShadowMap *map, const float world[3], float uvd[3]);

/* Point de comparaison d'une surface : décale `position` le long de `normal` (unitaire, normale
 * GÉOMÉTRIQUE du côté éclairé) selon l'inclinaison, la projette, puis retranche les biais de
 * profondeur. Retourne (u, v, profondeur de référence). */
void eng_shadow_receiver(const EngShadowMap *map, const EngShadowParams *params,
                         const float position[3], const float normal[3], float uvd[3]);

/* Part de lumière reçue, dans [0, 1] : moyenne du noyau PCF de rayon `radius` autour de (u, v), où
 * chaque prise est une interpolation bilinéaire des quatre comparaisons « profondeur de référence
 * <= profondeur stockée » (lumière). `depth` : size x size flottants, rangée 0 en premier. Hors de
 * la carte (u ou v hors de [0, 1], profondeur hors de [0, 1]) : 1, la lumière n'est pas arrêtée.
 * Les prises qui sortent de la carte lisent le texel du bord. */
float eng_shadow_visibility(const float *depth, uint32_t size, const float uvd[3], uint32_t radius);

/* 1 - force × (1 - visibilité) : le facteur qui multiplie la lumière DIRECTE du soleil. */
float eng_shadow_factor(const EngShadowParams *params, float visibility);
