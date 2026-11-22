/* Arbre scanné -> arbre de jeu : regroupement de points en grappes voisines (k-moyennes). Voir
 * engine/tree_bake.h. */
#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#include "engine/tree_bake.h"

#define SAMPLE_MAX                                                                                 \
    120000U /* points tirés pour chercher les centres (tous sont ensuite classés)                \
             */
#define LLOYD_PASSES 16U

/* générateur fixe (xorshift32) : mêmes entrées, mêmes grappes */
static uint32_t next_random(uint32_t *state)
{
    uint32_t x = *state != 0U ? *state : 0x9E3779B9U;
    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    *state = x;
    return x;
}

static float distance2(const float *a, const float *b)
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return (dx * dx) + (dy * dy) + (dz * dz);
}

static uint32_t nearest(const float *point, const float *centers, uint32_t k)
{
    uint32_t best = 0;
    float best_d = distance2(point, centers);
    for (uint32_t c = 1; c < k; c++) {
        const float d = distance2(point, &centers[(size_t)c * 3U]);
        if (d < best_d) {
            best_d = d;
            best = c;
        }
    }
    return best;
}

/* départ « k-means++ » : chaque nouveau centre est tiré loin des précédents (probabilité
 * proportionnelle au carré de la distance au centre le plus proche) */
static void seed_centers(const float *sample, const float *weight, uint32_t n, uint32_t k,
                         uint32_t seed, float *centers, float *nearest_d)
{
    uint32_t state = seed;
    memcpy(centers, &sample[(size_t)(next_random(&state) % n) * 3U], 3U * sizeof(float));
    for (uint32_t i = 0; i < n; i++) {
        nearest_d[i] = distance2(&sample[(size_t)i * 3U], centers) * weight[i];
    }
    for (uint32_t c = 1; c < k; c++) {
        double total = 0.0;
        for (uint32_t i = 0; i < n; i++) {
            total += (double)nearest_d[i];
        }
        uint32_t pick = next_random(&state) % n;
        if (total > 0.0) {
            double at = ((double)next_random(&state) / 4294967296.0) * total;
            for (uint32_t i = 0; i < n; i++) {
                at -= (double)nearest_d[i];
                if (at <= 0.0) {
                    pick = i;
                    break;
                }
            }
        }
        float *center = &centers[(size_t)c * 3U];
        memcpy(center, &sample[(size_t)pick * 3U], 3U * sizeof(float));
        for (uint32_t i = 0; i < n; i++) {
            const float d = distance2(&sample[(size_t)i * 3U], center) * weight[i];
            nearest_d[i] = d < nearest_d[i] ? d : nearest_d[i];
        }
    }
}

/* une passe de Lloyd sur l'échantillon : chaque centre va au barycentre des points qui lui sont
 * le plus proches ; un centre sans point reste où il est. Renvoie le nombre de points qui ont
 * changé de grappe. */
static uint32_t lloyd_pass(const float *sample, const float *weight, uint32_t n, uint32_t k,
                           float *centers, uint32_t *owner, double *sums)
{
    uint32_t moved = 0;
    memset(sums, 0, (size_t)k * 4U * sizeof(double));
    for (uint32_t i = 0; i < n; i++) {
        const float *p = &sample[(size_t)i * 3U];
        const uint32_t c = nearest(p, centers, k);
        moved += owner[i] != c ? 1U : 0U;
        owner[i] = c;
        double *s = &sums[(size_t)c * 4U];
        s[0] += (double)p[0] * (double)weight[i];
        s[1] += (double)p[1] * (double)weight[i];
        s[2] += (double)p[2] * (double)weight[i];
        s[3] += (double)weight[i];
    }
    for (uint32_t c = 0; c < k; c++) {
        const double *s = &sums[(size_t)c * 4U];
        if (s[3] > 0.0) {
            for (int a = 0; a < 3; a++) {
                centers[((size_t)c * 3U) + (size_t)a] = (float)(s[a] / s[3]);
            }
        }
    }
    return moved;
}

static bool points_finite(const float *points, uint32_t count)
{
    for (size_t i = 0; i < (size_t)count * 3U; i++) {
        if (!isfinite(points[i])) {
            return false;
        }
    }
    return true;
}

uint32_t eng_bake_cluster(const float *points, const float *weights, uint32_t count, uint32_t k,
                          uint32_t seed, uint32_t *assignment, float *centers)
{
    if (points == NULL || assignment == NULL || centers == NULL || count == 0U || k == 0U ||
        !points_finite(points, count)) {
        return 0;
    }
    const uint32_t wanted = k < count ? k : count;
    const uint32_t step = count > SAMPLE_MAX ? count / SAMPLE_MAX : 1U;
    const uint32_t n = (count + step - 1U) / step;
    float *sample = SDL_malloc((size_t)n * 3U * sizeof(float));
    float *weight = SDL_malloc((size_t)n * sizeof(float));
    float *nearest_d = SDL_malloc((size_t)n * sizeof(float));
    uint32_t *owner = SDL_malloc((size_t)n * sizeof(uint32_t));
    double *sums = SDL_malloc((size_t)wanted * 4U * sizeof(double));
    uint32_t *remap = SDL_malloc((size_t)wanted * sizeof(uint32_t));
    uint32_t kept = 0;
    if (sample != NULL && weight != NULL && nearest_d != NULL && owner != NULL && sums != NULL &&
        remap != NULL) {
        for (uint32_t i = 0; i < n; i++) {
            const uint32_t source = i * step;
            memcpy(&sample[(size_t)i * 3U], &points[(size_t)source * 3U], 3U * sizeof(float));
            const float w = weights != NULL ? weights[source] : 1.0F;
            weight[i] = isfinite(w) && w > 0.0F ? w : 0.0F;
            owner[i] = UINT32_MAX;
        }
        seed_centers(sample, weight, n, wanted, seed, centers, nearest_d);
        for (uint32_t pass = 0; pass < LLOYD_PASSES; pass++) {
            if (lloyd_pass(sample, weight, n, wanted, centers, owner, sums) == 0U) {
                break;
            }
        }
        /* tous les points : grappe la plus proche, puis centres = barycentres, grappes vides
         * retirées */
        memset(sums, 0, (size_t)wanted * 4U * sizeof(double));
        for (uint32_t i = 0; i < count; i++) {
            const float *p = &points[(size_t)i * 3U];
            const uint32_t c = nearest(p, centers, wanted);
            assignment[i] = c;
            const double w = weights != NULL && isfinite(weights[i]) && weights[i] > 0.0F
                                 ? (double)weights[i]
                                 : 1e-12;
            double *s = &sums[(size_t)c * 4U];
            s[0] += (double)p[0] * w;
            s[1] += (double)p[1] * w;
            s[2] += (double)p[2] * w;
            s[3] += w;
        }
        for (uint32_t c = 0; c < wanted; c++) {
            const double *s = &sums[(size_t)c * 4U];
            remap[c] = UINT32_MAX;
            if (s[3] > 0.0) {
                for (int a = 0; a < 3; a++) {
                    centers[((size_t)kept * 3U) + (size_t)a] = (float)(s[a] / s[3]);
                }
                remap[c] = kept++;
            }
        }
        for (uint32_t i = 0; i < count; i++) {
            assignment[i] = remap[assignment[i]];
        }
    }
    SDL_free(sample);
    SDL_free(weight);
    SDL_free(nearest_d);
    SDL_free(owner);
    SDL_free(sums);
    SDL_free(remap);
    return kept;
}
