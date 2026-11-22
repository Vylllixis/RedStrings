#include "engine/mesh_lod.h"

#include <math.h>
#include <string.h>

#define CLUSTER_HASH 65536U

bool eng_mesh_simplify(const float *pos, uint32_t vc, const uint32_t *idx, uint32_t ic,
                       uint32_t cells, float *out_pos, uint32_t out_vcap, uint32_t *out_vc,
                       uint32_t *out_idx, uint32_t out_icap, uint32_t *out_ic)
{
    if (pos == NULL || idx == NULL || out_pos == NULL || out_idx == NULL || out_vc == NULL ||
        out_ic == NULL || vc == 0U || ic % 3U != 0U || cells == 0U || cells > 1024U) {
        return false;
    }
    float mn[3] = {INFINITY, INFINITY, INFINITY};
    float mx[3] = {-INFINITY, -INFINITY, -INFINITY};
    for (uint32_t i = 0; i < vc; i++) {
        for (int a = 0; a < 3; a++) {
            const float v = pos[(size_t)i * 3U + (size_t)a];
            mn[a] = v < mn[a] ? v : mn[a];
            mx[a] = v > mx[a] ? v : mx[a];
        }
    }
    float cs = 0.0F;
    for (int a = 0; a < 3; a++) {
        cs = (mx[a] - mn[a]) > cs ? (mx[a] - mn[a]) : cs;
    }
    cs = cs > 0.0F ? cs / (float)cells : 1.0F;
    /* table de hachage case -> nouveau sommet (sondage linéaire) */
    static uint64_t keys[CLUSTER_HASH];
    static uint32_t vals[CLUSTER_HASH];
    static float sums[CLUSTER_HASH][4];
    static uint32_t remap_buf[1U << 20];
    if (vc > (1U << 20)) {
        return false;
    }
    memset(keys, 0xFF, sizeof keys);
    uint32_t nv = 0;
    for (uint32_t i = 0; i < vc; i++) {
        uint64_t key = 0;
        for (int a = 0; a < 3; a++) {
            uint32_t c = (uint32_t)((pos[(size_t)i * 3U + (size_t)a] - mn[a]) / cs);
            c = c >= cells ? cells - 1U : c;
            key = (key << 20) | c;
        }
        uint32_t h = (uint32_t)((key * 0x9E3779B97F4A7C15ULL) >> 48) % CLUSTER_HASH;
        while (keys[h] != UINT64_MAX && keys[h] != key) {
            h = (h + 1U) % CLUSTER_HASH;
        }
        if (keys[h] == UINT64_MAX) {
            if (nv >= out_vcap || nv >= CLUSTER_HASH / 2U) {
                return false;
            }
            keys[h] = key;
            vals[h] = nv++;
            sums[h][0] = sums[h][1] = sums[h][2] = sums[h][3] = 0.0F;
        }
        for (int a = 0; a < 3; a++) {
            sums[h][a] += pos[(size_t)i * 3U + (size_t)a];
        }
        sums[h][3] += 1.0F;
        remap_buf[i] = h;
    }
    for (uint32_t h = 0; h < CLUSTER_HASH; h++) {
        if (keys[h] != UINT64_MAX) {
            for (int a = 0; a < 3; a++) {
                out_pos[(size_t)vals[h] * 3U + (size_t)a] = sums[h][a] / sums[h][3];
            }
        }
    }
    uint32_t ni = 0;
    for (uint32_t t = 0; t < ic; t += 3U) {
        if (idx[t] >= vc || idx[t + 1U] >= vc || idx[t + 2U] >= vc) {
            return false;
        }
        const uint32_t a = vals[remap_buf[idx[t]]];
        const uint32_t b = vals[remap_buf[idx[t + 1U]]];
        const uint32_t c = vals[remap_buf[idx[t + 2U]]];
        if (a == b || b == c || a == c) {
            continue; /* dégénéré : retiré */
        }
        if (ni + 3U > out_icap) {
            return false;
        }
        out_idx[ni++] = a;
        out_idx[ni++] = b;
        out_idx[ni++] = c;
    }
    *out_vc = nv;
    *out_ic = ni;
    return true;
}

uint32_t eng_mesh_lod_select(uint32_t current, float distance, const float *thresholds,
                             uint32_t count, float hysteresis)
{
    if (thresholds == NULL || count == 0U || !isfinite(distance)) {
        return 0;
    }
    uint32_t want = 0;
    while (want + 1U < count && distance >= thresholds[want]) {
        want++;
    }
    if (current >= count) {
        return want;
    }
    if (want < current) { /* vers plus fin : seulement bien en dessous du seuil */
        uint32_t lvl = current;
        while (lvl > want && distance < thresholds[lvl - 1U] * (1.0F - hysteresis)) {
            lvl--;
        }
        return lvl;
    }
    return want;
}
