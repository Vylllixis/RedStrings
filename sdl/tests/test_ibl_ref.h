#pragma once

#include "engine/ibl.h"

/* Référence CPU de model.frag avec l'environnement PAR DÉFAUT d'un modèle GPU (ciel uniforme de
 * radiance 1 : irradiance 1, préfiltrée 1) : eng_pbr_shade_ibl avec la LUT BRDF calculée ici (1024
 * échantillons, indépendante de celle que le moteur envoie au GPU : 512). */
static inline void test_shade(const EngLighting *light, const EngPbrSurface *surface,
                              const float normal[3], const float position[3], float out[3])
{
    static float lut[ENG_IBL_BRDF_LUT_SIZE * ENG_IBL_BRDF_LUT_SIZE * 2U];
    static bool ready;
    static const float ones[6 * 3] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    if (!ready) {
        eng_ibl_brdf_lut(lut, ENG_IBL_BRDF_LUT_SIZE, 1024U);
        ready = true;
    }
    const EngIbl ibl = {.irradiance = ones,
                        .irradiance_size = 1U,
                        .prefiltered = ones,
                        .prefiltered_size = 1U,
                        .prefiltered_levels = 1U,
                        .brdf_lut = lut};
    eng_pbr_shade_ibl(light, &ibl, surface, normal, position, out);
}
