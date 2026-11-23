#include "engine/sfx.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#define TWO_PI 6.28318530717958647692F

typedef struct SfxRecipe {
    const char *name;
    float seconds;
    float noise_gain;
    float noise_decay; /* 1/s */
    float f1;
    float f1_decay;
    float f1_gain;
    float f2; /* 0 : absent */
    float f2_decay;
    float f2_gain;
    bool swell; /* enveloppe en cloche (souffle) au lieu d'une attaque sèche */
} SfxRecipe;

static const SfxRecipe RECIPES[ENG_SFX_COUNT] = {
    {"hit_light", 0.12F, 0.8F, 45.0F, 180.0F, 30.0F, 0.7F, 0.0F, 0.0F, 0.0F, false},
    {"hit_heavy", 0.25F, 1.0F, 22.0F, 90.0F, 14.0F, 1.0F, 55.0F, 10.0F, 0.6F, false},
    {"block", 0.15F, 0.35F, 60.0F, 620.0F, 25.0F, 0.6F, 1130.0F, 30.0F, 0.4F, false},
    {"parry", 0.30F, 0.25F, 90.0F, 1500.0F, 12.0F, 0.7F, 2250.0F, 14.0F, 0.5F, false},
    {"whoosh", 0.20F, 0.9F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, true},
    {"dodge", 0.25F, 0.8F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, true},
};

const char *eng_sfx_name(EngSfx s)
{
    return (unsigned)s < (unsigned)ENG_SFX_COUNT ? RECIPES[s].name : "?";
}

uint32_t eng_sfx_length(EngSfx s, uint32_t sample_rate)
{
    if ((unsigned)s >= (unsigned)ENG_SFX_COUNT || sample_rate < 8000U ||
        sample_rate > ENG_SFX_MAX_RATE) {
        return 0;
    }
    return (uint32_t)(RECIPES[s].seconds * (float)sample_rate + 0.5F);
}

uint32_t eng_sfx_synth(EngSfx s, uint32_t sample_rate, float *out, uint32_t cap)
{
    const uint32_t n = eng_sfx_length(s, sample_rate);
    if (n == 0U || out == NULL || cap < n) {
        return 0;
    }
    const SfxRecipe *r = &RECIPES[s];
    uint32_t seed = 0x9E3779B9U + (uint32_t)s * 7919U;
    float lp = 0.0F; /* filtre passe-bas du bruit des souffles */
    float peak = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        const float t = (float)i / (float)sample_rate;
        seed = seed * 1664525U + 1013904223U;
        const float white = (float)(seed >> 8) / 8388607.5F - 1.0F;
        float v = 0.0F;
        if (r->swell) {
            const float u = (float)i / (float)n;
            lp += (r->noise_gain > 0.0F ? 0.25F : 0.0F) * (white - lp);
            v = lp * sinf(3.14159265F * u) * r->noise_gain;
        } else {
            v = white * r->noise_gain * expf(-r->noise_decay * t);
            v += r->f1_gain * sinf(TWO_PI * r->f1 * t) * expf(-r->f1_decay * t);
            if (r->f2 > 0.0F) {
                v += r->f2_gain * sinf(TWO_PI * r->f2 * t) * expf(-r->f2_decay * t);
            }
        }
        out[i] = v;
        if (fabsf(v) > peak) {
            peak = fabsf(v);
        }
    }
    const float k = peak > 0.0F ? 0.9F / peak : 0.0F;
    const uint32_t fade = n < 64U ? n : 64U;
    for (uint32_t i = 0; i < n; i++) {
        float g = k;
        if (i + fade >= n) { /* fondu de sortie : finit exactement à 0 */
            g *= (float)(n - 1U - i) / (float)fade;
        }
        out[i] *= g;
    }
    return n;
}
