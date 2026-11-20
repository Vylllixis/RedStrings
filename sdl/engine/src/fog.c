#include "engine/fog.h"

#include <math.h>
#include <string.h>

void eng_fog_default(EngFog *fog)
{
    *fog = (EngFog){
        .density = 0.003F,
        .height_falloff = 0.004F,
        .base_height = 0.0F,
        .max_opacity = 1.0F,
    };
}

bool eng_fog_valid(const EngFog *fog)
{
    return fog != NULL && isfinite(fog->density) && fog->density >= 0.0F && fog->density <= 1.0F &&
           isfinite(fog->height_falloff) && fog->height_falloff >= 0.0F &&
           fog->height_falloff <= 1.0F && isfinite(fog->base_height) &&
           isfinite(fog->max_opacity) && fog->max_opacity >= 0.0F && fog->max_opacity <= 1.0F;
}

static float clamp_f(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

float eng_fog_optical_depth(const EngFog *fog, const float camera[3], const float point[3])
{
    if (!eng_fog_valid(fog)) {
        return 0.0F;
    }
    for (int c = 0; c < 3; c++) {
        if (!isfinite(camera[c]) || !isfinite(point[c])) {
            return 0.0F;
        }
    }
    const float dx = point[0] - camera[0];
    const float dy = point[1] - camera[1];
    const float dz = point[2] - camera[2];
    const float distance = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
    const float x = clamp_f(fog->height_falloff * dy, -80.0F, 80.0F);
    /* (1 - exp(-x)) / x, avec sa limite en 0 */
    const float shape = fabsf(x) < 1e-4F ? 1.0F - (0.5F * x) : (1.0F - expf(-x)) / x;
    const float at_camera =
        expf(clamp_f(-fog->height_falloff * (camera[1] - fog->base_height), -80.0F, 80.0F));
    return fog->density * at_camera * distance * shape;
}

float eng_fog_factor(const EngFog *fog, const float camera[3], const float point[3])
{
    if (!eng_fog_valid(fog)) {
        return 0.0F;
    }
    return fog->max_opacity * (1.0F - expf(-eng_fog_optical_depth(fog, camera, point)));
}

bool eng_fog_apply(const EngFog *fog, const EngAtmosphere *atmosphere, const float sun[3],
                   float sky_scale, const float camera[3], const float point[3], const float lit[3],
                   float out[3])
{
    memcpy(out, lit, 3 * sizeof(float));
    float horizon[3];
    const float to_point[3] = {point[0] - camera[0], 0.0F, point[2] - camera[2]};
    if (!eng_fog_valid(fog) || !(sky_scale >= 0.0F) ||
        !eng_atmosphere_horizon(atmosphere, sun, to_point, horizon)) {
        return false;
    }
    const float f = eng_fog_factor(fog, camera, point);
    for (int c = 0; c < 3; c++) {
        out[c] = lit[c] + (((horizon[c] * sky_scale) - lit[c]) * f);
    }
    return true;
}

void eng_fog_pack(const EngFog *fog, const EngAtmosphere *a, float sky_scale,
                  float out[ENG_FOG_UNIFORM_FLOATS])
{
    memset(out, 0, ENG_FOG_UNIFORM_FLOATS * sizeof(float));
    if (!eng_fog_valid(fog) || !eng_atmosphere_valid(a) || !(sky_scale >= 0.0F) ||
        !isfinite(sky_scale)) {
        return;
    }
    out[0] = fog->density;
    out[1] = fog->height_falloff;
    out[2] = fog->base_height;
    out[3] = fog->max_opacity;
    for (int c = 0; c < 3; c++) {
        out[4 + c] = a->rayleigh[c];
        out[8 + c] = a->night[c];
    }
    out[7] = a->mie;
    out[11] = a->irradiance;
    out[12] = a->mie_g;
    out[13] = a->path_limit;
    out[14] = a->sun_path_limit;
    out[15] = sky_scale;
}

void eng_fog_set_sun(float out[ENG_FOG_UNIFORM_FLOATS], const float sun[3])
{
    if (out == NULL) {
        return;
    }
    out[16] = 0.0F;
    out[17] = 0.0F;
    out[18] = 0.0F;
    out[19] = 0.0F;
    if (sun == NULL || !isfinite(sun[0]) || !isfinite(sun[1]) || !isfinite(sun[2])) {
        return;
    }
    const float len = sqrtf((sun[0] * sun[0]) + (sun[1] * sun[1]) + (sun[2] * sun[2]));
    if (!(len > 1e-6F)) {
        return;
    }
    out[16] = sun[0] / len;
    out[17] = sun[1] / len;
    out[18] = sun[2] / len;
    out[19] = 1.0F;
}
