/* Arbre scanné -> arbre de jeu : ombre du feuillage sur lui-même. Une grille de densité, puis la
 * part du ciel vue depuis chaque case (rayons marchés dans la grille). Voir engine/tree_bake.h. */
#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#include "engine/tree_bake.h"

#define DIRECTIONS 48U
#define RESOLUTION_MIN 4U
#define RESOLUTION_MAX 96U
#define DEPTH_MAX 8.0F /* épaisseur optique au-delà de laquelle un rayon est éteint */
#define SKY_DOWN 0.25F /* poids d'une direction vers le sol (1 vers le zénith) */

static size_t cell_index(const EngBakeGrid *g, uint32_t x, uint32_t y, uint32_t z)
{
    return (((size_t)z * g->size[1]) + y) * g->size[0] + x;
}

/* direction i d'une sphère de Fibonacci : DIRECTIONS directions réparties uniformément */
static void direction(uint32_t i, float out[3])
{
    const float golden = 2.39996323F; /* angle d'or (rad) */
    const float y = 1.0F - ((2.0F * ((float)i + 0.5F)) / (float)DIRECTIONS);
    const float r = sqrtf(1.0F - (y * y));
    const float angle = golden * (float)i;
    out[0] = r * cosf(angle);
    out[1] = y;
    out[2] = r * sinf(angle);
}

/* lumière gardée par un rayon parti du centre de la case (x, y, z) dans la direction d */
static float transmittance(const EngBakeGrid *g, uint32_t x, uint32_t y, uint32_t z,
                           const float d[3], float absorption)
{
    float p[3] = {(float)x + 0.5F, (float)y + 0.5F, (float)z + 0.5F}; /* en cases */
    float depth = 0.0F;
    for (;;) {
        for (int a = 0; a < 3; a++) {
            p[a] += d[a];
        }
        if (p[0] < 0.0F || p[1] < 0.0F || p[2] < 0.0F || p[0] >= (float)g->size[0] ||
            p[1] >= (float)g->size[1] || p[2] >= (float)g->size[2]) {
            break; /* sorti de la grille : le ciel */
        }
        depth += absorption * g->cell *
                 g->density[cell_index(g, (uint32_t)p[0], (uint32_t)p[1], (uint32_t)p[2])];
        if (depth > DEPTH_MAX) {
            return 0.0F;
        }
    }
    return expf(-depth);
}

static void compute_sky(EngBakeGrid *g, float absorption)
{
    float dir[DIRECTIONS][3];
    float weight[DIRECTIONS];
    float total = 0.0F;
    for (uint32_t i = 0; i < DIRECTIONS; i++) {
        direction(i, dir[i]);
        weight[i] = SKY_DOWN + ((1.0F - SKY_DOWN) * (0.5F + (0.5F * dir[i][1])));
        total += weight[i];
    }
    for (uint32_t z = 0; z < g->size[2]; z++) {
        for (uint32_t y = 0; y < g->size[1]; y++) {
            for (uint32_t x = 0; x < g->size[0]; x++) {
                float sum = 0.0F;
                for (uint32_t i = 0; i < DIRECTIONS; i++) {
                    sum += weight[i] * transmittance(g, x, y, z, dir[i], absorption);
                }
                g->sky[cell_index(g, x, y, z)] = sum / total;
            }
        }
    }
}

bool eng_bake_grid_build(EngBakeGrid *grid, const float *points, const float *areas, uint32_t count,
                         uint32_t resolution, float absorption)
{
    if (grid == NULL) {
        return false;
    }
    memset(grid, 0, sizeof *grid);
    if (points == NULL || areas == NULL || count == 0U || resolution < RESOLUTION_MIN ||
        resolution > RESOLUTION_MAX || !isfinite(absorption) || absorption < 0.0F) {
        return false;
    }
    float low[3] = {points[0], points[1], points[2]};
    float high[3] = {points[0], points[1], points[2]};
    for (uint32_t i = 0; i < count; i++) {
        for (int a = 0; a < 3; a++) {
            const float v = points[((size_t)i * 3U) + (size_t)a];
            if (!isfinite(v)) {
                return false;
            }
            low[a] = v < low[a] ? v : low[a];
            high[a] = v > high[a] ? v : high[a];
        }
    }
    float extent = 0.0F;
    for (int a = 0; a < 3; a++) {
        extent = high[a] - low[a] > extent ? high[a] - low[a] : extent;
    }
    grid->cell = extent > 1e-6F ? extent / (float)resolution : 1.0F;
    size_t cells = 1;
    for (int a = 0; a < 3; a++) {
        /* une case de marge de chaque côté : le pourtour voit le ciel */
        grid->size[a] = (uint32_t)ceilf((high[a] - low[a]) / grid->cell) + 3U;
        grid->min[a] = low[a] - (1.5F * grid->cell);
        cells *= grid->size[a];
    }
    grid->density = SDL_calloc(cells, sizeof(float));
    grid->sky = SDL_calloc(cells, sizeof(float));
    if (grid->density == NULL || grid->sky == NULL) {
        eng_bake_grid_free(grid);
        return false;
    }
    const float volume = grid->cell * grid->cell * grid->cell;
    for (uint32_t i = 0; i < count; i++) {
        const float area = areas[i];
        if (!isfinite(area) || area <= 0.0F) {
            continue;
        }
        uint32_t c[3];
        for (int a = 0; a < 3; a++) {
            const float at = (points[((size_t)i * 3U) + (size_t)a] - grid->min[a]) / grid->cell;
            c[a] = at <= 0.0F ? 0U
                              : ((uint32_t)at >= grid->size[a] ? grid->size[a] - 1U : (uint32_t)at);
        }
        grid->density[cell_index(grid, c[0], c[1], c[2])] += area / volume;
    }
    compute_sky(grid, absorption);
    return true;
}

float eng_bake_grid_sky(const EngBakeGrid *grid, const float position[3])
{
    if (grid == NULL || grid->sky == NULL || position == NULL) {
        return 1.0F;
    }
    uint32_t base[3];
    float t[3];
    for (int a = 0; a < 3; a++) {
        /* les valeurs sont aux centres des cases */
        float at = ((position[a] - grid->min[a]) / grid->cell) - 0.5F;
        const float last = (float)(grid->size[a] - 1U);
        at = isfinite(at) ? (at < 0.0F ? 0.0F : (at > last ? last : at)) : 0.0F;
        base[a] = (uint32_t)at;
        if (base[a] >= grid->size[a] - 1U) {
            base[a] = grid->size[a] > 1U ? grid->size[a] - 2U : 0U;
        }
        t[a] = at - (float)base[a];
    }
    float value = 0.0F;
    for (uint32_t corner = 0; corner < 8U; corner++) {
        uint32_t c[3];
        float w = 1.0F;
        for (uint32_t a = 0; a < 3U; a++) {
            const uint32_t next = (corner >> a) & 1U;
            c[a] = base[a] + next < grid->size[a] ? base[a] + next : grid->size[a] - 1U;
            w *= next != 0U ? t[a] : 1.0F - t[a];
        }
        value += w * grid->sky[cell_index(grid, c[0], c[1], c[2])];
    }
    return value;
}

void eng_bake_grid_free(EngBakeGrid *grid)
{
    if (grid == NULL) {
        return;
    }
    SDL_free(grid->density);
    SDL_free(grid->sky);
    memset(grid, 0, sizeof *grid);
}
