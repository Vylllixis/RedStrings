/* Arbre scanné -> arbre de jeu : repère d'une grappe, ses axes principaux (vecteurs propres de la
 * covariance de ses points) et son étendue le long de chacun. Voir engine/tree_bake.h. */
#include <math.h>
#include <string.h>

#include "engine/tree_bake.h"

#define JACOBI_SWEEPS 24
#define HALF_MIN 1e-4F /* demi-étendue minimale d'une grappe plate ou réduite à un point */

/* diagonalise la matrice symétrique `a` (rotations de Jacobi) : `a` devient diagonale, les
 * colonnes de `v` sont les vecteurs propres */
static void jacobi(double a[3][3], double v[3][3])
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            v[i][j] = i == j ? 1.0 : 0.0;
        }
    }
    for (int sweep = 0; sweep < JACOBI_SWEEPS; sweep++) {
        const double off = fabs(a[0][1]) + fabs(a[0][2]) + fabs(a[1][2]);
        if (off < 1e-18) {
            return;
        }
        for (int p = 0; p < 2; p++) {
            for (int q = p + 1; q < 3; q++) {
                if (fabs(a[p][q]) < 1e-30) {
                    continue;
                }
                const double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                const double t =
                    (theta >= 0.0 ? 1.0 : -1.0) / (fabs(theta) + sqrt((theta * theta) + 1.0));
                const double c = 1.0 / sqrt((t * t) + 1.0);
                const double s = t * c;
                for (int k = 0; k < 3; k++) { /* colonnes p et q de a, puis lignes */
                    const double akp = a[k][p];
                    const double akq = a[k][q];
                    a[k][p] = (c * akp) - (s * akq);
                    a[k][q] = (s * akp) + (c * akq);
                }
                for (int k = 0; k < 3; k++) {
                    const double apk = a[p][k];
                    const double aqk = a[q][k];
                    a[p][k] = (c * apk) - (s * aqk);
                    a[q][k] = (s * apk) + (c * aqk);
                }
                for (int k = 0; k < 3; k++) {
                    const double vkp = v[k][p];
                    const double vkq = v[k][q];
                    v[k][p] = (c * vkp) - (s * vkq);
                    v[k][q] = (s * vkp) + (c * vkq);
                }
            }
        }
    }
}

/* le point i (trois flottants) d'un tableau dont les points sont espacés de `stride` octets */
static void point_at(const float *points, size_t stride, uint32_t i, float out[3])
{
    memcpy(out, (const char *)points + ((size_t)i * stride), 3U * sizeof(float));
}

bool eng_bake_fit(const float *points, size_t stride, uint32_t count, EngBakeFrame *out)
{
    if (points == NULL || out == NULL || count == 0U || stride < 3U * sizeof(float)) {
        return false;
    }
    double mean[3] = {0.0, 0.0, 0.0};
    for (uint32_t i = 0; i < count; i++) {
        float p[3];
        point_at(points, stride, i, p);
        if (!isfinite(p[0]) || !isfinite(p[1]) || !isfinite(p[2])) {
            return false;
        }
        for (int a = 0; a < 3; a++) {
            mean[a] += (double)p[a];
        }
    }
    for (int a = 0; a < 3; a++) {
        mean[a] /= (double)count;
    }
    double cov[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
    for (uint32_t i = 0; i < count; i++) {
        float p[3];
        point_at(points, stride, i, p);
        const double d[3] = {(double)p[0] - mean[0], (double)p[1] - mean[1],
                             (double)p[2] - mean[2]};
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                cov[r][c] += d[r] * d[c];
            }
        }
    }
    double vectors[3][3];
    jacobi(cov, vectors);
    /* axes rangés par étendue décroissante (valeur propre) ; le troisième ferme une base directe */
    int order[3] = {0, 1, 2};
    for (int i = 0; i < 2; i++) {
        for (int j = i + 1; j < 3; j++) {
            if (cov[order[j]][order[j]] > cov[order[i]][order[i]]) {
                const int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    }
    double axis[3][3];
    for (int i = 0; i < 2; i++) {
        double length = 0.0;
        for (int a = 0; a < 3; a++) {
            axis[i][a] = vectors[a][order[i]];
            length += axis[i][a] * axis[i][a];
        }
        length = sqrt(length);
        for (int a = 0; a < 3; a++) {
            axis[i][a] = length > 0.0 ? axis[i][a] / length : (a == i ? 1.0 : 0.0);
        }
    }
    axis[2][0] = (axis[0][1] * axis[1][2]) - (axis[0][2] * axis[1][1]);
    axis[2][1] = (axis[0][2] * axis[1][0]) - (axis[0][0] * axis[1][2]);
    axis[2][2] = (axis[0][0] * axis[1][1]) - (axis[0][1] * axis[1][0]);
    double low[3] = {0.0, 0.0, 0.0};
    double high[3] = {0.0, 0.0, 0.0};
    for (uint32_t i = 0; i < count; i++) {
        float p[3];
        point_at(points, stride, i, p);
        for (int k = 0; k < 3; k++) {
            const double along = (((double)p[0] - mean[0]) * axis[k][0]) +
                                 (((double)p[1] - mean[1]) * axis[k][1]) +
                                 (((double)p[2] - mean[2]) * axis[k][2]);
            low[k] = i == 0U || along < low[k] ? along : low[k];
            high[k] = i == 0U || along > high[k] ? along : high[k];
        }
    }
    for (int a = 0; a < 3; a++) {
        double center = mean[a];
        for (int k = 0; k < 3; k++) {
            center += axis[k][a] * 0.5 * (low[k] + high[k]);
        }
        out->center[a] = (float)center;
    }
    for (int k = 0; k < 3; k++) {
        const float half = (float)(0.5 * (high[k] - low[k]));
        out->half[k] = half > HALF_MIN ? half : HALF_MIN;
        for (int a = 0; a < 3; a++) {
            out->axis[k][a] = (float)axis[k][a];
        }
    }
    return true;
}
