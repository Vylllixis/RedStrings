#pragma once

#include <stdint.h>

/* Bruit de gradient 2D (OpenSimplex2) fractal, derrière lequel FastNoiseLite (code tiers, M3-01)
 * reste caché : seul engine/src/third_party_fnl.c en voit les types.
 *
 * Résultat : environ [-1, 1] (les fractales débordent légèrement ; l'appelant normalise). Mêmes
 * paramètres, même résultat sur une même plateforme et un même binaire ; d'une plateforme ou d'un
 * compilateur à l'autre, des arrondis flottants peuvent différer (voir docs/terrain.md). */

typedef enum EngNoiseFractal {
    ENG_NOISE_FBM = 0,    /* somme d'octaves : relief arrondi */
    ENG_NOISE_RIDGED = 1, /* crêtes : relief de montagnes */
} EngNoiseFractal;

typedef struct EngNoiseParams {
    int32_t seed;
    float frequency; /* cycles par unité de coordonnée de la première octave */
    uint32_t octaves;
    float lacunarity; /* multiplicateur de fréquence d'une octave à la suivante */
    float gain;       /* multiplicateur d'amplitude d'une octave à la suivante */
    EngNoiseFractal fractal;
} EngNoiseParams;

/* Écrit dans out[i] le bruit en (x0 + i * step, y) pour i dans [0, count). */
void eng_noise_row_2d(const EngNoiseParams *params, float x0, float step, float y, uint32_t count,
                      float *out);
