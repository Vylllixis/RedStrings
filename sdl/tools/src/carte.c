/* carte : aperçu vu de dessus de la carte du monde (engine/world_map.h).
 *
 *   carte <carte.txt> <apercu.ppm> [taille]
 *
 * Lit la description, construit le relief et les régions, écrit un PPM (taille 1024 par défaut) et
 * (avec x0 z0 pas n) une grille d'altitudes, puis
 * affiche les altitudes et le temps de construction. Code de retour : 0 réussite, 1 échec, 2
 * mauvais usage. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "engine/world_map.h"

static EngWorldMap g_map; /* ~20 Kio : hors pile */

static char *read_all(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    char *buf = NULL;
    if (fseek(f, 0, SEEK_END) == 0) {
        const long n = ftell(f);
        if (n >= 0 && fseek(f, 0, SEEK_SET) == 0) {
            buf = malloc((size_t)n + 1U);
            if (buf != NULL && fread(buf, 1, (size_t)n, f) == (size_t)n) {
                buf[n] = '\0';
                *len = (size_t)n;
            } else {
                free(buf);
                buf = NULL;
            }
        }
    }
    (void)fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 3 || (argc > 4 && argc != 8)) {
        fprintf(stderr, "usage : carte <carte.txt> <apercu.ppm> [taille [x0 z0 pas n]]\n");
        return 2;
    }
    const long size = argc == 4 ? strtol(argv[3], NULL, 10) : 1024;
    if (size < 64 || size > 4096) {
        fprintf(stderr, "carte : taille de 64 à 4096\n");
        return 2;
    }
    size_t len = 0;
    char *text = read_all(argv[1], &len);
    if (text == NULL) {
        fprintf(stderr, "carte : lecture de %s impossible\n", argv[1]);
        return 1;
    }
    char err[200];
    const bool parsed = eng_worldmap_parse(&g_map, text, len, err, sizeof err);
    free(text);
    if (!parsed) {
        fprintf(stderr, "carte : %s : %s\n", argv[1], err);
        return 1;
    }
    EngArena arena;
    const size_t bytes =
        eng_worldmap_build_bytes(&g_map) + ((size_t)size * (size_t)size * 3U) + 4096U;
    if (!eng_arena_init(&arena, "carte", bytes)) {
        return 1;
    }
    const clock_t t0 = clock();
    if (!eng_worldmap_build(&g_map, &arena)) {
        fprintf(stderr, "carte : construction impossible\n");
        eng_arena_shutdown(&arena);
        return 1;
    }
    const double build_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
    uint8_t *rgb = eng_arena_alloc_array(&arena, 3U, (size_t)size * (size_t)size, 1U);
    int status = 1;
    FILE *out = rgb != NULL ? fopen(argv[2], "wb") : NULL;
    if (out != NULL && eng_worldmap_render(&g_map, (uint32_t)size, rgb)) {
        (void)fprintf(out, "P6\n%ld %ld\n255\n", size, size);
        status = fwrite(rgb, 3U, (size_t)size * (size_t)size, out) == (size_t)size * (size_t)size
                     ? 0
                     : 1;
    }
    if (out != NULL) {
        (void)fclose(out);
    }
    printf("carte : %.0f m de côté, %u x %u échantillons, %u formes, %u régions ; altitudes %.1f à "
           "%.1f m ; construite en %.0f ms -> %s\n",
           (double)g_map.side, g_map.size, g_map.size, g_map.feature_count, g_map.region_count,
           (double)g_map.height_min, (double)g_map.height_max, build_ms, argv[2]);
    static EngMapTree trees[16384];
    uint32_t by_kind[ENG_TREE_KINDS] = {0};
    const int blocks = (int)ceil(g_map.side / 256.0);
    for (int jz = 0; jz < blocks; jz++) {
        for (int jx = 0; jx < blocks; jx++) {
            const double bx = (-0.5 * g_map.side) + (256.0 * jx);
            const double bz = (-0.5 * g_map.side) + (256.0 * jz);
            const uint32_t n = eng_worldmap_trees(&g_map, bx, bz, 256.0, trees, 16384U);
            for (uint32_t t = 0; t < n; t++) {
                by_kind[trees[t].kind]++;
            }
        }
    }
    printf("  arbres : %u chênes, %u sapins enneigés, %u acacias\n", by_kind[ENG_TREE_OAK],
           by_kind[ENG_TREE_PINE], by_kind[ENG_TREE_ACACIA]);
    for (uint32_t k = 0; k < g_map.feature_count; k++) { /* pente la plus forte de chaque chemin */
        const EngMapFeature *f = &g_map.features[k];
        if (f->kind != (uint8_t)ENG_MAPF_PATH) {
            continue;
        }
        float worst = 0.0F;
        uint32_t at = 0;
        for (uint32_t p = 0; p + 1U < f->point_count; p++) {
            const float *a = g_map.points[f->first_point + p];
            const float *b = g_map.points[f->first_point + p + 1U];
            const float dh =
                eng_worldmap_height(&g_map, b[0], b[1]) - eng_worldmap_height(&g_map, a[0], a[1]);
            const float slope = fabsf(dh) / hypotf(b[0] - a[0], b[1] - a[1]);
            if (slope > worst) {
                worst = slope;
                at = p;
            }
        }
        const float *a = g_map.points[f->first_point + at];
        printf("  chemin %u : pente max %.0f %% (%.0f degrés), segment %u depuis (%.0f, %.0f)\n", k,
               (double)(100.0F * worst), (double)(atanf(worst) * 57.29578F), at, (double)a[0],
               (double)a[1]);
    }
    for (uint32_t k = 0; k < ENG_MAPZONE_COUNT; k++) {
        const EngMapZoneSpot *z = &g_map.zones[k];
        if (z->used) {
            printf("  zone %2u : (%5.0f, %5.0f) à %5.1f m\n", k, (double)z->x, (double)z->z,
                   (double)eng_worldmap_height(&g_map, z->x, z->z));
        }
    }
    printf("  départ : %.1f m ; sol à 150 m du départ : %.1f / %.1f / %.1f / %.1f m\n",
           (double)eng_worldmap_height(&g_map, g_map.spawn[0], g_map.spawn[1]),
           (double)eng_worldmap_height(&g_map, 150.0, 0.0),
           (double)eng_worldmap_height(&g_map, -150.0, 0.0),
           (double)eng_worldmap_height(&g_map, 0.0, 150.0),
           (double)eng_worldmap_height(&g_map, 0.0, -150.0));
    if (argc == 8) { /* grille d'altitudes n x n autour d'un point, pour dessiner les chemins */
        const float x0 = strtof(argv[4], NULL);
        const float z0 = strtof(argv[5], NULL);
        const float step = strtof(argv[6], NULL);
        const int n = (int)strtol(argv[7], NULL, 10);
        printf("  z \\ x");
        for (int i = 0; i < n; i++) {
            printf(" %6.0f", (double)(x0 + ((float)i * step)));
        }
        printf("\n");
        for (int j = 0; j < n; j++) {
            const float z = z0 + ((float)j * step);
            printf("  %6.0f", (double)z);
            for (int i = 0; i < n; i++) {
                printf(" %6.0f", (double)eng_worldmap_height(&g_map, x0 + ((float)i * step), z));
            }
            printf("\n");
        }
    }
    eng_arena_shutdown(&arena);
    return status;
}
