#pragma once

#include <stdbool.h>

/* Tonemapping et gamma (M1-14). La scène est dessinée en HDR (RGBA16F, lumière LINÉAIRE, valeurs
 * au-delà de 1,0 permises), puis une passe plein écran la ramène à l'écran :
 *
 *     rgb  = hdr × 2^exposure_ev           (exposition, en EV : +1 EV double la lumière)
 *     rgb  = opérateur(rgb)                (courbe qui comprime [0, +inf[ vers [0, 1])
 *     sortie = encodage sRGB(rgb)          (sauf si la cible est déjà une texture sRGB)
 *
 * Opérateurs :
 *  - ENG_TONEMAP_NONE : saturate (aucune compression : tout ce qui dépasse 1,0 est écrasé).
 *  - ENG_TONEMAP_ACES : approximation de Narkowicz de la courbe ACES filmic
 *        (x(2,51x + 0,03)) / (x(2,43x + 0,59) + 0,14), par canal.
 *  - ENG_TONEMAP_AGX  : AgX (Troy Sobotka) dans l'implémentation de three.js / Filament : matrices
 *        sRGB <-> Rec.2020 et « inset/outset » AgX, encodage log2 sur [-12,47393 ; 4,026069] EV,
 *        sigmoïde polynomiale d'ordre 6 (agxDefaultContrastApprox), puis linéarisation en 2,2.
 *        Sans le « look » optionnel d'AgX.
 *
 * eng_tonemap_apply est la RÉFÉRENCE de ce calcul : shaders/tonemap.frag.hlsl fait la même chose
 * et les tests comparent les deux (docs/tonemapping.md). */

typedef enum EngTonemapOperator {
    ENG_TONEMAP_NONE = 0,
    ENG_TONEMAP_ACES,
    ENG_TONEMAP_AGX,
    ENG_TONEMAP_COUNT, /* nombre d'opérateurs, pas un opérateur */
} EngTonemapOperator;

#define ENG_EXPOSURE_EV_MIN (-10.0F)
#define ENG_EXPOSURE_EV_MAX 10.0F

typedef struct EngTonemap {
    EngTonemapOperator op;
    float exposure_ev; /* en EV (stops), [ENG_EXPOSURE_EV_MIN, ENG_EXPOSURE_EV_MAX] */
} EngTonemap;

/* ACES, 0 EV. */
void eng_tonemap_default(EngTonemap *t);

/* Remet `t` dans le domaine valide : opérateur inconnu -> ACES ; exposition non finie -> 0 ;
 * exposition hors bornes -> bornée. true si rien n'a dû être corrigé. */
bool eng_tonemap_sanitize(EngTonemap *t);

/* Multiplicateur de lumière de l'exposition : 2^exposure_ev. */
float eng_tonemap_exposure_scale(const EngTonemap *t);

/* Nom lisible de l'opérateur (« aucun », « ACES », « AgX »), « ? » s'il est inconnu. */
const char *eng_tonemap_operator_name(EngTonemapOperator op);

/* Exposition puis opérateur. `rgb` est de la lumière linéaire >= 0 (les négatifs et NaN sont
 * traités comme 0, les valeurs au-delà de 65504, le plus grand RGBA16F fini, comme 65504 : un
 * débordement +inf donne du blanc, pas un pixel noir) ; `out` est de la lumière linéaire
 * d'AFFICHAGE dans [0, 1] (avant encodage sRGB). `t` doit être valide (eng_tonemap_sanitize). `rgb`
 * et `out` peuvent être le même tableau. */
void eng_tonemap_apply(const EngTonemap *t, const float rgb[3], float out[3]);

/* Encodage sRGB (OETF) d'une valeur linéaire, bornée à [0, 1] au préalable ; NaN -> 0. */
float eng_srgb_encode(float linear);
/* Opération inverse : valeur sRGB [0, 1] -> linéaire. */
float eng_srgb_decode(float encoded);
