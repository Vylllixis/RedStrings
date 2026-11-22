#ifndef LAUNCHER_WORLD_WATER_H
#define LAUNCHER_WORLD_WATER_H

/* Eau de la carte du monde (lacs, rivières) dans le monde ouvert : la surface (un seul maillage
 * pour toute la carte : eng_worldmap_water), dessinée avec le shader d'eau du moteur
 * (engine/water.h : couleur selon la profondeur, reflet du ciel, rides, reflet du soleil,
 * brouillard), et un FOND DE GUÉ physique : le joueur ne descend pas plus bas que 0,7 m sous la
 * surface (de l'eau jusqu'à la taille), il traverse à gué. */

#include <cglm/cglm.h>
#include <stdbool.h>
#include <stdint.h>

#include "engine/atmosphere.h"
#include "engine/fog.h"
#include "engine/model_gpu.h"
#include "engine/physics.h"
#include "engine/water.h"
#include "engine/world_map.h"
#include "engine/world_origin.h"

#define WORLD_WADE_DEPTH 0.7F /* profondeur du gué sous la surface (m) */

typedef struct WorldWater {
    EngModelGpu model;
    EngPipelineHandle pipeline;
    EngWaterParams params;
    EngWaterSky sky;
    EngPhysics *physics;
    EngBody floor;
    double time_s;
    float side; /* côté de la carte (m) */
    uint32_t wet_cells;
    bool ready;
} WorldWater;

/* Construit la surface de l'eau de `map` et son pipeline. false : carte sans eau ou ressource
 * manquante (le monde reste sans eau). */
bool world_water_init(WorldWater *w, EngRenderer *renderer, EngPhysics *physics, EngArena *arena,
                      const EngWorldMap *map);
void world_water_shutdown(WorldWater *w, EngRenderer *renderer);
/* (Re)crée le fond de gué au repère local de `origin` (au lancement, à chaque recentrage). */
void world_water_place_floor(WorldWater *w, const EngWorldOrigin *origin);
/* Ciel réfléchi par l'eau : celui de l'atmosphère pour ce soleil. */
void world_water_set_sky(WorldWater *w, const EngAtmosphere *atmosphere, const float sun[3],
                         float sky_intensity);
/* Hauteur du fond de gué (m) ; -INFINITY sans eau. */
float world_water_floor(const WorldWater *w);
void world_water_draw(const WorldWater *w, EngRenderQueue *queue, const EngWorldOrigin *origin,
                      const float fog[ENG_FOG_UNIFORM_FLOATS], mat4 view_proj,
                      const EngLighting *light);

#endif
