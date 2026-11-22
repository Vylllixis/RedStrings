/* Code tiers : FastNoiseLite (Auburn/FastNoiseLite, version C, MIT), compilé sans -Werror (voir
 * engine/CMakeLists.txt). Seul fichier qui en voit les types. */
#define FNL_IMPL
#include <FastNoiseLite.h>

#include "engine/noise.h"

void eng_noise_row_2d(const EngNoiseParams *params, float x0, float step, float y, uint32_t count,
                      float *out)
{
    fnl_state state = fnlCreateState();
    state.seed = params->seed;
    state.frequency = params->frequency;
    state.noise_type = FNL_NOISE_OPENSIMPLEX2;
    state.fractal_type = params->fractal == ENG_NOISE_RIDGED ? FNL_FRACTAL_RIDGED : FNL_FRACTAL_FBM;
    state.octaves = (int)params->octaves;
    state.lacunarity = params->lacunarity;
    state.gain = params->gain;
    for (uint32_t i = 0; i < count; i++) {
        out[i] = fnlGetNoise2D(&state, x0 + ((float)i * step), y);
    }
}
