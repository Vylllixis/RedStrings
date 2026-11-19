#ifndef ENG_SFX_H
#define ENG_SFX_H

#include <stdint.h>

/* Sons de combat PROCÉDURAUX (M5-19) : aucun fichier audio, tout est synthétisé de façon
 * déterministe (bruit à graine fixe, sinusoïdes amorties). Mono, flottants dans [-1 ; 1].
 * Sons provisoires : on les juge à l'oreille et on les remplacera par de vrais enregistrements. */

typedef enum EngSfx {
    ENG_SFX_HIT_LIGHT = 0,
    ENG_SFX_HIT_HEAVY,
    ENG_SFX_BLOCK,
    ENG_SFX_PARRY,
    ENG_SFX_WHOOSH,
    ENG_SFX_DODGE,
    ENG_SFX_COUNT
} EngSfx;

#define ENG_SFX_MAX_RATE 48000U
#define ENG_SFX_MAX_SAMPLES 14400U /* 0,30 s à 48 kHz : la plus longue (parade) */

const char *eng_sfx_name(EngSfx s);

/* Nombre d'échantillons du son à cette fréquence (8000 à 48000 Hz), 0 si invalide. */
uint32_t eng_sfx_length(EngSfx s, uint32_t sample_rate);

/* Synthétise dans `out` (capacité `cap`). Retourne le nombre d'échantillons écrits, 0 si `cap` est
 * insuffisante ou arguments invalides. Le crête vaut 0,9 ; le dernier échantillon est nul. */
uint32_t eng_sfx_synth(EngSfx s, uint32_t sample_rate, float *out, uint32_t cap);

#endif
