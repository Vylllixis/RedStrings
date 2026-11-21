#ifndef LAUNCHER_WORLD_TREES_H
#define LAUNCHER_WORLD_TREES_H

/* Arbres de la carte du monde dans le monde ouvert (2026-11-21) : chênes, sapins enneigés et
 * acacias (assets/props/nature/oak, pine_snow, acacia), placés par la carte
 * (eng_worldmap_trees). Une liste d'instances par cellule chargée (coordonnées LOCALES : décalées
 * à chaque recentrage de l'origine), dessinées en un dessin instancié par essence et par cellule,
 * dans l'image et dans l'ombre. Au-delà de 220 m, une version simplifiée (28 à 64 triangles) ;
 * seuls les arbres à moins de 200 m projettent une ombre. Les troncs à moins de 40 m du joueur sont
 * des capsules solides. */

#include <cglm/cglm.h>
#include <stdbool.h>
#include <stdint.h>

#include "engine/frustum.h"
#include "engine/instancing.h"
#include "engine/physics.h"
#include "engine/world_map.h"
#include "engine/world_origin.h"
#include "engine/world_stream.h"

#define WORLD_TREES_PER_CELL 1400U
#define WORLD_TREE_BODIES 320U

typedef struct WorldTreeSlot {
    EngInstance inst[WORLD_TREES_PER_CELL]; /* rangées par essence */
    float trunk[WORLD_TREES_PER_CELL];      /* rayon du tronc (m) */
    EngBufferHandle buffer[ENG_TREE_KINDS];
    uint32_t first[ENG_TREE_KINDS];
    uint32_t count[ENG_TREE_KINDS];
    float bmin[3]; /* boîte englobante locale */
    float bmax[3];
    EngCellCoord coord;
    uint32_t total;
    bool used;
} WorldTreeSlot;

typedef struct WorldTrees {
    EngModelGpu models[ENG_TREE_KINDS];
    EngModelGpu lods[ENG_TREE_KINDS]; /* versions simplifiées, au loin */
    float camera[3];                  /* dernière image : position de la caméra (locale) */
    EngPipelineHandle pipeline;
    EngPipelineHandle shadow_pipeline;
    EngRenderer *renderer;
    EngPhysics *physics;
    WorldTreeSlot slots[ENG_STREAM_SLOTS];
    EngBody bodies[WORLD_TREE_BODIES];
    uint32_t body_count;
    float body_center[2]; /* où le joueur était quand les troncs solides ont été choisis */
    uint32_t drawn;       /* dernière image : arbres dessinés */
    uint32_t loaded;      /* arbres des cellules chargées */
    bool has_center;
    bool ready;
} WorldTrees;

/* Charge les modèles et crée les pipelines. false (journalisé) : le monde reste sans arbres. */
bool world_trees_init(WorldTrees *t, EngRenderer *renderer, EngPhysics *physics,
                      const EngEnvironment *env);
void world_trees_shutdown(WorldTrees *t);
/* Instances de la cellule `coord` (emplacement `slot`), en local par `origin`. */
void world_trees_load_cell(WorldTrees *t, uint32_t slot, const EngWorldMap *map, EngCellCoord coord,
                           const EngWorldOrigin *origin);
void world_trees_unload_cell(WorldTrees *t, uint32_t slot);
/* Recentrage de l'origine : toutes les instances (et les troncs solides) se décalent de `shift`. */
void world_trees_shift(WorldTrees *t, const float shift[3]);
/* Troncs solides autour du joueur (`feet` local) : rechoisis quand il a bougé de plus de 8 m. */
void world_trees_update_bodies(WorldTrees *t, const float feet[3]);
void world_trees_draw(WorldTrees *t, EngRenderQueue *queue, mat4 view_proj,
                      const EngLighting *light, const EngModelShadow *shadow,
                      const EngFrustum *frustum);
void world_trees_draw_depth(const WorldTrees *t, EngRenderQueue *queue, mat4 light_view_proj);

#endif
