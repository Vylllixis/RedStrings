#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/fog.h"
#include "engine/model_gpu.h"
#include "engine/renderer.h"
#include "engine/splatmap.h"
#include "engine/terrain.h"

/* Terrain sur le GPU (M3-03) : l'EngModel du terrain (engine/terrain.h) dessiné avec le fragment
 * shader `terrain.frag` (mélange de quatre textures de couche par une splatmap, mapping
 * triplanaire, éclairage PBR, ombre : docs/terrain.md). S'appuie sur EngModelGpu pour les tampons,
 * l'environnement et la file de dessins ; le vertex shader est celui des modèles (model.vert).
 *
 * Construire le terrain avec eng_terrain_splat_material() (facteur de couleur blanc) : la couleur
 * vient des textures. */

typedef struct EngTerrainGpuParams {
    const EngSplatmap *splat; /* obligatoire : un texel par sommet du terrain */
    /* Pixels RGBA 8 bits (sRGB) des quatre couches (herbe, roche, terre, neige), layer_size x
     * layer_size chacune ; NULL : la texture générée par eng_terrain_layer_pixels. */
    const uint8_t *layer_pixels[ENG_SPLAT_LAYERS];
    uint32_t layer_size;       /* 0 : ENG_TERRAIN_LAYER_TEXTURE_SIZE ; >= 4 */
    float tile_meters;         /* longueur d'une répétition des couches, mètres ; 0 : 8 */
    float triplanar_sharpness; /* exposant des poids triplanaires, >= 1 ; 0 : 4 */
    float anisotropy;          /* filtrage anisotrope (0 : aucun, jusqu'à 16) */
} EngTerrainGpuParams;

typedef struct EngTerrainGpu {
    EngModelGpu model;
    EngTextureHandle splat;
    EngTextureHandle layers[ENG_SPLAT_LAYERS];
    EngSamplerHandle sampler;
    float tile_meters;
    float sharpness;
    uint32_t part_count;
    float fog[ENG_FOG_UNIFORM_FLOATS]; /* eng_fog_pack ; tout à 0 : pas de brouillard */
} EngTerrainGpu;

/* Pipeline du terrain : mêmes sommets que les modèles (EngModelVertex), shaders model.vert et
 * terrain.frag, profondeur activée, face arrière éliminée. */
void eng_terrain_pipeline_desc(EngPipelineDesc *desc);

/* Crée les ressources GPU. `arena` reçoit les tableaux du modèle GPU (autant que le terrain) ;
 * `scratch` sert à générer les textures de couche (restituée). La splatmap doit avoir le côté du
 * champ du terrain (size x size, un texel par sommet). Échec (false, journalisé) : tout est
 * détruit. */
bool eng_terrain_gpu_create(EngTerrainGpu *gpu, EngRenderer *renderer, EngArena *arena,
                            EngArena *scratch, const EngTerrain *terrain,
                            const EngTerrainGpuParams *params);
void eng_terrain_gpu_destroy(EngTerrainGpu *gpu, EngRenderer *renderer);

/* Remplace l'environnement qui éclaire le terrain (voir eng_model_gpu_set_environment). */
bool eng_terrain_gpu_set_environment(EngTerrainGpu *gpu, EngRenderer *renderer,
                                     const EngEnvironment *env);

/* Règle le brouillard (engine/fog.h) appliqué par terrain.frag. `fog` NULL ou invalide, ou
 * `atmosphere` NULL ou invalide : pas de brouillard. `sky_scale` : échelle d'exposition du ciel
 * dessiné (>= 0) ; la couleur du brouillard est celle du ciel à l'horizon. À appeler avant
 * eng_terrain_gpu_draw, à chaque image si le soleil bouge. Renvoie false si un des arguments a
 * été refusé (le brouillard est alors coupé). */
bool eng_terrain_gpu_set_fog(EngTerrainGpu *gpu, const EngFog *fog, const EngAtmosphere *atmosphere,
                             float sky_scale);

/* Met en file un dessin par chunk. `chunks` NULL : tous ; sinon les `count` chunks listés (indices
 * de EngTerrain.chunks, hors limites ignorés : c'est le culling de M3-04). Le terrain est en
 * coordonnées monde (matrice modèle = identité). Renvoie le nombre de dessins acceptés. */
uint32_t eng_terrain_gpu_draw(const EngTerrainGpu *gpu, EngRenderQueue *queue,
                              EngPipelineHandle pipeline, mat4 view_proj,
                              const EngLighting *lighting, const EngModelShadow *shadow,
                              const uint32_t *chunks, uint32_t count);

/* Met en file les chunks dans la PASSE D'OMBRE (voir eng_model_gpu_draw_depth). */
uint32_t eng_terrain_gpu_draw_depth(const EngTerrainGpu *gpu, EngRenderQueue *queue,
                                    EngPipelineHandle pipeline, mat4 light_view_proj);
