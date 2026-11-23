#include "engine/lighting.h"

#include <math.h>
#include <string.h>

#define DEG_TO_RAD 0.017453292519943295F

static float clamp_nonneg(float v, bool *fixed)
{
    if (!(v >= 0.0F)) { /* négatif ou NaN */
        *fixed = true;
        return 0.0F;
    }
    if (isinf(v)) {
        *fixed = true;
        return 0.0F;
    }
    return v;
}

void eng_lighting_default(EngLighting *lighting)
{
    *lighting = (EngLighting){0};
    (void)eng_lighting_set_sun_angles(lighting, 30.0F, 45.0F);
    for (int c = 0; c < 3; c++) {
        lighting->sun_color[c] = 1.0F;
        lighting->ambient_color[c] = 1.0F;
    }
    lighting->sun_intensity = 1.0F;
    lighting->ambient_intensity = 0.25F;
}

bool eng_lighting_set_sun_angles(EngLighting *lighting, float azimuth_deg, float elevation_deg)
{
    if (!isfinite(azimuth_deg) || !isfinite(elevation_deg)) {
        return false;
    }
    if (elevation_deg > 90.0F) {
        elevation_deg = 90.0F;
    } else if (elevation_deg < -90.0F) {
        elevation_deg = -90.0F;
    }
    const float az = azimuth_deg * DEG_TO_RAD;
    const float el = elevation_deg * DEG_TO_RAD;
    const float horizontal = cosf(el);
    lighting->sun_direction[0] = horizontal * sinf(az);
    lighting->sun_direction[1] = sinf(el);
    lighting->sun_direction[2] = horizontal * cosf(az);
    return true;
}

bool eng_lighting_sanitize(EngLighting *lighting)
{
    bool fixed = false;
    float len2 = 0.0F;
    bool finite = true;
    for (int c = 0; c < 3; c++) {
        finite = finite && isfinite(lighting->sun_direction[c]);
        len2 += lighting->sun_direction[c] * lighting->sun_direction[c];
    }
    if (!finite || !(len2 > 1e-12F) || isinf(len2)) {
        lighting->sun_direction[0] = 0.0F;
        lighting->sun_direction[1] = 1.0F;
        lighting->sun_direction[2] = 0.0F;
        fixed = true;
    } else if (fabsf(len2 - 1.0F) > 1e-4F) {
        const float inv = 1.0F / sqrtf(len2);
        for (int c = 0; c < 3; c++) {
            lighting->sun_direction[c] *= inv;
        }
        fixed = true;
    }
    for (int c = 0; c < 3; c++) {
        lighting->sun_color[c] = clamp_nonneg(lighting->sun_color[c], &fixed);
        lighting->ambient_color[c] = clamp_nonneg(lighting->ambient_color[c], &fixed);
        if (!isfinite(lighting->camera_position[c])) {
            lighting->camera_position[c] = 0.0F;
            fixed = true;
        }
    }
    lighting->sun_intensity = clamp_nonneg(lighting->sun_intensity, &fixed);
    lighting->ambient_intensity = clamp_nonneg(lighting->ambient_intensity, &fixed);
    return !fixed;
}
