#ifndef LAUNCHER_WORLD_TREES_H
#define LAUNCHER_WORLD_TREES_H

/* Arbres et petit décor de la carte du monde dans le monde ouvert (2026-11-21, 2026-11-22) :
 * chênes, sapins enneigés, acacias, buissons et rochers (assets/props/nature), placés par la carte
 * (eng_worldmap_trees). Une liste d'instances par cellule chargée (coordonnées LOCALES : décalées
 * à chaque recentrage de l'origine).
 *
 *   - ARBRES : un dessin instancié par essence et par cellule, dans l'image et dans l'ombre.
 * Au-delà de 220 m, une version simplifiée (28 à 64 triangles) ; seuls les arbres à moins de 200 m
 *     projettent une ombre.
 *   - PETIT DÉCOR (buissons, rochers) : dessiné seulement autour de la caméra (170 m pour les
 *     buissons, 260 m pour les rochers), dans une liste refaite quand elle a bougé de 4 m ; chaque
 *     pied grandit de zéro à sa taille en approchant : rien n'apparaît d'un coup.
 *
 * Troncs et rochers à moins de 40 m du joueur sont des corps solides (les buissons se traversent) ;
 * world_trees_obstacles les donne aux ennemis, qui les évitent (engine/ai_avoid.h).
 *
 * ARBRES RÉALISTES (2026-11-22, world_trees_real.c, docs/arbres-realistes.md) : quand ils sont
 * demandés et que leurs modèles (assets/props/nature/real, produits par l'outil treebake à partir
 * d'arbres scannés) se chargent, ils REMPLACENT les trois essences d'arbres simples. Chaque arbre
 * choisit son niveau de détail selon sa distance à la caméra : de près (tronc détaillé, feuillage
 * en cartes), à mi-distance (allégé), de loin (quatre rectangles). Les arbres des cellules proches
 * sont rangés dans trois listes par essence, refaites quand la caméra a bougé de 5 m ; ceux des
 * cellules lointaines sont dessinés par cellule, en vue lointaine. Shaders des arbres
 * (engine/tree_render.h) : vent, feuillage découpé, ombre découpée. */

#include <cglm/cglm.h>
#include <stdbool.h>
#include <stdint.h>

#include "engine/frustum.h"
#include "engine/instancing.h"
#include "engine/physics.h"
#include "engine/tree_render.h"
#include "engine/world_map.h"
#include "engine/world_origin.h"
#include "engine/world_stream.h"

#define WORLD_TREES_PER_CELL 2400U
#define WORLD_TREE_BODIES 320U
#define WORLD_NEAR_MAX 1024U /* petit décor dessiné autour de la caméra, par essence */
#define WORLD_REAL_KINDS 3U  /* arbres réalistes : chêne, sapin, acacia (les trois premières) */
#define WORLD_REAL_LODS 3U   /* de près, à mi-distance, de loin */
#define WORLD_REAL_PARTS 8U  /* parties d'un niveau de détail, au plus */

typedef struct WorldTreeSlot {
    EngInstance inst[WORLD_TREES_PER_CELL]; /* rangées par essence */
    float solid[WORLD_TREES_PER_CELL]; /* rayon de l'obstacle (m) : tronc, rocher ; 0 : aucun */
    EngBufferHandle buffer[ENG_TREE_KINDS]; /* arbres seulement */
    uint32_t first[ENG_TREE_KINDS];
    uint32_t count[ENG_TREE_KINDS];
    float bmin[3]; /* boîte englobante locale */
    float bmax[3];
    EngCellCoord coord;
    uint32_t total;
    bool used;
    bool listed; /* arbres réalistes : cellule proche, ses arbres sont dans les listes par distance
                  */
} WorldTreeSlot;

typedef struct WorldNearList { /* le petit décor d'une essence autour de la caméra */
    EngBufferHandle buffer;
    uint32_t count;
    uint32_t shadow_count; /* les premiers : assez près pour projeter une ombre */
} WorldNearList;

/* Une essence d'arbre réaliste : un modèle dont les parties sont rangées par niveau de détail
 * (treebake : P parties de près, les P mêmes à mi-distance, 1 partie de loin). */
typedef struct WorldRealKind {
    EngModelGpu model;
    uint32_t parts[WORLD_REAL_LODS][WORLD_REAL_PARTS];
    uint32_t part_count[WORLD_REAL_LODS];
    EngTreeShape shape;
    float size;                           /* hauteur du modèle (m) */
    WorldNearList lists[WORLD_REAL_LODS]; /* arbres des cellules proches, par niveau */
    bool loaded;
} WorldRealKind;

typedef struct WorldTrees {
    EngModelGpu models[ENG_TREE_KINDS];
    EngModelGpu lods[ENG_TREE_KINDS]; /* arbres : versions simplifiées, au loin */
    float camera[3];                  /* dernière image : position de la caméra (locale) */
    EngPipelineHandle pipeline;
    EngPipelineHandle shadow_pipeline;
    EngRenderer *renderer;
    EngPhysics *physics;
    WorldTreeSlot slots[ENG_STREAM_SLOTS];
    WorldNearList near[ENG_TREE_KINDS];
    float near_center[3]; /* où était la caméra quand le petit décor a été choisi */
    bool near_valid;
    EngBody bodies[WORLD_TREE_BODIES];
    uint32_t body_count;
    float body_center[2];  /* où le joueur était quand les corps solides ont été choisis */
    uint32_t drawn;        /* dernière image : arbres dessinés */
    uint32_t near_drawn;   /* dernière image : buissons et rochers dessinés */
    uint32_t loaded;       /* pieds des cellules chargées (arbres et petit décor) */
    uint32_t loaded_small; /* dont petit décor */
    uint32_t truncated;    /* cellules dont la liste a été tronquée (WORLD_TREES_PER_CELL) */
    bool has_center;
    bool ready;
    /* arbres réalistes (world_trees_real.c) */
    WorldRealKind real[WORLD_REAL_KINDS];
    EngPipelineHandle tree_pipeline;
    EngPipelineHandle tree_shadow_pipeline;
    float real_center[3]; /* où était la caméra quand les listes ont été faites */
    double time_s;        /* horloge du vent */
    uint32_t real_drawn[WORLD_REAL_LODS]; /* dernière image : arbres dessinés par niveau */
    uint32_t real_dropped; /* arbres laissés de côté : listes pleines (journalisé une fois) */
    bool realistic;        /* les arbres réalistes remplacent les arbres simples */
    bool real_valid;
} WorldTrees;

/* Charge les modèles et crée les pipelines. false (journalisé) : le monde reste sans arbres.
 * `realistic` : arbres réalistes ; s'ils ne se chargent pas (journalisé), les arbres simples. */
bool world_trees_init(WorldTrees *t, EngRenderer *renderer, EngPhysics *physics,
                      const EngEnvironment *env, bool realistic);
void world_trees_shutdown(WorldTrees *t);
/* Instances de la cellule `coord` (emplacement `slot`), en local par `origin`. */
void world_trees_load_cell(WorldTrees *t, uint32_t slot, const EngWorldMap *map, EngCellCoord coord,
                           const EngWorldOrigin *origin);
void world_trees_unload_cell(WorldTrees *t, uint32_t slot);
/* Recentrage de l'origine : toutes les instances (et les corps solides) se décalent de `shift`. */
void world_trees_shift(WorldTrees *t, const float shift[3]);
/* À chaque image, AVANT ses dessins : petit décor autour de la caméra (refait quand elle a bougé
 * de 4 m) et corps solides autour du joueur (`feet` local ; rechoisis quand il a bougé de 8 m). */
void world_trees_update_bodies(WorldTrees *t, const float feet[3]);
/* Troncs et rochers à moins de `radius` de `center` (x, z locaux) : (x, z, rayon) dans `out`, au
 * plus `max`. Pour l'évitement des ennemis. */
uint32_t world_trees_obstacles(const WorldTrees *t, const float center[2], float radius,
                               float (*out)[3], uint32_t max);
/* `fog` : brouillard du terrain (ENG_FOG_UNIFORM_FLOATS flottants d'eng_fog_pack) pour les arbres
 * réalistes, ou NULL. */
void world_trees_draw(WorldTrees *t, EngRenderQueue *queue, mat4 view_proj,
                      const EngLighting *light, const EngModelShadow *shadow,
                      const EngFrustum *frustum, const float *fog);
void world_trees_draw_depth(const WorldTrees *t, EngRenderQueue *queue, mat4 light_view_proj);
/* Avance l'horloge du vent des arbres réalistes de `dt_s` secondes. */
void world_trees_tick(WorldTrees *t, float dt_s);

/* ── Interne (world_trees_near.c) : petit décor autour de la caméra ── */
bool world_tree_is_small(uint32_t kind); /* buisson ou rocher : dessiné de près seulement */
/* distance horizontale de `p` à la boîte de la cellule */
float world_tree_slot_distance(const WorldTreeSlot *s, const float p[3]);
void world_trees_near_refresh(WorldTrees *t); /* refait les listes si la caméra a bougé */
void world_trees_near_release(WorldTrees *t);
void world_trees_clear_bodies(WorldTrees *t);

/* ── Interne (world_trees_real.c) : arbres réalistes ── */
/* Lit le modèle `relative` (chemin relatif à l'exécutable) et l'envoie au GPU. */
bool world_tree_load_model(EngRenderer *renderer, const char *relative, EngModelGpu *out);
/* Modèles et pipelines ; false (journalisé, tout est libéré) : restent les arbres simples. */
bool world_trees_real_init(WorldTrees *t, const EngEnvironment *env);
void world_trees_real_shutdown(WorldTrees *t);
void world_trees_real_refresh(WorldTrees *t); /* refait les listes si la caméra a bougé */
void world_trees_real_release(WorldTrees *t);
void world_trees_real_draw(WorldTrees *t, EngRenderQueue *queue, mat4 view_proj,
                           const EngLighting *light, const EngModelShadow *shadow,
                           const EngFrustum *frustum, const float *fog);
void world_trees_real_draw_depth(const WorldTrees *t, EngRenderQueue *queue, mat4 light_view_proj);
/* Mesures d'une essence selon les modèles en service (simples ou réalistes) : hauteur du modèle,
 * rayon de l'obstacle et encombrement horizontal en fraction de la hauteur de l'arbre. */
float world_tree_model_size(const WorldTrees *t, uint32_t kind);
float world_tree_solid_ratio(const WorldTrees *t, uint32_t kind);
float world_tree_spread(const WorldTrees *t, uint32_t kind);

#endif
