#ifndef LAUNCHER_WORLD_FAR_H
#define LAUNCHER_WORLD_FAR_H

/* Vue lointaine (carte du monde, 2026-11-21) : toute la carte en basse résolution (un sommet tous
 * les 32 m), dessinée là où le streaming n'a pas chargé de cellule. Depuis la colline du château,
 * on voit ainsi les montagnes, le désert et le volcan au loin. Découpée en morceaux de 256 m, un
 * par cellule du monde : un morceau n'est pas dessiné si sa cellule détaillée l'est. Abaissée de
 * 1,5 m pour que les jupes des cellules détaillées cachent les raccords. */

#include <cglm/cglm.h>
#include <stdbool.h>
#include <stdint.h>

#include "engine/frustum.h"
#include "engine/model_gpu.h"
#include "engine/terrain_gpu.h"
#include "engine/world_cell.h"
#include "engine/world_map.h"
#include "engine/world_origin.h"

#define WORLD_FAR_SPACING 32.0F
#define WORLD_FAR_MAX_CHUNKS 256U /* 16 x 16 morceaux pour une carte de 4096 m */

typedef struct WorldFar {
    EngModelGpu model;
    EngTextureHandle splat;
    EngTextureHandle splat2;
    float chunk_min[WORLD_FAR_MAX_CHUNKS][3]; /* boîtes des morceaux, repère de la carte */
    float chunk_max[WORLD_FAR_MAX_CHUNKS][3];
    EngCellCoord chunk_cell[WORLD_FAR_MAX_CHUNKS];
    double corner[3]; /* coin nord-ouest de la carte, monde */
    uint32_t chunk_count;
    uint32_t drawn; /* dernière image : morceaux dessinés */
    bool ready;
} WorldFar;

/* Construit le maillage lointain de `map` (tableaux GPU dans `arena`, travail dans `scratch`). */
bool world_far_init(WorldFar *f, EngRenderer *renderer, EngArena *arena, EngArena *scratch,
                    const EngWorldMap *map, const EngEnvironment *env);
void world_far_shutdown(WorldFar *f, EngRenderer *renderer);
/* Dessine les morceaux visibles dont `skip` (indexé par morceau) est faux. */
void world_far_draw(WorldFar *f, EngRenderQueue *queue, EngPipelineHandle pipeline,
                    const EngTerrainGpu *shared, const EngWorldOrigin *origin, mat4 view_proj,
                    const EngLighting *light, const EngModelShadow *shadow,
                    const EngFrustum *frustum, const bool *skip);

#endif
