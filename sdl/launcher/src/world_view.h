#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/world_map.h"
#include "world_far.h"
#include "world_trees.h"
#include "world_grass.h"
#include "world_water.h"
#include "engine/arena.h"
#include "engine/atmosphere.h"
#include "engine/environment.h"
#include "engine/fog.h"
#include "engine/jobs.h"
#include "engine/lighting.h"
#include "engine/model_gpu.h"
#include "engine/physics.h"
#include "engine/renderer.h"
#include "engine/terrain_gpu.h"
#include "engine/upload_queue.h"
#include "engine/world_origin.h"
#include "engine/world_stream.h"

/* Monde ouvert RENDU (M7, option --world) : les cellules de 256 m que le streaming charge dans des
 * jobs (world_stream) deviennent, une fois prêtes :
 *   - un maillage de terrain (même shader triplanaire et mêmes couches que le terrain de M3, une
 *     splatmap de 65 x 65 texels par cellule), envoyé au GPU par la file d'uploads à budget
 *     (upload_queue : WORLD_VIEW_UPLOAD_BUDGET octets par image, les plus proches d'abord) ;
 *   - un corps Jolt « champ de hauteurs » (M7-11), retiré quand la cellule est déchargée.
 * Les positions sont LOCALES à l'origine flottante (world_origin, M7-10) : chaque cellule est
 * placée par une translation (coin de cellule − origine) et, quand l'origine saute, les corps Jolt
 * des cellules sont recréés au nouvel endroit et l'appelant décale le joueur (`shift`).
 *
 * Coût mémoire borné : ENG_STREAM_SLOTS emplacements préalloués (arène par emplacement). */

#define WORLD_VIEW_UPLOAD_BUDGET (64U * 1024U) /* octets par image : docs/perf-budgets.md */
#define WORLD_VIEW_REBASE_M 512.0              /* recentrage de l'origine au-delà (m) */
#define WORLD_VIEW_HIST_MS 100U                /* histogramme des images, par milliseconde */
#define WORLD_VIEW_LOD_LEVELS 3U /* M7-05 : pas de 1, 2 et 4 échantillons (4, 8 et 16 m) */

typedef struct WorldCellView {
    bool used;     /* cellule prête dans le streaming */
    bool uploaded; /* maillage GPU et corps Jolt créés */
    EngCellCoord coord;
    EngArena arena; /* tables du modèle GPU, remise à zéro au déchargement */
    EngModelGpu model;
    EngTextureHandle splat;
    EngBody body;
    EngTextureHandle splat2; /* carte du monde : seconde carte de poids (sable, lave) */
    bool sand;               /* la cellule a du sable (compteurs, tests) */
    bool lava;               /* ... de la lave */
    uint32_t lod;            /* M7-05 : niveau dessiné (0 : le plus fin) */
    float height_min;
    float height_max;
} WorldCellView;

typedef struct WorldView {
    bool ready;
    EngJobs jobs;
    EngWorldGen gen;
    EngWorldStream stream;
    EngWorldOrigin origin;
    EngUploadQueue uploads;
    EngPhysics *physics; /* monde physique du mode jouable */
    EngRenderer *renderer;
    EngTerrainGpu shared;       /* couches, sampler et brouillard communs à toutes les cellules */
    EngPipelineHandle pipeline; /* terrain (model.vert + terrain.frag) */
    const EngWorldMap *map;     /* carte du monde dessinée à la main (NULL : bandes de biomes) */
    WorldTrees *trees;          /* arbres de la carte (NULL : aucun) */
    WorldFar far;               /* vue lointaine de la carte (far.ready faux : aucune) */
    WorldWater water;           /* lacs et rivières de la carte (water.ready faux : aucune) */
    WorldGrass grass;           /* herbe de la plaine (grass.ready faux : aucune) */
    EngArena map_arena;         /* relief et régions tramés de la carte */
    EngPipelineHandle shadow_pipeline; /* profondeur des modèles statiques */
    EngArena shared_arena;
    EngArena scratch; /* construction d'une cellule (restituée à chaque fois) */
    WorldCellView cells[ENG_STREAM_SLOTS];
    /* mesures (journal, --world-bench rendu) */
    uint32_t uploaded_cells;
    uint64_t uploads_done;
    uint64_t bodies_added;
    uint64_t bodies_removed;
    double worst_upload_ms;
    double worst_stage_ms[4]; /* pire temps par étape : maillage, splatmap, GPU, Jolt */
    double stage_sum_ms[4];
    uint32_t frame_uploads; /* dernière mise à jour : cellules envoyées */
    bool frame_rebased;     /* dernière mise à jour : origine recentrée */
    double frame_update_ms; /* dernière mise à jour : durée de world_view_update */
    uint32_t cells_drawn;   /* dernière image, après élimination par le frustum */
    uint32_t lod_drawn[WORLD_VIEW_LOD_LEVELS]; /* dernière image : cellules par niveau */
    uint32_t sand_drawn;      /* dernière image : cellules dessinées avec la couche de sable */
    uint32_t lava_drawn;      /* ... et avec la couche de lave */
    uint32_t triangles_drawn; /* dernière image, passe principale */
    /* temps des images après le chargement initial (critère M7 n° 1 : aucune > 25 ms) */
    uint32_t frame_hist[WORLD_VIEW_HIST_MS + 1U];
    uint32_t frames;
    double frame_sum_ms;
    uint32_t frames_over_25;
    float worst_frame_ms;
    float worst_cpu_ms;
} WorldView;

/* Arbres du prochain world_view_init : réalistes (arbres scannés, world_trees.h ; c'est le défaut)
 * ou simples. Les arbres simples servent aussi de repli si les réalistes ne se chargent pas. */
void world_view_use_real_trees(bool on);

/* `physics` : le monde du joueur (les cellules y ajoutent leurs corps). `env` : éclairage par image
 * (peut être NULL). false (journalisé) si une ressource manque. */
bool world_view_init(WorldView *w, int32_t seed, EngRenderer *renderer, EngPhysics *physics,
                     const EngEnvironment *env);
void world_view_shutdown(WorldView *w);

/* Une image : streaming autour du joueur (position MONDE), uploads étalés, recentrage de
 * l'origine. Retourne true si l'origine a bougé : `shift` (à AJOUTER aux positions locales du
 * joueur, de la caméra...) est alors rempli. */
bool world_view_update(WorldView *w, const double player_world[3], float shift[3]);

/* Chargement initial BLOQUANT (écran de chargement) : toutes les cellules à au plus `radius` du
 * joueur prêtes et envoyées. false si `timeout_s` est dépassé. */
bool world_view_prime(WorldView *w, const double player_world[3], int32_t radius, double timeout_s);

/* Brouillard du terrain (couleur du ciel à l'horizon, à l'intensité du ciel dessiné). */
void world_view_set_fog(WorldView *w, const EngFog *fog, const EngAtmosphere *atmosphere,
                        float sky_intensity, const float true_sun[3]);

/* Dessin des cellules envoyées et visibles (frustum) : passe principale ou d'ombre. */
void world_view_draw(WorldView *w, EngRenderQueue *queue, mat4 view_proj, const EngLighting *light,
                     const EngModelShadow *shadow);
void world_view_draw_depth(const WorldView *w, EngRenderQueue *queue, mat4 light_view_proj);

/* Mesures : une image (durée totale et travail CPU, ms) ; résumé journalisé (« MONDE : ... »). */
void world_view_note_frame(WorldView *w, float frame_ms, float cpu_ms);
void world_view_log_summary(const WorldView *w, double travelled_m);

/* Hauteur du sol au point LOCAL (x, z) si sa cellule est chargée. */
bool world_view_height(const WorldView *w, float x, float z, float *out);
