#include "engine/pbr.h"

#include <math.h>

#define PI_F 3.14159265358979323846F
#define F0_DIELECTRIC 0.04F
/* Plancher des dénominateurs : une normale perpendiculaire à la vue ou à la lumière ne divise pas
 * par zéro. */
#define MIN_DOT 1e-4F

static float dot3(const float a[3], const float b[3])
{
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

static void normalize3(float v[3])
{
    const float len2 = dot3(v, v);
    if (len2 > 1e-20F) {
        const float inv = 1.0F / sqrtf(len2);
        for (int c = 0; c < 3; c++) {
            v[c] *= inv;
        }
    }
}

static float saturate(float v)
{
    return v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v);
}

void eng_pbr_surface_default(EngPbrSurface *surface)
{
    *surface = (EngPbrSurface){.base_color = {1.0F, 1.0F, 1.0F},
                               .metallic = 0.0F,
                               .roughness = 1.0F,
                               .occlusion = 1.0F,
                               .emissive = {0.0F, 0.0F, 0.0F}};
}

/* v dans [0, 1] ; une valeur non numérique devient `fallback`. */
static float unit_or(float v, float fallback, bool *fixed)
{
    if (!(v >= 0.0F) && !(v < 0.0F)) { /* NaN */
        *fixed = true;
        return fallback;
    }
    if (v < 0.0F) {
        *fixed = true;
        return 0.0F;
    }
    if (v > 1.0F) {
        *fixed = true;
        return 1.0F;
    }
    return v;
}

static float nonneg_or_zero(float v, bool *fixed)
{
    if (!(v >= 0.0F) || isinf(v)) { /* négatif, NaN ou infini */
        *fixed = true;
        return 0.0F;
    }
    return v;
}

bool eng_pbr_sanitize(EngPbrSurface *surface)
{
    bool fixed = false;
    surface->metallic = unit_or(surface->metallic, 0.0F, &fixed);
    surface->roughness = unit_or(surface->roughness, 1.0F, &fixed);
    surface->occlusion = unit_or(surface->occlusion, 1.0F, &fixed);
    for (int c = 0; c < 3; c++) {
        surface->base_color[c] = nonneg_or_zero(surface->base_color[c], &fixed);
        surface->emissive[c] = nonneg_or_zero(surface->emissive[c], &fixed);
    }
    return !fixed;
}

float eng_pbr_ndf_ggx(float n_dot_h, float alpha)
{
    const float a2 = alpha * alpha;
    const float d = (n_dot_h * n_dot_h * (a2 - 1.0F)) + 1.0F;
    return a2 / (PI_F * d * d);
}

float eng_pbr_visibility_smith_ggx(float n_dot_l, float n_dot_v, float alpha)
{
    const float a2 = alpha * alpha;
    const float ggx_v = n_dot_l * sqrtf((n_dot_v * n_dot_v * (1.0F - a2)) + a2);
    const float ggx_l = n_dot_v * sqrtf((n_dot_l * n_dot_l * (1.0F - a2)) + a2);
    const float sum = ggx_v + ggx_l;
    return sum > 0.0F ? 0.5F / sum : 0.0F;
}

float eng_pbr_fresnel_schlick(float f0, float v_dot_h)
{
    const float m = 1.0F - saturate(v_dot_h);
    const float m2 = m * m;
    return f0 + ((1.0F - f0) * m2 * m2 * m);
}

void eng_pbr_env_brdf(float n_dot_v, float roughness, float *a, float *b)
{
    /* Karis, « Physically Based Shading on Mobile » (2014) */
    const float c0[4] = {-1.0F, -0.0275F, -0.572F, 0.022F};
    const float c1[4] = {1.0F, 0.0425F, 1.04F, -0.04F};
    float r[4];
    for (int i = 0; i < 4; i++) {
        r[i] = (roughness * c0[i]) + c1[i];
    }
    const float e = exp2f(-9.28F * n_dot_v);
    const float a004 = (fminf(r[0] * r[0], e) * r[0]) + r[1];
    *a = (-1.04F * a004) + r[2];
    *b = (1.04F * a004) + r[3];
}

void eng_pbr_perturb_normal(const float normal[3], const float tangent[4], const float sample[3],
                            float scale, float out[3])
{
    float t[3];
    const float along = dot3(tangent, normal);
    for (int c = 0; c < 3; c++) {
        t[c] = tangent[c] - (normal[c] * along);
    }
    normalize3(t);
    const float b[3] = {((normal[1] * t[2]) - (normal[2] * t[1])) * tangent[3],
                        ((normal[2] * t[0]) - (normal[0] * t[2])) * tangent[3],
                        ((normal[0] * t[1]) - (normal[1] * t[0])) * tangent[3]};
    const float x = ((2.0F * sample[0]) - 1.0F) * scale;
    const float y = ((2.0F * sample[1]) - 1.0F) * scale;
    const float z = (2.0F * sample[2]) - 1.0F;
    for (int c = 0; c < 3; c++) {
        out[c] = (t[c] * x) + (b[c] * y) + (normal[c] * z);
    }
    normalize3(out);
}

void eng_pbr_shade(const EngLighting *lighting, const EngPbrSurface *surface, const float normal[3],
                   const float position[3], float out[3])
{
    float n[3] = {normal[0], normal[1], normal[2]};
    normalize3(n);
    float v[3];
    for (int c = 0; c < 3; c++) {
        v[c] = lighting->camera_position[c] - position[c];
    }
    normalize3(v);
    const float *l = lighting->sun_direction;
    float h[3];
    for (int c = 0; c < 3; c++) {
        h[c] = l[c] + v[c];
    }
    normalize3(h);

    const float roughness = fmaxf(surface->roughness, ENG_ROUGHNESS_MIN);
    const float alpha = roughness * roughness;
    const float n_dot_l = dot3(n, l);
    const float n_dot_v = fmaxf(dot3(n, v), MIN_DOT);
    const float n_dot_h = fmaxf(dot3(n, h), 0.0F);
    const float v_dot_h = fmaxf(dot3(v, h), 0.0F);

    float a_term = 0.0F;
    float b_term = 0.0F;
    eng_pbr_env_brdf(n_dot_v, roughness, &a_term, &b_term);
    const float specular_brdf = n_dot_l > 0.0F
                                    ? eng_pbr_ndf_ggx(n_dot_h, alpha) *
                                          eng_pbr_visibility_smith_ggx(n_dot_l, n_dot_v, alpha)
                                    : 0.0F;
    const float lit = n_dot_l > 0.0F ? n_dot_l : 0.0F;
    for (int c = 0; c < 3; c++) {
        const float f0 =
            F0_DIELECTRIC + ((surface->base_color[c] - F0_DIELECTRIC) * surface->metallic);
        const float c_diff = surface->base_color[c] * (1.0F - surface->metallic);
        const float fresnel = eng_pbr_fresnel_schlick(f0, v_dot_h);
        const float sun = lighting->sun_color[c] * lighting->sun_intensity;
        const float ambient = lighting->ambient_color[c] * lighting->ambient_intensity;
        const float direct =
            sun * lit * (((1.0F - fresnel) * c_diff) + (PI_F * fresnel * specular_brdf));
        const float indirect = surface->occlusion * ambient * (c_diff + (f0 * a_term) + b_term);
        out[c] = direct + indirect + surface->emissive[c];
    }
}
