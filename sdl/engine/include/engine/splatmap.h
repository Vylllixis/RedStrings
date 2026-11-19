#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/heightmap.h"

/* Carte de mélange des matériaux du terrain (« splatmap », M3-03) : pour chaque échantillon du
 * champ de hauteurs, le poids de quatre couches de surface, calculés d'après l'altitude, la pente
 * et un bruit basse fréquence. Quatre canaux RGBA 8 bits : R herbe, G roche, B terre, A neige ; les
 * quatre valeurs d'un texel somment EXACTEMENT 255 (un mélange, pas une accumulation). Le fragment
 * shader `terrain.frag` lit cette carte (un texel par sommet du terrain) et mélange les quatre
 * textures de couche (voir docs/terrain.md).
 *
 * Règles (poids réels, avant quantification ; chaque terme dans [0, 1]) :
 *   roche = lissage(pente, rock_slope_start, rock_slope_full)
 *   neige = lissage(altitude relative, snow_start, snow_full) * (1 - roche)   (pas de neige sur la
 * roche) terre = lissage(bruit, 1 - dirt_amount - 0,1, 1 - dirt_amount + 0,1) * (1 - roche - neige)
 *   herbe = le reste
 * où lissage est smoothstep, la pente est l'angle de la surface avec l'horizontale (degrés),
 * l'altitude relative vaut (altitude - altitude_min) / (altitude_max - altitude_min) du champ. */

#define ENG_SPLAT_LAYERS 4U
typedef enum EngSplatLayer {
    ENG_SPLAT_GRASS = 0,
    ENG_SPLAT_ROCK = 1,
    ENG_SPLAT_DIRT = 2,
    ENG_SPLAT_SNOW = 3,
} EngSplatLayer;

typedef struct EngSplatParams {
    float rock_slope_start_deg; /* pente où la roche commence (degrés) */
    float rock_slope_full_deg;  /* pente où elle est complète ; > start, <= 90 */
    float snow_start;           /* altitude relative [0, 1] où la neige commence */
    float snow_full;            /* altitude relative où elle est complète ; >= start */
    float dirt_amount;          /* part approximative des surfaces plates en terre, [0, 1] */
    float dirt_wavelength;      /* mètres : taille des plaques de terre, > 0 */
    int32_t seed;               /* graine du bruit des plaques de terre */
} EngSplatParams;

/* roche de 30° à 50°, neige de 70 % à 85 % de la plage d'altitude, 30 % de terre en plaques de 24
 * m, graine 7. */
EngSplatParams eng_splat_default_params(void);

/* Vérifie les paramètres (journalisé) : valeurs finies, 0 <= start < full <= 90, 0 <= snow_start <=
 * snow_full <= 1, 0 <= dirt_amount <= 1, dirt_wavelength > 0. */
bool eng_splat_params_valid(const EngSplatParams *params);

/* Poids d'une surface (fonction pure, sert au calcul de la carte et aux tests) : `slope_deg` pente
 * en degrés, `altitude_relative` dans [0, 1] (bornée), `dirt_noise` dans [0, 1] (borné). `out`
 * reçoit quatre poids dans [0, 1] de somme 1 (à l'arrondi flottant près). Paramètres supposés
 * valides. */
void eng_splat_weights(const EngSplatParams *params, float slope_deg, float altitude_relative,
                       float dirt_noise, float out[ENG_SPLAT_LAYERS]);

/* Quantifie quatre poids [0, 1] en quatre octets de somme exactement 255 (le reste de l'arrondi va
 * au plus grand poids). */
void eng_splat_quantize(const float weights[ENG_SPLAT_LAYERS], uint8_t out[ENG_SPLAT_LAYERS]);

/* Pente (degrés, [0, 90[) de la surface au sommet (x, z) du champ : différences centrées en mètres
 * (unilatérales au bord), comme les normales du terrain (engine/terrain.h). */
float eng_heightmap_slope_degrees(const EngHeightmap *map, uint32_t x, uint32_t z);

typedef struct EngSplatmap {
    uint32_t size;   /* côté, égal à celui du champ */
    uint8_t *pixels; /* size * size * 4 octets RGBA, ligne par ligne comme le champ */
} EngSplatmap;

/* Calcule la carte du champ (`size` x `size` texels, un par échantillon) dans `arena` (4 * size²
 * octets, plus une ligne de travail restituée). false (journalisé) : paramètres invalides ou arène
 * pleine ; `out` est alors nul. */
bool eng_splatmap_generate(EngArena *arena, const EngHeightmap *map, const EngSplatParams *params,
                           EngSplatmap *out);
