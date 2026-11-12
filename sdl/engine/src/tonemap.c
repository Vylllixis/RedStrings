#include "engine/tonemap.h"

#include <math.h>

#define HALF_MAX 65504.0F /* plus grand flottant 16 bits fini */

void eng_tonemap_default(EngTonemap *t)
{
    t->op = ENG_TONEMAP_ACES;
    t->exposure_ev = 0.0F;
}

bool eng_tonemap_sanitize(EngTonemap *t)
{
    bool clean = true;
    if ((int)t->op < 0 || (int)t->op >= ENG_TONEMAP_COUNT) {
        t->op = ENG_TONEMAP_ACES;
        clean = false;
    }
    if (!isfinite(t->exposure_ev)) {
        t->exposure_ev = 0.0F;
        clean = false;
    } else if (t->exposure_ev > ENG_EXPOSURE_EV_MAX) {
        t->exposure_ev = ENG_EXPOSURE_EV_MAX;
        clean = false;
    } else if (t->exposure_ev < ENG_EXPOSURE_EV_MIN) {
        t->exposure_ev = ENG_EXPOSURE_EV_MIN;
        clean = false;
    }
    return clean;
}

float eng_tonemap_exposure_scale(const EngTonemap *t)
{
    return exp2f(t->exposure_ev);
}

const char *eng_tonemap_operator_name(EngTonemapOperator op)
{
    switch (op) {
    case ENG_TONEMAP_NONE:
        return "aucun";
    case ENG_TONEMAP_ACES:
        return "ACES";
    case ENG_TONEMAP_AGX:
        return "AgX";
    default:
        return "?";
    }
}

static float saturate(float x)
{
    if (!(x > 0.0F)) { /* négatif ou NaN */
        return 0.0F;
    }
    return x > 1.0F ? 1.0F : x;
}

static float aces(float x)
{
    return saturate((x * (2.51F * x + 0.03F)) / (x * (2.43F * x + 0.59F) + 0.14F));
}

/* Résultat d'une matrice GLSL mat3(c0, c1, c2) multipliée par v : c0 v.x + c1 v.y + c2 v.z (les
 * trois vecteurs sont les COLONNES, comme dans three.js dont viennent les constantes). */
static void mul_columns(const float c0[3], const float c1[3], const float c2[3], const float v[3],
                        float out[3])
{
    float result[3];
    for (int i = 0; i < 3; i++) {
        result[i] = c0[i] * v[0] + c1[i] * v[1] + c2[i] * v[2];
    }
    out[0] = result[0];
    out[1] = result[1];
    out[2] = result[2];
}

static float agx_contrast(float x)
{
    const float x2 = x * x;
    const float x4 = x2 * x2;
    return 15.5F * x4 * x2 - 40.14F * x4 * x + 31.96F * x4 - 6.868F * x2 * x + 0.4298F * x2 +
           0.1191F * x - 0.00232F;
}

static void agx(const float in[3], float out[3])
{
    static const float srgb_to_rec2020_0[3] = {0.6274F, 0.0691F, 0.0164F};
    static const float srgb_to_rec2020_1[3] = {0.3293F, 0.9195F, 0.0880F};
    static const float srgb_to_rec2020_2[3] = {0.0433F, 0.0113F, 0.8956F};
    static const float inset_0[3] = {0.856627153315983F, 0.137318972929847F, 0.11189821299995F};
    static const float inset_1[3] = {0.0951212405381588F, 0.761241990602591F, 0.0767994186031903F};
    static const float inset_2[3] = {0.0482516061458583F, 0.101439036467562F, 0.811302368396859F};
    static const float outset_0[3] = {1.1271005818144368F, -0.1413297634984383F,
                                      -0.14132976349843826F};
    static const float outset_1[3] = {-0.11060664309660323F, 1.157823702216272F,
                                      -0.11060664309660294F};
    static const float outset_2[3] = {-0.016493938717834573F, -0.016493938717834257F,
                                      1.2519364065950405F};
    static const float rec2020_to_srgb_0[3] = {1.6605F, -0.1246F, -0.0182F};
    static const float rec2020_to_srgb_1[3] = {-0.5876F, 1.1329F, -0.1006F};
    static const float rec2020_to_srgb_2[3] = {-0.0728F, -0.0083F, 1.1187F};
    static const float min_ev = -12.47393F;
    static const float max_ev = 4.026069F;

    float c[3];
    mul_columns(srgb_to_rec2020_0, srgb_to_rec2020_1, srgb_to_rec2020_2, in, c);
    mul_columns(inset_0, inset_1, inset_2, c, c);
    for (int i = 0; i < 3; i++) {
        c[i] = log2f(c[i] > 1e-10F ? c[i] : 1e-10F);
        c[i] = (c[i] - min_ev) / (max_ev - min_ev);
        c[i] = agx_contrast(saturate(c[i]));
    }
    mul_columns(outset_0, outset_1, outset_2, c, c);
    for (int i = 0; i < 3; i++) {
        c[i] = powf(c[i] > 0.0F ? c[i] : 0.0F, 2.2F);
    }
    mul_columns(rec2020_to_srgb_0, rec2020_to_srgb_1, rec2020_to_srgb_2, c, out);
    for (int i = 0; i < 3; i++) {
        out[i] = saturate(out[i]);
    }
}

void eng_tonemap_apply(const EngTonemap *t, const float rgb[3], float out[3])
{
    const float scale = eng_tonemap_exposure_scale(t);
    float c[3];
    for (int i = 0; i < 3; i++) {
        const float v = rgb[i];
        /* négatif et NaN : 0 ; plafond du plus grand RGBA16F fini : +inf (débordement) donne du
         * blanc au lieu de inf/inf = NaN, c'est-à-dire d'un pixel noir */
        c[i] = (v > 0.0F ? (v < HALF_MAX ? v : HALF_MAX) : 0.0F) * scale;
    }
    switch (t->op) {
    case ENG_TONEMAP_ACES:
        for (int i = 0; i < 3; i++) {
            out[i] = aces(c[i]);
        }
        break;
    case ENG_TONEMAP_AGX:
        agx(c, out);
        break;
    case ENG_TONEMAP_NONE:
    default:
        for (int i = 0; i < 3; i++) {
            out[i] = saturate(c[i]);
        }
        break;
    }
}

float eng_srgb_encode(float linear)
{
    const float x = saturate(linear);
    return x <= 0.0031308F ? 12.92F * x : 1.055F * powf(x, 1.0F / 2.4F) - 0.055F;
}

float eng_srgb_decode(float encoded)
{
    const float x = saturate(encoded);
    return x <= 0.04045F ? x / 12.92F : powf((x + 0.055F) / 1.055F, 2.4F);
}
