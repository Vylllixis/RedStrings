#pragma once

#include <stdbool.h>

#include "engine/csm.h"
#include "engine/grass.h"
#include "engine/water.h"
#include "engine/arena.h"
#include "engine/asset_cook.h"
#include "engine/asset_types.h"
#include "engine/assets.h"
#include "engine/camera.h"
#include "engine/debug_lines.h"
#include "engine/debug_lines_gpu.h"
#include "engine/editor.h"
#include "engine/environment.h"
#include "engine/heightmap.h"
#include "engine/lighting.h"
#include "engine/model_gpu.h"
#include "engine/renderer.h"
#include "engine/shadow.h"
#include "engine/terrain.h"
#include "engine/atmosphere.h"
#include "engine/frustum.h"
#include "engine/terrain_gpu.h"
#include "engine/world.h"

/* Scène de démonstration provisoire du lanceur : des cubes texturés et un modèle cuit, dessinés
 * par le renderer avec une matrice MVP (uniforms), une texture PNG avec mipmaps et un depth buffer.
 * Elle utilise l'API publique du renderer comme le fera le jeu. Le renderer doit avoir été créé
 * avec depth_buffer = true. */

#define DEMO_MAX_TERRAIN_CHUNKS 1024U /* chunks suivis pour le culling (32 x 32) */
#define DEMO_WORLD_MODELS 16U         /* modèles distincts utilisables par le monde */

/* Un modèle demandé par une entité du monde : chemin tel que dans EngMeshRenderer.model, handle du
 * gestionnaire d'assets (nul si le chargement a échoué : l'échec est mémorisé, pas réessayé à
 * chaque image). */
typedef struct DemoWorldModel {
    char path[ENG_MESH_PATH_MAX];
    EngAssetHandle handle;
} DemoWorldModel;

/* Dessin de debug supplémentaire (mode jouable) : voir demo_scene_set_debug_hook. */
/* `model` : matrice du modèle central à cet instant ; `has_model` : ce modèle est affiché (pas de
 * monde éditeur). */
typedef void (*DemoDebugHook)(void *user, EngDebugLines *lines, mat4 model, bool has_model);

/* Modèles supplémentaires (M4 : personnages animés) : voir demo_scene_set_model_hook. Appelé dans
 * la passe d'ombre (`shadow_pass`, `view_proj` = matrice du soleil, `light` et `shadow` NULL) puis
 * dans la passe principale (après le décor). */
typedef void (*DemoModelHook)(void *user, EngRenderQueue *queue, mat4 view_proj,
                              const EngLighting *light, const EngModelShadow *shadow,
                              bool shadow_pass);

typedef struct DemoScene {
    EngCsm csm; /* M7-09 : cascades de l'image (mode monde) ; en tête : alignement des mat4 */
    EngBufferHandle vertices;
    EngBufferHandle indices;
    EngTextureHandle texture;
    EngSamplerHandle sampler;
    EngPipelineHandle pipeline;
    /* Modèle cuit (assets/cooked/models/demo.mesh), chargé par le gestionnaire d'assets : s'il n'a
     * pas pu l'être, has_model est faux et la scène garde son cube central. `model` pointe dans le
     * payload du gestionnaire (stable tant que `model_handle` est tenu). */
    EngAssets assets;
    EngModelAssetContext model_context;
    uint32_t model_type;
    EngAssetHandle model_handle;
    const EngModelGpu *model;
    EngPipelineHandle model_pipeline;
    /* Éclairage par image (M1-16) : ciel procédural, skybox et éclairage du modèle. Sans lui
     * (shader absent, échec de création), le modèle garde son ciel uniforme blanc et pas de
     * skybox. */
    EngEnvironment environment;
    EngPipelineHandle sky_pipeline;
    /* Ciel atmosphérique (M3-05) : sans image HDR (--env), le ciel est calculé par le shader
     * atmosphere.frag à partir de la direction du soleil de l'éclairage, et la couleur du soleil
     * et l'ambiante suivent sa hauteur. `last_*` : valeurs de la dernière image (journal). */
    EngAtmosphere atmosphere;
    EngPipelineHandle atmosphere_pipeline;
    float last_sun_elevation_deg;
    float last_sun_color[3];
    float last_sun_visibility;
    float last_sky_intensity; /* M7 : brouillard du monde streamé */
    float last_true_sun[4]; /* M8-07 : vrai soleil xyz (la lumière peut être la lune) ; w libre */
    float night; /* M8-07 : poids de la nuit [0, 1] (clair de lune), réglé par le lanceur */
    float last_ambient[3];
    /* Brouillard du terrain (M3-06) : actif si le ciel atmosphérique l'est (sa couleur en vient).
     * main.c lie ses réglages aux tweaks ; `fog.density` 0 : coupé. */
    EngFog fog;
    /* Ombre du soleil (M1-17) : un sol qui reçoit les ombres des cubes et du modèle, une carte
     * d'ombre rendue avant l'image. Sans eux (shaders, format de profondeur, sol impossibles), la
     * scène est dessinée sans ombre. */
    EngModelGpu ground;
    EngTextureHandle shadow_map;
    uint32_t shadow_size;
    EngPipelineHandle model_shadow_pipeline;
    EngPipelineHandle cube_shadow_pipeline;
    EngPipelineHandle instanced_shadow_pipeline; /* B8 : arbres et rochers dans la passe d'ombre */
    /* Terrain (--terrain, M3-02) : s'il est chargé, la scène ne dessine que le ciel et lui (voir
     * demo_scene_draw). `terrain_view` : position conseillée de la caméra. */
    EngTerrainGpu terrain;
    EngPipelineHandle terrain_pipeline;
    float terrain_view[3];
    /* Rochers instanciés (--rocks N, M3-07) : un maillage de rocher, N instances posées au hasard
     * sur le terrain (placement procédural réel : M3-08), UN dessin par image. */
    EngModelGpu rocks;
    EngPipelineHandle rock_pipeline;
    EngBufferHandle rock_instances;
    uint32_t rock_count;
    /* Placement procédural (--placement, M3-08) : végétation sur l'herbe (même maillage, teinte
     * verte), UN dessin de plus ; les rochers suivent la roche de la splatmap. */
    EngBufferHandle veg_instances;
    uint32_t veg_count;
    /* B8 : avec --placement, rochers et arbres KayKit (CC0, assets/props/nature/) au lieu du
     * petit rocher procédural teinté ; repli sur celui-ci si les fichiers manquent. */
    EngModelGpu tree;
    /* Herbe animée (--grass, M3-09) : un brin, un champ de brins, UN dessin, vent et fondu au
     * vertex shader. */
    EngModelGpu grass_blade;
    EngPipelineHandle grass_pipeline;
    EngBufferHandle grass_instances;
    uint32_t grass_count;
    EngGrassStyle grass_style;
    /* Culling (M3-04) : boîtes des chunks (copiées du EngTerrain CPU, libéré après le chargement),
     * et résultat de la dernière image. Au-delà de DEMO_MAX_TERRAIN_CHUNKS : tout est dessiné. */
    EngTerrainChunk terrain_bounds[DEMO_MAX_TERRAIN_CHUNKS];
    uint32_t terrain_chunk_total;
    uint32_t terrain_chunks_drawn;
    /* Debug draw (M1-11) : lignes accumulées à chaque image puis envoyées par eng_debug_lines_gpu.
     */
    EngDebugLines lines;
    EngDebugLinesGpu lines_gpu;
    /* Rechargement à chaud (--watch-assets, M2-06) : recuit le modèle quand une de ses sources
     * change (commande `assetc`, une règle par source). Les chaînes vivent ici : la scène ne doit
     * pas être déplacée une fois initialisée. */
    EngAssetCook cook;
    char cook_sources[3][512];
    char cook_output[512];
    const char *cook_argv[5];
    /* Monde édité (M2-10b) : voir demo_scene_set_world. Non possédé. */
    EngWorld *world;
    DemoWorldModel world_models[DEMO_WORLD_MODELS];
    uint32_t world_model_count;
    /* Ce qui a pu être créé (chaque élément est facultatif), regroupé pour limiter le remplissage.
     */
    bool has_atmosphere;
    bool has_rocks;
    bool has_veg;
    bool has_grass;
    /* Eau (--water, M3-10) : un plan opaque au niveau `water_params.level`, dessiné après le
     * terrain. */
    EngModelGpu water_model;
    EngPipelineHandle water_pipeline;
    EngWaterParams water_params;
    uint32_t water_cells;
    bool has_water;
    bool has_assets;
    bool has_model;
    bool has_environment;
    bool has_ground;
    bool has_terrain;
    bool has_shadow;
    bool has_lines;
    bool has_tree; /* B8 : `tree` chargé */
    DemoDebugHook debug_hook; /* demo_scene_set_debug_hook */
    void *debug_user;
    DemoModelHook model_hook; /* demo_scene_set_model_hook */
    void *model_user;
    float shadow_focus[3]; /* M7 : centre de la carte d'ombre en mode monde (le joueur) */
    bool hide_props;       /* sol seul : ni cubes ni modèle central (galerie des personnages) */
    bool world_mode;       /* M7 : monde streamé (crochet de modèles) : ni sol ni cubes */
    bool cascades;         /* M7-09 : ombres en cascade (demo_scene_use_cascades) */
    bool has_cook;
    bool world_models_full_warned;
} DemoScene;

/* Réglages de la démonstration (options --no-mipmaps et --anisotropy du lanceur) : permettent de
 * comparer le rendu avec et sans mipmaps, avec et sans anisotropie. */
typedef struct DemoOptions {
    bool mipmaps; /* chaîne de mipmaps complète + filtrage trilinéaire ; faux : niveau 0 seul */
    float anisotropy; /* 0 ou 1 : désactivée ; jusqu'à 16 */
    const char
        *environment_path; /* image équirectangulaire .hdr du ciel ; NULL : ciel procédural */
    uint32_t environment_samples; /* échantillons du préfiltrage ; 0 : défaut du moteur */
    const char *terrain_path;     /* --terrain : champ de hauteurs .hmap ; NULL : pas de terrain */
    float sun_elevation;          /* --sun-elevation : hauteur initiale du soleil (degrés) */
    float sun_azimuth;            /* --sun-azimuth : azimut initial du soleil (degrés) */
    uint32_t rock_count;          /* --rocks : rochers instanciés sur le terrain (0 : aucun) */
    bool water;                   /* --water : plan d'eau (M3-10) */
    bool water_level_set;         /* --water-level M donné */
    float water_level;            /* altitude du plan, mètres */
    bool grass;              /* --grass : champ d'herbe animée sur l'herbe du terrain (M3-09) */
    bool placement;          /* --placement : rochers et végétation placés par règles (M3-08) */
    uint32_t placement_seed; /* --placement-seed : graine des règles (défaut 1) */
    float placement_clearing_m; /* B8 : rayon (m) laissé sans arbre ni rocher autour du centre
                                   (combat : aire de jeu et camp) ; 0 : aucun */
    float fog_density; /* --fog-density : densité du brouillard (1/m) ; < 0 : défaut ; 0 : coupé */
    bool watch_assets; /* --watch-assets : recuire et recharger le modèle quand ses sources changent
                        */
} DemoOptions;

#define DEMO_SHADOW_SIZE 2048U /* côté de la carte d'ombre, en texels */
#define DEMO_CSM_COUNT 4U      /* M7-09 : cascades du mode monde (atlas 2 x 2) */
#define DEMO_CSM_FAR 160.0F    /* m : au-delà, pas d'ombre */
#define DEMO_CSM_LAMBDA 0.8F   /* découpage : 0 uniforme, 1 logarithmique */

/* `permanent` : arène des données du modèle GPU (vit autant que la scène) ; `level` : reçoit
 * temporairement le modèle côté CPU ; `scratch` : travail et lecture des fichiers. `level` et
 * `scratch` sont restituées dans leur état d'origine. */
bool demo_scene_init(DemoScene *scene, EngRenderer *renderer, EngArena *permanent, EngArena *level,
                     EngArena *scratch, const DemoOptions *options);

/* Terrain (--terrain) : vrai si la scène en dessine un ; `position` reçoit alors un point de vue
 * conseillé (au-dessus du centre du terrain). Le terrain remplace la démonstration : ciel et
 * terrain seulement, éclairé par le soleil et l'environnement mais sans ombre portée (les casters
 * de la démonstration ne sont pas dessinés ; l'ombre du terrain viendra avec les cascades). */
bool demo_scene_terrain_view(const DemoScene *scene, float position[3]);

/* Dessine la scène vue par `camera` : une grille de 7 × 7 cubes espacés de 8 m (repères pour se
 * déplacer), au centre, le modèle glTF (un tore et une sphère) qui tourne sur lui-même.
 * `time_seconds` : temps de simulation (pas fixes + alpha), qui pilote la rotation ; `aspect` :
 * largeur / hauteur de la fenêtre. `lighting` : éclairage du modèle glTF, matériaux PBR (la
 * position de la caméra y est écrasée par celle de `camera`, pour le spéculaire) ; NULL : éclairage
 * par défaut. Les cubes texturés ne sont pas éclairés (shader textured.*). Le ciel (skybox) est
 * dessiné en premier ; l'intensité de l'ambiante de `lighting` est l'intensité de l'environnement
 * (ciel et éclairage du modèle).
 *
 * Ombre (M1-17, docs/shadows.md) : `shadow` non NULL, l'ombre du soleil est calculée avec ces
 * réglages : le sol et le modèle la reçoivent, les cubes, le modèle et le sol la projettent (la
 * file d'ombre du `renderer` est remplie, la carte est rendue par eng_renderer_end_frame). Les
 * cubes texturés, sans éclairage, ne reçoivent pas d'ombre. `shadow` NULL ou ombres indisponibles :
 * aucune passe d'ombre. La carte couvre la sphère de rayon 42 m autour de l'origine, qui contient
 * le sol (±28 m) et la grille de cubes. */
/* M7-09 : ombres en cascade (mode monde) : la carte d'ombre devient un atlas de 2 x 2 cascades de
 * DEMO_SHADOW_SIZE texels, recalculées à chaque image autour de la caméra (eng_csm_update). */
bool demo_scene_use_cascades(DemoScene *scene, EngRenderer *renderer);

void demo_scene_draw(DemoScene *scene, EngRenderer *renderer, EngRenderQueue *queue,
                     double time_seconds, const EngCamera *camera, float aspect,
                     const EngLighting *lighting, const EngShadowParams *shadow);
/* Debug draw de la scène (touche F3), À APPELER APRÈS demo_scene_draw dans la même image : grille
 * au sol (une ligne plus claire tous les 10 cases), repère du monde, la boîte orientée qui entoure
 * le modèle glTF et son repère, une sphère autour de chaque cube de la grille d'angle. Les lignes
 * testées en profondeur sont cachées par les cubes ; les repères sont superposés. Sans effet si le
 * debug draw n'a pas pu être créé.
 *
 * `scene_debug` : la grille, les sphères et les repères de la démonstration (F3) ; `editor` non
 * NULL : la sélection et les poignées de l'éditeur (en superposition). Le modèle central n'est plus
 * entouré quand la scène dessine un monde. Rien à dessiner : aucune commande. */
void demo_scene_draw_debug(DemoScene *scene, EngRenderer *renderer, EngRenderQueue *queue,
                           double time_seconds, const EngCamera *camera, float aspect,
                           bool scene_debug, const EngEditor *editor);

/* Monde édité (M2-10b) : tant qu'un monde est donné, la scène dessine SES entités (composants
 * Transform + MeshRenderer visibles, modèles chargés par le gestionnaire d'assets d'après le champ
 * `model`) à la place du modèle central tournant ; la grille de cubes, le sol et le ciel restent.
 * Les modèles sont demandés dans demo_scene_update (au plus DEMO_WORLD_MODELS chemins distincts ;
 * les suivants ne sont pas dessinés). NULL : retour à la démonstration. Le monde doit survivre à la
 * scène ou être retiré avant sa destruction. */
void demo_scene_set_world(DemoScene *scene, EngWorld *world);
/* `hook` est appelé par demo_scene_draw_debug sur la liste de lignes de la scène, après la grille
 * et avant l'envoi au GPU. NULL : aucun. */
void demo_scene_set_debug_hook(DemoScene *scene, DemoDebugHook hook, void *user);
/* `hook` dessine des modèles en plus du décor (arène et terrain), dans les deux passes. NULL :
 * aucun. */
void demo_scene_set_model_hook(DemoScene *scene, DemoModelHook hook, void *user);
/* Bornes locales du modèle d'une entité (EngEditorBoundsFn, user = DemoScene*) ; false si
 * l'entité n'a pas de MeshRenderer ou si son modèle n'est pas chargé. */
bool demo_scene_entity_bounds(void *scene, EngEntity entity, float min[3], float max[3]);
/* À appeler une fois par image AVANT de construire l'image, avec un temps croissant en secondes :
 * avec --watch-assets, recuit et recharge ce qui a changé (engine/asset_cook.h, engine/assets.h).
 */
void demo_scene_update(DemoScene *scene, double now_seconds);
void demo_scene_shutdown(DemoScene *scene, EngRenderer *renderer);
