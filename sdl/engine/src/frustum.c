#include "engine/frustum.h"

#include <math.h>
#include <string.h>

/* Plan = combinaison des lignes de la matrice : ligne i = (m[0][i], m[1][i], m[2][i], m[3][i]). */
static void row_of(mat4 m, int i, float out[4])
{
    for (int c = 0; c < 4; c++) {
        out[c] = m[c][i];
    }
}

static bool normalize_plane(float plane[4])
{
    const float len = sqrtf((plane[0] * plane[0]) + (plane[1] * plane[1]) + (plane[2] * plane[2]));
    if (!(len > 1e-12F) || !isfinite(len)) {
        return false;
    }
    for (int c = 0; c < 4; c++) {
        plane[c] /= len;
    }
    return true;
}

/* Pyramide invalide : chaque plan vaut -1 partout (0·p - 1 < 0), donc tout est rejeté. */
static bool reject_everything(EngFrustum *out)
{
    memset(out, 0, sizeof *out);
    for (int p = 0; p < 6; p++) {
        out->planes[p][3] = -1.0F;
    }
    return false;
}

bool eng_frustum_from_view_proj(EngFrustum *out, mat4 view_proj)
{
    float r[4][4];
    for (int i = 0; i < 4; i++) {
        row_of(view_proj, i, r[i]);
        for (int c = 0; c < 4; c++) {
            if (!isfinite(r[i][c])) {
                return reject_everything(out);
            }
        }
    }
    /* Découpage : -w <= x <= w, -w <= y <= w, 0 <= z <= w (profondeur [0, 1]). */
    EngFrustum f;
    for (int c = 0; c < 4; c++) {
        f.planes[0][c] = r[3][c] + r[0][c]; /* gauche : w + x >= 0 */
        f.planes[1][c] = r[3][c] - r[0][c]; /* droite : w - x >= 0 */
        f.planes[2][c] = r[3][c] + r[1][c]; /* bas    : w + y >= 0 */
        f.planes[3][c] = r[3][c] - r[1][c]; /* haut   : w - y >= 0 */
        f.planes[4][c] = r[2][c];           /* proche : z >= 0 */
        f.planes[5][c] = r[3][c] - r[2][c]; /* loin   : w - z >= 0 */
    }
    for (int p = 0; p < 6; p++) {
        if (!normalize_plane(f.planes[p])) {
            return reject_everything(out);
        }
    }
    *out = f;
    return true;
}

static float plane_distance(const float plane[4], const float p[3])
{
    return (plane[0] * p[0]) + (plane[1] * p[1]) + (plane[2] * p[2]) + plane[3];
}

bool eng_frustum_contains_point(const EngFrustum *frustum, const float point[3])
{
    for (int p = 0; p < 6; p++) {
        if (!(plane_distance(frustum->planes[p], point) >= 0.0F)) {
            return false;
        }
    }
    return true;
}

EngFrustumResult eng_frustum_classify_aabb(const EngFrustum *frustum, const float min[3],
                                           const float max[3])
{
    for (int a = 0; a < 3; a++) {
        if (!isfinite(min[a]) || !isfinite(max[a]) || min[a] > max[a]) {
            return ENG_FRUSTUM_OUTSIDE;
        }
    }
    bool all_inside = true;
    for (int p = 0; p < 6; p++) {
        const float *plane = frustum->planes[p];
        /* sommet le plus avancé (vers l'intérieur) et le moins avancé, selon la normale */
        float best[3];
        float worst[3];
        for (int a = 0; a < 3; a++) {
            best[a] = plane[a] >= 0.0F ? max[a] : min[a];
            worst[a] = plane[a] >= 0.0F ? min[a] : max[a];
        }
        if (plane_distance(plane, best) < 0.0F) {
            return ENG_FRUSTUM_OUTSIDE; /* même le coin le plus intérieur est dehors */
        }
        if (plane_distance(plane, worst) < 0.0F) {
            all_inside = false;
        }
    }
    return all_inside ? ENG_FRUSTUM_INSIDE : ENG_FRUSTUM_INTERSECT;
}

bool eng_frustum_intersects_aabb(const EngFrustum *frustum, const float min[3], const float max[3])
{
    return eng_frustum_classify_aabb(frustum, min, max) != ENG_FRUSTUM_OUTSIDE;
}

uint32_t eng_chunks_visible(const EngTerrainChunk *chunks, uint32_t chunk_count,
                            const EngFrustum *frustum, uint32_t *out, uint32_t capacity)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < chunk_count && count < capacity; i++) {
        if (eng_frustum_intersects_aabb(frustum, chunks[i].bounds_min, chunks[i].bounds_max)) {
            out[count++] = i;
        }
    }
    return count;
}

uint32_t eng_terrain_visible_chunks(const EngTerrain *terrain, const EngFrustum *frustum,
                                    uint32_t *out, uint32_t capacity)
{
    return eng_chunks_visible(terrain->chunks, terrain->chunks_per_side * terrain->chunks_per_side,
                              frustum, out, capacity);
}
