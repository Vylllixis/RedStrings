/* Arbre scanné -> arbre de jeu : rangement des photographies dans l'atlas, prolongement de leurs
 * bords dans le vide, tassement des sommets d'un maillage. Voir engine/tree_bake.h. */
#include <stddef.h>

#include "engine/tree_bake.h"

/* ── Rangement par étagères ── */

bool eng_bake_pack(EngBakeRect *rects, uint32_t count, uint32_t width, uint32_t height,
                   uint32_t gap, uint32_t *order)
{
    if (rects == NULL || order == NULL || count == 0U) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (rects[i].w == 0U || rects[i].h == 0U) {
            return false;
        }
        order[i] = i;
    }
    /* du plus haut au plus bas (tri par insertion : stable, donc déterministe) */
    for (uint32_t i = 1; i < count; i++) {
        const uint32_t item = order[i];
        uint32_t j = i;
        while (j > 0U && rects[order[j - 1U]].h < rects[item].h) {
            order[j] = order[j - 1U];
            j--;
        }
        order[j] = item;
    }
    uint32_t x = gap;
    uint32_t y = gap;
    uint32_t shelf = 0;
    for (uint32_t i = 0; i < count; i++) {
        EngBakeRect *r = &rects[order[i]];
        if ((uint64_t)r->w + (2ULL * gap) > width) {
            return false;
        }
        if ((uint64_t)x + r->w + gap > width) { /* étagère pleine : la suivante, dessous */
            x = gap;
            y += shelf + gap;
            shelf = 0;
        }
        if ((uint64_t)y + r->h + gap > height) {
            return false;
        }
        r->x = x;
        r->y = y;
        x += r->w + gap;
        shelf = r->h > shelf ? r->h : shelf;
    }
    return true;
}

/* ── Prolongement des bords dans le vide ── */

static void dilate_pixel(const EngBakeTarget *t, const uint8_t *work, uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h, uint32_t px, uint32_t py, uint8_t *filled)
{
    uint32_t sum[3][3] = {{0U, 0U, 0U}, {0U, 0U, 0U}, {0U, 0U, 0U}};
    uint32_t found = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            const int nx = (int)px + dx;
            const int ny = (int)py + dy;
            if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 || nx >= (int)w || ny >= (int)h ||
                work[((size_t)ny * w) + (size_t)nx] != 1U) {
                continue;
            }
            const size_t at = ((((size_t)y + (size_t)ny) * t->width) + x + (size_t)nx) * 4U;
            for (int c = 0; c < 3; c++) {
                sum[0][c] += t->color[at + (size_t)c];
                sum[1][c] += t->normal[at + (size_t)c];
                sum[2][c] += t->arm[at + (size_t)c];
            }
            found++;
        }
    }
    if (found == 0U) {
        return;
    }
    const size_t at = ((((size_t)y + py) * t->width) + x + px) * 4U;
    for (int c = 0; c < 3; c++) {
        t->color[at + (size_t)c] = (uint8_t)(sum[0][c] / found);
        t->normal[at + (size_t)c] = (uint8_t)(sum[1][c] / found);
        t->arm[at + (size_t)c] = (uint8_t)(sum[2][c] / found);
    }
    *filled = 2U; /* rempli pendant cette passe : ne sert de source qu'à la suivante */
}

void eng_bake_dilate(const EngBakeTarget *target, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                     uint32_t passes, uint8_t *work)
{
    if (target == NULL || work == NULL || target->color == NULL || target->normal == NULL ||
        target->arm == NULL || w == 0U || h == 0U || (uint64_t)x + w > target->width ||
        (uint64_t)y + h > target->height) {
        return;
    }
    for (uint32_t py = 0; py < h; py++) {
        for (uint32_t px = 0; px < w; px++) {
            const size_t at = ((((size_t)y + py) * target->width) + x + px) * 4U;
            work[((size_t)py * w) + px] = target->color[at + 3U] > 0U ? 1U : 0U;
        }
    }
    for (uint32_t pass = 0; pass < passes; pass++) {
        uint32_t grown = 0;
        for (uint32_t py = 0; py < h; py++) {
            for (uint32_t px = 0; px < w; px++) {
                uint8_t *cell = &work[((size_t)py * w) + px];
                if (*cell == 0U) {
                    dilate_pixel(target, work, x, y, w, h, px, py, cell);
                    grown += *cell == 2U ? 1U : 0U;
                }
            }
        }
        if (grown == 0U) {
            return;
        }
        for (size_t i = 0; i < (size_t)w * h; i++) {
            work[i] = work[i] == 2U ? 1U : work[i];
        }
    }
}

/* ── Sommets inutilisés ── */

uint32_t eng_bake_compact(EngBakeVertex *vertices, uint32_t vertex_count, uint32_t *indices,
                          uint32_t triangle_count, uint32_t *remap)
{
    if (vertices == NULL || indices == NULL || remap == NULL) {
        return 0;
    }
    for (uint32_t v = 0; v < vertex_count; v++) {
        remap[v] = UINT32_MAX;
    }
    for (size_t i = 0; i < (size_t)triangle_count * 3U; i++) {
        if (indices[i] < vertex_count) {
            remap[indices[i]] = 0U; /* utilisé */
        }
    }
    uint32_t kept = 0;
    for (uint32_t v = 0; v < vertex_count; v++) {
        if (remap[v] != UINT32_MAX) {
            vertices[kept] = vertices[v];
            remap[v] = kept++;
        }
    }
    for (size_t i = 0; i < (size_t)triangle_count * 3U; i++) {
        indices[i] = indices[i] < vertex_count ? remap[indices[i]] : 0U;
    }
    return kept;
}
