#include "engine/csm.h"

#include <math.h>
#include <string.h>

bool eng_csm_splits(float n, float f, uint32_t count, float lambda, float *out)
{
    if (out == NULL || count == 0U || count > ENG_CSM_MAX || !(n > 0.0F) || !(f > n) ||
        !(lambda >= 0.0F) || lambda > 1.0F) {
        return false;
    }
    for (uint32_t i = 0; i <= count; i++) {
        const float t = (float)i / (float)count;
        const float lg = n * powf(f / n, t);
        const float un = n + (f - n) * t;
        out[i] = lambda * lg + (1.0F - lambda) * un;
    }
    out[0] = n;
    out[count] = f;
    return true;
}

void eng_csm_slice_sphere(float d0, float d1, float fov_y, float aspect, float *center_dist,
                          float *radius)
{
    /* demi-diagonale du plan à la distance d : d * k, k = sqrt(tan²(fy/2) (1 + aspect²)) */
    const float t = tanf(fov_y * 0.5F);
    const float k2 = t * t * (1.0F + aspect * aspect);
    /* centre sur l'axe, équidistant des coins proches et lointains :
     * (c - d0)² + d0² k2 = (d1 - c)² + d1² k2  =>  c = (d1 + d0)(1 + k2) / 2 */
    float c = 0.5F * (d0 + d1) * (1.0F + k2);
    if (c > d1) { /* tranche très large : la sphère centrée sur le plan lointain suffit */
        c = d1;
    }
    const float r_near = sqrtf((c - d0) * (c - d0) + d0 * d0 * k2);
    const float r_far = sqrtf((d1 - c) * (d1 - c) + d1 * d1 * k2);
    *center_dist = c;
    *radius = r_near > r_far ? r_near : r_far;
}

bool eng_csm_update(EngCsm *csm, const EngCsmView *v, const float to_sun[3], uint32_t count,
                    float lambda, uint32_t size)
{
    if (csm == NULL || v == NULL || to_sun == NULL || !(v->fov_y > 0.0F) || !(v->aspect > 0.0F)) {
        return false;
    }
    EngCsm tmp;
    memset(&tmp, 0, sizeof tmp);
    if (!eng_csm_splits(v->near_plane, v->far_plane, count, lambda, tmp.splits)) {
        return false;
    }
    tmp.count = count;
    for (uint32_t i = 0; i < count; i++) {
        float cd = 0.0F;
        float r = 0.0F;
        eng_csm_slice_sphere(tmp.splits[i], tmp.splits[i + 1U], v->fov_y, v->aspect, &cd, &r);
        /* rayon arrondi au décimètre : une petite variation de near/far ne change pas les texels */
        r = ceilf(r * 10.0F) / 10.0F;
        const float c[3] = {v->position[0] + v->forward[0] * cd,
                            v->position[1] + v->forward[1] * cd,
                            v->position[2] + v->forward[2] * cd};
        if (!eng_shadow_fit(&tmp.maps[i], to_sun, c, r, size)) {
            return false;
        }
        tmp.radius[i] = r;
    }
    *csm = tmp;
    return true;
}

uint32_t eng_csm_pick(const EngCsm *csm, float view_depth)
{
    if (csm == NULL) {
        return 0;
    }
    for (uint32_t i = 0; i < csm->count; i++) {
        if (view_depth <= csm->splits[i + 1U]) {
            return i;
        }
    }
    return csm->count;
}
