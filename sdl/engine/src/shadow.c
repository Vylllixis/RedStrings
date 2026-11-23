#include "engine/shadow.h"

#include <math.h>

static bool finite3(const float v[3])
{
    return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]);
}

void eng_shadow_params_default(EngShadowParams *params)
{
    *params = (EngShadowParams){
        .constant_bias = 1.0F,
        .slope_bias = 1.0F,
        .normal_bias = 1.0F,
        .pcf_radius = 1U,
        .strength = 1.0F,
    };
}

static float clamp_non_negative(float v, bool *changed)
{
    if (!isfinite(v) || v < 0.0F) {
        *changed = true;
        return 0.0F;
    }
    return v;
}

bool eng_shadow_params_sanitize(EngShadowParams *params)
{
    bool changed = false;
    params->constant_bias = clamp_non_negative(params->constant_bias, &changed);
    params->slope_bias = clamp_non_negative(params->slope_bias, &changed);
    params->normal_bias = clamp_non_negative(params->normal_bias, &changed);
    if (params->pcf_radius > ENG_SHADOW_MAX_PCF_RADIUS) {
        params->pcf_radius = ENG_SHADOW_MAX_PCF_RADIUS;
        changed = true;
    }
    if (!isfinite(params->strength)) {
        params->strength = 1.0F;
        changed = true;
    } else if (params->strength < 0.0F || params->strength > 1.0F) {
        params->strength = params->strength < 0.0F ? 0.0F : 1.0F;
        changed = true;
    }
    return !changed;
}

void eng_shadow_sphere_from_box(const float box_min[3], const float box_max[3], float center[3],
                                float *radius)
{
    float half_sq = 0.0F;
    for (int a = 0; a < 3; a++) {
        center[a] = 0.5F * (box_min[a] + box_max[a]);
        const float half = 0.5F * (box_max[a] - box_min[a]);
        half_sq += half * half;
    }
    *radius = sqrtf(half_sq);
}

bool eng_shadow_fit(EngShadowMap *map, const float to_sun[3], const float center[3], float radius,
                    uint32_t size)
{
    if (!finite3(to_sun) || !finite3(center) || !isfinite(radius) || !(radius > 0.0F) ||
        size < 8U) {
        return false;
    }
    const float length_sq =
        (to_sun[0] * to_sun[0]) + (to_sun[1] * to_sun[1]) + (to_sun[2] * to_sun[2]);
    if (!(length_sq > 1e-12F)) {
        return false;
    }
    const float inv = 1.0F / sqrtf(length_sq);
    const vec3 sun = {to_sun[0] * inv, to_sun[1] * inv, to_sun[2] * inv};

    /* une marge de deux texels de chaque côté : la sphère occupe size - 4 texels ; l'alignement du
     * centre sur la grille la déplace d'un demi-texel au plus, il reste donc 1,5 texel de marge */
    const float half = radius * (float)size / (float)(size - 4U);
    const float texel = 2.0F * half / (float)size;

    /* base de la caméra du soleil : elle regarde vers -sun ; mêmes formules que glm_lookat */
    vec3 up = {0.0F, 1.0F, 0.0F};
    if (fabsf(sun[1]) > 0.99F) {
        up[1] = 0.0F;
        up[2] = 1.0F;
    }
    vec3 forward = {-sun[0], -sun[1], -sun[2]};
    vec3 right;
    glm_vec3_cross(forward, up, right);
    glm_vec3_normalize(right);
    vec3 up_l;
    glm_vec3_cross(right, forward, up_l);

    /* centre aligné sur la grille des texels (dans le plan de la carte) */
    const float cr = (center[0] * right[0]) + (center[1] * right[1]) + (center[2] * right[2]);
    const float cu = (center[0] * up_l[0]) + (center[1] * up_l[1]) + (center[2] * up_l[2]);
    const float dr = (roundf(cr / texel) * texel) - cr;
    const float du = (roundf(cu / texel) * texel) - cu;
    vec3 snapped;
    for (int a = 0; a < 3; a++) {
        snapped[a] = center[a] + (right[a] * dr) + (up_l[a] * du);
    }

    vec3 eye;
    for (int a = 0; a < 3; a++) {
        eye[a] = snapped[a] + (sun[a] * half);
    }
    mat4 view;
    mat4 proj;
    glm_lookat(eye, snapped, up, view);
    glm_ortho(-half, half, -half, half, 0.0F, 2.0F * half, proj);
    glm_mat4_mul(proj, view, map->view_proj);

    /* (x, y, z) clip -> (u, v, profondeur) : u = 0,5 x + 0,5 ; v = 0,5 - 0,5 y ; profondeur = z */
    mat4 remap = GLM_MAT4_IDENTITY_INIT;
    remap[0][0] = 0.5F;
    remap[3][0] = 0.5F;
    remap[1][1] = -0.5F;
    remap[3][1] = 0.5F;
    glm_mat4_mul(remap, map->view_proj, map->shadow_matrix);

    for (int a = 0; a < 3; a++) {
        map->to_sun[a] = sun[a];
    }
    map->texel_world = texel;
    map->depth_range = 2.0F * half;
    map->size = size;
    return true;
}

void eng_shadow_project(const EngShadowMap *map, const float world[3], float uvd[3])
{
    for (int r = 0; r < 3; r++) {
        uvd[r] = (map->shadow_matrix[0][r] * world[0]) + (map->shadow_matrix[1][r] * world[1]) +
                 (map->shadow_matrix[2][r] * world[2]) + map->shadow_matrix[3][r];
    }
}

void eng_shadow_receiver(const EngShadowMap *map, const EngShadowParams *params,
                         const float position[3], const float normal[3], float uvd[3])
{
    float cos_theta =
        (normal[0] * map->to_sun[0]) + (normal[1] * map->to_sun[1]) + (normal[2] * map->to_sun[2]);
    cos_theta = cos_theta < 0.0F ? 0.0F : (cos_theta > 1.0F ? 1.0F : cos_theta);
    const float sin_theta = sqrtf(1.0F - (cos_theta * cos_theta));
    float tan_theta = sin_theta / (cos_theta > 1e-4F ? cos_theta : 1e-4F);
    tan_theta = tan_theta > ENG_SHADOW_MAX_SLOPE ? ENG_SHADOW_MAX_SLOPE : tan_theta;

    float shifted[3];
    const float offset = params->normal_bias * sin_theta * map->texel_world;
    for (int a = 0; a < 3; a++) {
        shifted[a] = position[a] + (normal[a] * offset);
    }
    eng_shadow_project(map, shifted, uvd);
    const float texels =
        params->constant_bias + (params->slope_bias * (float)(params->pcf_radius + 1U) * tan_theta);
    uvd[2] -= texels / (float)map->size;
}

static float depth_at(const float *depth, uint32_t size, int x, int y)
{
    const int max = (int)size - 1;
    x = x < 0 ? 0 : (x > max ? max : x);
    y = y < 0 ? 0 : (y > max ? max : y);
    return depth[((size_t)y * size) + (size_t)x];
}

/* Quatre comparaisons (1 : lumière) interpolées bilinéairement autour de (u, v). */
static float bilinear_compare(const float *depth, uint32_t size, float u, float v, float reference)
{
    const float x = (u * (float)size) - 0.5F;
    const float y = (v * (float)size) - 0.5F;
    const float fx = floorf(x);
    const float fy = floorf(y);
    const float wx = x - fx;
    const float wy = y - fy;
    const int x0 = (int)fx;
    const int y0 = (int)fy;
    const float a = reference <= depth_at(depth, size, x0, y0) ? 1.0F : 0.0F;
    const float b = reference <= depth_at(depth, size, x0 + 1, y0) ? 1.0F : 0.0F;
    const float c = reference <= depth_at(depth, size, x0, y0 + 1) ? 1.0F : 0.0F;
    const float d = reference <= depth_at(depth, size, x0 + 1, y0 + 1) ? 1.0F : 0.0F;
    return (((a * (1.0F - wx)) + (b * wx)) * (1.0F - wy)) + (((c * (1.0F - wx)) + (d * wx)) * wy);
}

float eng_shadow_visibility(const float *depth, uint32_t size, const float uvd[3], uint32_t radius)
{
    if (uvd[0] < 0.0F || uvd[0] > 1.0F || uvd[1] < 0.0F || uvd[1] > 1.0F || uvd[2] < 0.0F ||
        uvd[2] > 1.0F) {
        return 1.0F;
    }
    if (radius > ENG_SHADOW_MAX_PCF_RADIUS) {
        radius = ENG_SHADOW_MAX_PCF_RADIUS;
    }
    const float texel = 1.0F / (float)size;
    const int r = (int)radius;
    float sum = 0.0F;
    for (int j = -r; j <= r; j++) {
        for (int i = -r; i <= r; i++) {
            sum += bilinear_compare(depth, size, uvd[0] + ((float)i * texel),
                                    uvd[1] + ((float)j * texel), uvd[2]);
        }
    }
    const float taps = (float)((2 * r) + 1);
    return sum / (taps * taps);
}

float eng_shadow_factor(const EngShadowParams *params, float visibility)
{
    return 1.0F - (params->strength * (1.0F - visibility));
}
