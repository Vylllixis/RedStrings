#ifndef LAUNCHER_WORLD_GRASS_H
#define LAUNCHER_WORLD_GRASS_H

/* Herbe de la plaine dans le monde ouvert (2026-11-22) : des touffes de brins (engine/grass.h :
 * eng_grass_tuft_build) animées par le vent, placées par la carte (eng_worldmap_grass : toujours
 * aux mêmes endroits), en deux semis autour de la caméra :
 *   - de près, dense : une touffe tous les 0,45 m, visible jusqu'à 22 m ;
 *   - au loin, clairsemée et plus haute : une tous les 1,3 m, visible jusqu'à 46 m.
 * Chaque touffe rapetisse puis disparaît avec la distance DANS LE SHADER (grass.vert) : rien
 * n'apparaît d'un coup. Les listes sont refaites quand la caméra a bougé de 5 m (elles couvrent
 * 5 m de plus que la distance de disparition). Pas d'herbe sur les chemins, la roche, dans l'eau,
 * hors de la plaine, ni sous les murs du décor (world_grass_set_blockers). */

#include <cglm/cglm.h>
#include <stdbool.h>
#include <stdint.h>

#include "engine/grass.h"
#include "engine/instancing.h"
#include "engine/model_gpu.h"
#include "engine/world_map.h"
#include "engine/world_origin.h"

#define WORLD_GRASS_RINGS 2U
#define WORLD_GRASS_MAX 20000U    /* touffes par semis */
#define WORLD_GRASS_BLOCKERS 256U /* boîtes du décor sans herbe */
#define WORLD_GRASS_MARGIN_M 5.0F /* les listes sont refaites tous les 5 m */

typedef struct WorldGrassRing {
    EngBufferHandle buffer;
    uint32_t count;
} WorldGrassRing;

/* sol sous (x, z) du repère local ; false : relief pas encore chargé (la touffe garde la hauteur
 * de la carte) */
typedef bool (*WorldGrassGround)(void *user, float x, float z, float *out);

typedef struct WorldGrass {
    EngModelGpu tuft;
    EngPipelineHandle pipeline;
    EngRenderer *renderer;
    WorldGrassRing ring[WORLD_GRASS_RINGS];
    float blockers[WORLD_GRASS_BLOCKERS][5]; /* centre x, z ; demi-côtés x, z ; cap (local) */
    uint32_t blocker_count;
    float camera[3];  /* dernière image : caméra (locale) */
    float center[3];  /* caméra quand les listes ont été faites (locale) */
    double time_s;    /* vent */
    uint32_t tufts;   /* touffes dans les listes */
    uint32_t blocked; /* touffes écartées sous le décor à la dernière construction */
    uint32_t rebuilds;
    bool valid;
    bool ready;
} WorldGrass;

bool world_grass_init(WorldGrass *g, EngRenderer *renderer, const EngEnvironment *env);
void world_grass_shutdown(WorldGrass *g);
/* Recentrage de l'origine : tout se décale de `shift` et les listes sont à refaire. */
void world_grass_shift(WorldGrass *g, const float shift[3]);
/* Boîtes du décor (murs, piliers) sous lesquelles l'herbe ne pousse pas ; REMPLACE les
 * précédentes. */
void world_grass_set_blockers(WorldGrass *g, float (*boxes)[5], uint32_t count);
/* À chaque image, AVANT ses dessins : refait les listes si la caméra a bougé de 5 m. */
void world_grass_update(WorldGrass *g, const EngWorldMap *map, const EngWorldOrigin *origin,
                        WorldGrassGround ground, void *user);
void world_grass_draw(WorldGrass *g, EngRenderQueue *queue, mat4 view_proj,
                      const EngLighting *light, const EngModelShadow *shadow);

#endif
