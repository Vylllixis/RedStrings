#include <stddef.h>

#include "engine/world_map.h"

/* Meutes de petits monstres : une zone de genre ENG_MAPZONE_PACK par ligne « meute x z », avec les
 * ennemis du biome qui domine à cet endroit (la carte ne nomme pas le biome : déplacer une meute
 * dans le fichier suffit). */
uint32_t eng_worldmap_packs(const EngWorldMap *map, EngMapZone *out, uint32_t max)
{
    if (map == NULL || out == NULL || map->heights == NULL || map->weights == NULL) {
        return 0U;
    }
    uint32_t n = 0;
    for (uint32_t i = 0; i < map->pack_count && i < ENG_WORLDMAP_MAX_PACKS && n < max; i++) {
        const double x = (double)map->packs[i][0];
        const double z = (double)map->packs[i][1];
        float w[ENG_BIOME_COUNT] = {0};
        eng_worldmap_weights(map, x, z, w, NULL);
        int best = 0;
        for (int b = 1; b < ENG_BIOME_COUNT; b++) {
            if (w[b] > w[best]) {
                best = b;
            }
        }
        if (eng_mapzones_pack((EngBiome)best, x, z, &out[n])) {
            n++;
        }
    }
    return n;
}
