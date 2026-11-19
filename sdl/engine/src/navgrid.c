#include "engine/navgrid.h"

#include <math.h>
#include <string.h>

enum { ST_NONE = 0, ST_OPEN = 1, ST_CLOSED = 2 };

bool eng_nav_build(EngNavGrid *nav, const EngAiWorld *world, const float min[2], const float max[2],
                   float cell, float agent_radius)
{
    if (nav == NULL || min == NULL || max == NULL || !isfinite(cell) || cell <= 0.0F ||
        !isfinite(agent_radius) || agent_radius < 0.0F || !(max[0] > min[0]) ||
        !(max[1] > min[1])) {
        return false;
    }
    const float fw = ceilf((max[0] - min[0]) / cell);
    const float fh = ceilf((max[1] - min[1]) / cell);
    if (!(fw >= 1.0F) || !(fh >= 1.0F) || fw > (float)ENG_NAV_MAX_DIM ||
        fh > (float)ENG_NAV_MAX_DIM) {
        return false;
    }
    memset(nav, 0, sizeof *nav);
    nav->origin[0] = min[0];
    nav->origin[1] = min[1];
    nav->cell = cell;
    nav->w = (uint32_t)fw;
    nav->h = (uint32_t)fh;
    for (uint32_t z = 0; z < nav->h; z++) {
        for (uint32_t x = 0; x < nav->w; x++) {
            const float c[2] = {min[0] + ((float)x + 0.5F) * cell,
                                min[1] + ((float)z + 0.5F) * cell};
            nav->blocked[(size_t)z * nav->w + x] =
                (eng_aiw_distance(world, c) < agent_radius) ? 1U : 0U;
        }
    }
    return true;
}

static bool to_cell(const EngNavGrid *nav, const float p[2], int32_t *cx, int32_t *cz)
{
    if (!isfinite(p[0]) || !isfinite(p[1])) {
        return false;
    }
    const float fx = floorf((p[0] - nav->origin[0]) / nav->cell);
    const float fz = floorf((p[1] - nav->origin[1]) / nav->cell);
    if (fx < 0.0F || fz < 0.0F || fx >= (float)nav->w || fz >= (float)nav->h) {
        return false;
    }
    *cx = (int32_t)fx;
    *cz = (int32_t)fz;
    return true;
}

static void cell_center(const EngNavGrid *nav, uint32_t idx, float out[2])
{
    const uint32_t x = idx % nav->w;
    const uint32_t z = idx / nav->w;
    out[0] = nav->origin[0] + ((float)x + 0.5F) * nav->cell;
    out[1] = nav->origin[1] + ((float)z + 0.5F) * nav->cell;
}

uint32_t eng_nav_block_steep(EngNavGrid *nav, EngNavHeightFn height, void *user,
                             float max_slope_deg)
{
    if (nav == NULL || height == NULL || !isfinite(max_slope_deg) || max_slope_deg <= 0.0F ||
        max_slope_deg >= 90.0F || nav->w == 0U || nav->h == 0U) {
        return 0;
    }
    const float max_grad = tanf(max_slope_deg * 0.01745329252F);
    const float h = 0.5F * nav->cell;
    uint32_t n = 0;
    for (uint32_t z = 0; z < nav->h; z++) {
        for (uint32_t x = 0; x < nav->w; x++) {
            const uint32_t i = (z * nav->w) + x;
            if (nav->blocked[i] != 0U) {
                continue;
            }
            const float cx = nav->origin[0] + (((float)x + 0.5F) * nav->cell);
            const float cz = nav->origin[1] + (((float)z + 0.5F) * nav->cell);
            const float gx = (height(user, cx + h, cz) - height(user, cx - h, cz)) / nav->cell;
            const float gz = (height(user, cx, cz + h) - height(user, cx, cz - h)) / nav->cell;
            const float grad = sqrtf((gx * gx) + (gz * gz));
            if (!isfinite(grad) || grad > max_grad) {
                nav->blocked[i] = 1U;
                n++;
            }
        }
    }
    return n;
}

uint32_t eng_nav_remove_islands(EngNavGrid *nav, uint32_t min_cells)
{
    if (nav == NULL || nav->w == 0U || min_cells < 2U) {
        return 0;
    }
    const uint32_t total = nav->w * nav->h;
    memset(nav->state, 0, total); /* 1 : déjà rangé dans une région */
    uint32_t blocked = 0;
    for (uint32_t seed = 0; seed < total; seed++) {
        if (nav->blocked[seed] != 0U || nav->state[seed] != 0U) {
            continue;
        }
        /* parcours en largeur : `scratch` reçoit toutes les cellules de la région, dans l'ordre */
        uint32_t head = 0;
        uint32_t tail = 0;
        nav->scratch[tail++] = seed;
        nav->state[seed] = 1U;
        while (head < tail) {
            const uint32_t cur = nav->scratch[head++];
            const uint32_t x = cur % nav->w;
            const uint32_t z = cur / nav->w;
            const uint32_t next[4] = {
                x > 0U ? cur - 1U : UINT32_MAX, x + 1U < nav->w ? cur + 1U : UINT32_MAX,
                z > 0U ? cur - nav->w : UINT32_MAX, z + 1U < nav->h ? cur + nav->w : UINT32_MAX};
            for (int k = 0; k < 4; k++) {
                const uint32_t n = next[k];
                if (n != UINT32_MAX && nav->blocked[n] == 0U && nav->state[n] == 0U) {
                    nav->state[n] = 1U;
                    nav->scratch[tail++] = n;
                }
            }
        }
        if (tail < min_cells) {
            for (uint32_t i = 0; i < tail; i++) {
                nav->blocked[nav->scratch[i]] = 1U;
            }
            blocked += tail;
        }
    }
    memset(nav->state, 0, total);
    return blocked;
}

bool eng_nav_walkable(const EngNavGrid *nav, const float p[2])
{
    int32_t cx = 0;
    int32_t cz = 0;
    if (nav == NULL || p == NULL || nav->w == 0U || !to_cell(nav, p, &cx, &cz)) {
        return false;
    }
    return nav->blocked[(size_t)cz * nav->w + (uint32_t)cx] == 0U;
}

bool eng_nav_line_free(const EngNavGrid *nav, const float a[2], const float b[2])
{
    if (nav == NULL || a == NULL || b == NULL) {
        return false;
    }
    const float dx = b[0] - a[0];
    const float dz = b[1] - a[1];
    const float len = sqrtf(dx * dx + dz * dz);
    const float step = nav->cell * 0.5F;
    const uint32_t n = (uint32_t)ceilf(len / step);
    for (uint32_t i = 0; i <= n; i++) {
        const float t = n == 0U ? 0.0F : (float)i / (float)n;
        const float p[2] = {a[0] + dx * t, a[1] + dz * t};
        if (!eng_nav_walkable(nav, p)) {
            return false;
        }
    }
    return true;
}

/* cellule praticable la plus proche (anneaux croissants), ou -1 */
static int32_t nearest_free(const EngNavGrid *nav, int32_t cx, int32_t cz)
{
    const int32_t maxr = 12;
    for (int32_t r = 0; r <= maxr; r++) {
        int64_t best = -1;
        int32_t best_d = 0x7FFFFFFF;
        for (int32_t dz = -r; dz <= r; dz++) {
            for (int32_t dx = -r; dx <= r; dx++) {
                if ((dx < 0 ? -dx : dx) != r && (dz < 0 ? -dz : dz) != r) {
                    continue;
                }
                const int32_t x = cx + dx;
                const int32_t z = cz + dz;
                if (x < 0 || z < 0 || x >= (int32_t)nav->w || z >= (int32_t)nav->h) {
                    continue;
                }
                const int64_t idx = (int64_t)z * nav->w + x;
                const int32_t d = dx * dx + dz * dz;
                if (nav->blocked[idx] == 0U && d < best_d) {
                    best_d = d;
                    best = idx;
                }
            }
        }
        if (best >= 0) {
            return (int32_t)best;
        }
    }
    return -1;
}

/* ---- tas binaire (clé f, départage par indice) avec décroissance de clé ---- */
static bool less(const EngNavGrid *n, uint32_t a, uint32_t b)
{
    if (n->f[a] != n->f[b]) {
        return n->f[a] < n->f[b];
    }
    return a < b;
}

static void sift_up(EngNavGrid *n, uint32_t i)
{
    while (i > 0U) {
        const uint32_t p = (i - 1U) / 2U;
        if (!less(n, n->heap[i], n->heap[p])) {
            break;
        }
        const uint32_t t = n->heap[i];
        n->heap[i] = n->heap[p];
        n->heap[p] = t;
        n->heap_pos[n->heap[i]] = (int32_t)i;
        n->heap_pos[n->heap[p]] = (int32_t)p;
        i = p;
    }
}

static void sift_down(EngNavGrid *n, uint32_t i)
{
    for (;;) {
        const uint32_t l = 2U * i + 1U;
        const uint32_t r = l + 1U;
        uint32_t m = i;
        if (l < n->heap_n && less(n, n->heap[l], n->heap[m])) {
            m = l;
        }
        if (r < n->heap_n && less(n, n->heap[r], n->heap[m])) {
            m = r;
        }
        if (m == i) {
            return;
        }
        const uint32_t t = n->heap[i];
        n->heap[i] = n->heap[m];
        n->heap[m] = t;
        n->heap_pos[n->heap[i]] = (int32_t)i;
        n->heap_pos[n->heap[m]] = (int32_t)m;
        i = m;
    }
}

static float octile(int32_t ax, int32_t az, int32_t bx, int32_t bz)
{
    const float dx = (float)(ax > bx ? ax - bx : bx - ax);
    const float dz = (float)(az > bz ? az - bz : bz - az);
    const float lo = dx < dz ? dx : dz;
    const float hi = dx < dz ? dz : dx;
    return hi + (1.41421356F - 1.0F) * lo;
}

bool eng_nav_nearest_walkable(const EngNavGrid *nav, const float p[2], float out[2])
{
    int32_t cx = 0;
    int32_t cz = 0;
    if (nav == NULL || p == NULL || out == NULL || !to_cell(nav, p, &cx, &cz)) {
        return false;
    }
    if (nav->blocked[((size_t)cz * nav->w) + (size_t)cx] == 0U) {
        out[0] = p[0];
        out[1] = p[1];
        return true;
    }
    const int32_t idx = nearest_free(nav, cx, cz);
    if (idx < 0) {
        return false;
    }
    cell_center(nav, (uint32_t)idx, out);
    return true;
}

uint32_t eng_nav_path(EngNavGrid *nav, const float from[2], const float to[2], float out[][2],
                      uint32_t max_points)
{
    if (nav == NULL || from == NULL || to == NULL || out == NULL || max_points == 0U ||
        nav->w == 0U) {
        return 0;
    }
    int32_t sx = 0;
    int32_t sz = 0;
    int32_t gx = 0;
    int32_t gz = 0;
    if (!to_cell(nav, from, &sx, &sz) || !to_cell(nav, to, &gx, &gz)) {
        return 0;
    }
    const int32_t s_idx = nearest_free(nav, sx, sz);
    const int32_t g_idx = nearest_free(nav, gx, gz);
    if (s_idx < 0 || g_idx < 0) {
        return 0;
    }
    sx = s_idx % (int32_t)nav->w;
    sz = s_idx / (int32_t)nav->w;
    gx = g_idx % (int32_t)nav->w;
    gz = g_idx / (int32_t)nav->w;
    const uint32_t total = nav->w * nav->h;
    memset(nav->state, 0, total);
    for (uint32_t i = 0; i < total; i++) {
        nav->g[i] = INFINITY;
        nav->parent[i] = -1;
        nav->heap_pos[i] = -1;
    }
    nav->heap_n = 0;
    nav->g[s_idx] = 0.0F;
    nav->f[s_idx] = octile(sx, sz, gx, gz);
    nav->state[s_idx] = ST_OPEN;
    nav->heap[nav->heap_n] = (uint32_t)s_idx;
    nav->heap_pos[s_idx] = 0;
    nav->heap_n = 1;
    bool found = false;
    static const int32_t dxs[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int32_t dzs[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (nav->heap_n > 0U) {
        const uint32_t cur = nav->heap[0];
        nav->heap_n--;
        if (nav->heap_n > 0U) {
            nav->heap[0] = nav->heap[nav->heap_n];
            nav->heap_pos[nav->heap[0]] = 0;
            sift_down(nav, 0);
        }
        nav->heap_pos[cur] = -1;
        nav->state[cur] = ST_CLOSED;
        if ((int32_t)cur == g_idx) {
            found = true;
            break;
        }
        const int32_t cx = (int32_t)(cur % nav->w);
        const int32_t cz = (int32_t)(cur / nav->w);
        for (int k = 0; k < 8; k++) {
            const int32_t nx = cx + dxs[k];
            const int32_t nz = cz + dzs[k];
            if (nx < 0 || nz < 0 || nx >= (int32_t)nav->w || nz >= (int32_t)nav->h) {
                continue;
            }
            const uint32_t ni = (uint32_t)nz * nav->w + (uint32_t)nx;
            if (nav->blocked[ni] != 0U || nav->state[ni] == ST_CLOSED) {
                continue;
            }
            if (k >= 4) { /* pas de coupe de coin */
                if (nav->blocked[(size_t)cz * nav->w + (uint32_t)nx] != 0U ||
                    nav->blocked[(size_t)nz * nav->w + (uint32_t)cx] != 0U) {
                    continue;
                }
            }
            const float ng = nav->g[cur] + (k >= 4 ? 1.41421356F : 1.0F);
            if (ng >= nav->g[ni]) {
                continue;
            }
            nav->g[ni] = ng;
            nav->f[ni] = ng + octile(nx, nz, gx, gz);
            nav->parent[ni] = (int32_t)cur;
            if (nav->state[ni] == ST_OPEN) {
                sift_up(nav, (uint32_t)nav->heap_pos[ni]);
            } else {
                nav->state[ni] = ST_OPEN;
                nav->heap[nav->heap_n] = ni;
                nav->heap_pos[ni] = (int32_t)nav->heap_n;
                nav->heap_n++;
                sift_up(nav, nav->heap_n - 1U);
            }
        }
    }
    if (!found) {
        return 0;
    }
    /* cellules du chemin, du but vers le départ, puis inversées */
    uint32_t n = 0;
    for (int32_t c = g_idx; c >= 0 && n < total; c = nav->parent[c]) {
        nav->scratch[n++] = (uint32_t)c;
    }
    for (uint32_t i = 0; i < n / 2U; i++) {
        const uint32_t t = nav->scratch[i];
        nav->scratch[i] = nav->scratch[n - 1U - i];
        nav->scratch[n - 1U - i] = t;
    }
    /* but réel si praticable, sinon centre de la cellule la plus proche */
    float goal[2] = {to[0], to[1]};
    if (!eng_nav_walkable(nav, to)) {
        cell_center(nav, (uint32_t)g_idx, goal);
    }
    /* lissage par « corde tendue » : on saute au point le plus lointain encore visible */
    float cur[2] = {from[0], from[1]};
    uint32_t emitted = 0;
    uint32_t i = 0;
    while (i < n) {
        uint32_t j = n - 1U;
        float target[2];
        for (;; j--) {
            if (j == n - 1U) {
                target[0] = goal[0];
                target[1] = goal[1];
            } else {
                cell_center(nav, nav->scratch[j], target);
            }
            if (j == i || eng_nav_line_free(nav, cur, target)) {
                break;
            }
        }
        if (emitted >= max_points) {
            out[max_points - 1U][0] = goal[0];
            out[max_points - 1U][1] = goal[1];
            return max_points;
        }
        out[emitted][0] = target[0];
        out[emitted][1] = target[1];
        emitted++;
        cur[0] = target[0];
        cur[1] = target[1];
        i = j + 1U;
    }
    return emitted;
}

uint32_t eng_nav_draw(const EngNavGrid *nav, EngDebugLines *lines, float y)
{
    if (nav == NULL || lines == NULL) {
        return 0;
    }
    uint32_t n = 0;
    const float h = nav->cell * 0.5F;
    for (uint32_t z = 0; z < nav->h; z++) {
        for (uint32_t x = 0; x < nav->w; x++) {
            if (nav->blocked[(size_t)z * nav->w + x] == 0U) {
                continue;
            }
            float c[2];
            cell_center(nav, z * nav->w + x, c);
            const vec3 a = {c[0] - h, y, c[1] - h};
            const vec3 b = {c[0] + h, y, c[1] + h};
            const vec3 d = {c[0] - h, y, c[1] + h};
            const vec3 e = {c[0] + h, y, c[1] - h};
            if (eng_debug_line(lines, a, b, ENG_DEBUG_RED) &&
                eng_debug_line(lines, d, e, ENG_DEBUG_RED)) {
                n += 2U;
            }
        }
    }
    return n;
}
