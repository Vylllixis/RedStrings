#ifndef ENG_WORLD_MAP_H
#define ENG_WORLD_MAP_H

/* Carte du monde dessinée à la main (demande de Théo, 2026-11-21) : UNE carte, toujours la même,
 * décrite dans un fichier texte (assets/world/carte.txt) : colline du château de départ, régions
 * (plaine, montagne enneigée, désert, volcan), montagnes, crêtes, volcan, plateaux, dunes, lacs,
 * rivières, chemins, aires aplanies, zones de combat. Rien n'est tiré au hasard : le seul bruit est
 * un détail de surface à graine FIXE, identique à chaque lancement.
 *
 * Repère : mètres, x vers l'est, z vers le SUD (le nord est en -z, en haut de l'aperçu), le
 * château de départ au centre (0, 0). La carte couvre [-côté/2 ; côté/2] sur x et z ; au-delà, la
 * bordure (falaises) continue.
 *
 * Étapes : eng_worldmap_parse (texte -> description), eng_worldmap_build (description -> relief et
 * régions, tramés une fois au chargement), puis eng_worldmap_height / _weights (lecture,
 * thread-safe) et eng_worldmap_render (aperçu vu de dessus, carte du jeu). Format du fichier :
 * docs/world-map.md. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/biome.h"
#include "engine/map_zones.h"
#include "engine/model.h"

#define ENG_WORLDMAP_MAX_FEATURES 160U
#define ENG_WORLDMAP_MAX_POINTS                                                                    \
    640U /* sommets de toutes les lignes (crêtes, rivières, chemins)                             \
          */
#define ENG_WORLDMAP_MAX_REGIONS 16U
#define ENG_WORLDMAP_MAX_SIZE 4097U /* échantillons par côté */

typedef enum EngMapFeatureKind {
    ENG_MAPF_HILL = 0, /* colline : bosse arrondie, sommet plat optionnel */
    ENG_MAPF_MOUNTAIN, /* montagne : pic */
    ENG_MAPF_RIDGE,    /* crête : chaîne le long d'une ligne */
    ENG_MAPF_VOLCANO,  /* volcan : cône et cratère à fond plat */
    ENG_MAPF_MESA,     /* plateau : sommet plat, flancs raides */
    ENG_MAPF_DUNES,    /* dunes : vagues de sable */
    ENG_MAPF_FLATTEN,  /* aire aplanie (arènes, camps, château) */
    ENG_MAPF_PATH,     /* chemin : pente adoucie, terre battue */
    ENG_MAPF_LAKE,     /* lac : creusé sous le niveau de l'eau */
    ENG_MAPF_RIVER,    /* rivière : lit creusé le long d'une ligne */
    ENG_MAPF_LAVA,     /* coulée de lave le long d'une ligne (sillon peu profond) */
    ENG_MAPF_FOREST,   /* forêt : arbres d'une essence dans un disque */
    ENG_MAPF_SCATTER,  /* arbres épars d'une essence partout dans un biome */
    ENG_MAPF_KINDS
} EngMapFeatureKind;

typedef struct EngMapFeature {
    float x; /* centre (formes rondes) */
    float z;
    float radius; /* rayon ; largeur pour les lignes */
    float height; /* hauteur ; profondeur pour les lacs et rivières ; NAN : aplanir au sol local */
    float a;      /* plateau de la colline, rayon du cratère, amplitude des dunes */
    float b;      /* profondeur du cratère, longueur d'onde des dunes */
    float c;      /* direction des dunes (radians) */
    uint32_t first_point; /* lignes : premier sommet dans EngWorldMap.points */
    uint32_t point_count;
    uint8_t kind;  /* EngMapFeatureKind */
    uint8_t tree;  /* forêt, arbres épars : EngTreeKind */
    uint8_t biome; /* arbres épars : EngBiome */
} EngMapFeature;

typedef enum EngTreeKind {
    ENG_TREE_OAK = 0,   /* chêne (plaine) */
    ENG_TREE_PINE,      /* sapin enneigé (montagne) */
    ENG_TREE_ACACIA,    /* arbre de savane (désert) */
    ENG_TREE_BUSH,      /* buisson (plaine) */
    ENG_TREE_BUSH_SNOW, /* buisson enneigé (montagne) */
    ENG_TREE_BUSH_DRY,  /* buisson sec (désert) */
    ENG_TREE_ROCK,      /* rocher (partout, même au volcan) */
    ENG_TREE_KINDS
} EngTreeKind;

typedef struct EngMapTree {
    float x; /* monde */
    float z;
    float y;      /* sol au pied */
    float yaw;    /* radians */
    float scale;  /* taille en mètres : hauteur d'un arbre ou d'un buisson, largeur d'un rocher */
    uint8_t kind; /* EngTreeKind */
    uint8_t tint[3]; /* teinte (255 : neutre) : variation d'un pied à l'autre, roche du biome */
} EngMapTree;

typedef struct EngMapRegion {
    float x; /* graine de la région : le biome domine autour de ce point */
    float z;
    float reach;   /* portée (m) : une région de grande portée s'étend plus loin */
    uint8_t biome; /* EngBiome */
} EngMapRegion;

typedef struct EngMapZoneSpot {
    float x;
    float z;
    uint8_t biome; /* EngBiome */
    uint8_t kind;  /* EngMapZoneKind */
    bool used;
} EngMapZoneSpot;

typedef struct EngWorldMap {
    /* description (eng_worldmap_parse) */
    float side;         /* côté de la carte (m) */
    float spacing;      /* m entre deux échantillons du relief */
    float water;        /* niveau de l'eau (m) */
    float border_start; /* distance au centre (norme « carré arrondi ») où montent les falaises */
    float border_height;
    float spawn[2];
    EngMapRegion regions[ENG_WORLDMAP_MAX_REGIONS];
    EngMapFeature features[ENG_WORLDMAP_MAX_FEATURES];
    float points[ENG_WORLDMAP_MAX_POINTS][2];
    EngMapZoneSpot zones[ENG_MAPZONE_COUNT]; /* indice : biome * 3 + genre (map_zones.h) */
    uint32_t region_count;
    uint32_t feature_count;
    uint32_t point_count;
    /* trame (eng_worldmap_build) : size x size échantillons, ligne z par ligne z */
    uint32_t size;
    float *heights;   /* m */
    uint8_t *weights; /* 4 octets par échantillon : montagne, désert, volcan, chemin (0 à 255) */
    uint8_t *lava; /* 1 octet par échantillon : coulée de lave (0 à 255), jamais sur un chemin */
    float height_min;
    float height_max;
} EngWorldMap;

/* Lit la description depuis `text` (`length` octets, pas forcément terminé par 0). false : erreur,
 * décrite dans `err` (« ligne N : ... »). La trame n'est pas construite. */
bool eng_worldmap_parse(EngWorldMap *map, const char *text, size_t length, char *err,
                        size_t err_cap);

/* Construit le relief et les régions (allocations dans `arena` : 9 octets par échantillon).
 * Toujours le même résultat pour la même description. false si l'arène est trop petite. */
bool eng_worldmap_build(EngWorldMap *map, EngArena *arena);
/* Octets d'arène demandés par eng_worldmap_build pour cette description. */
size_t eng_worldmap_build_bytes(const EngWorldMap *map);

/* Lecture (après build ; thread-safe). Hors de la carte : le bord, prolongé. */
float eng_worldmap_height(const EngWorldMap *map, double x, double z);
/* Poids des biomes (somme 1) ; `path` (peut être NULL) : 0 à 1, 1 au milieu d'un chemin. */
void eng_worldmap_weights(const EngWorldMap *map, double x, double z, float out[ENG_BIOME_COUNT],
                          float *path);

/* Lave : 0 à 1, 1 au milieu d'une coulée. */
float eng_worldmap_lava(const EngWorldMap *map, double x, double z);

/* Places des 12 zones de combat : celles de engine/map_zones.h (noms, PV, dégâts, butin), à la
 * position donnée par la carte (`zone ...`) quand elle y figure. */
bool eng_worldmap_zones(const EngWorldMap *map, EngMapZone out[ENG_MAPZONE_COUNT]);

/* Arbres, buissons et rochers dont le pied est dans le carré [x0 ; x0 + size[ x [z0 ; z0 + size[
 * (au plus `max`) : forêts et objets épars de la carte (seuls ou par petits groupes), sur une
 * grille fixe du monde légèrement décalée (même résultat quel que soit le découpage en cellules).
 * Jamais sur un chemin, dans l'eau, sur la lave, sur une pente raide, ni près du départ ou d'une
 * zone de combat ; arbres et buissons jamais dans le volcan (les rochers, si). */
uint32_t eng_worldmap_trees(const EngWorldMap *map, double x0, double z0, double size,
                            EngMapTree *out, uint32_t max);
const char *eng_tree_name(EngTreeKind kind);

/* ── Herbe de la plaine (2026-11-22) ── */

#define ENG_GRASS_MAX_SLOPE 0.6F /* pas d'herbe sur une pente plus raide (roche) */

typedef struct EngMapTuft {
    float x; /* monde */
    float z;
    float y;     /* sol au pied (interpolé sur la trame) */
    float yaw;   /* radians */
    float scale; /* hauteur de la touffe (m) : de 0,55 à 1,4 fois la hauteur demandée */
    uint8_t
        tint[3]; /* teinte : taches d'herbe haute plus jaune, variation d'une touffe à l'autre */
} EngMapTuft;

/* Part d'herbe en (x, z), de 0 à 1 : 1 en pleine plaine ; 0 hors de la plaine, sur un chemin, sur
 * la lave, dans l'eau (à moins de 25 cm au-dessus) ou sur une pente de plus de
 * ENG_GRASS_MAX_SLOPE ; entre les deux aux frontières des biomes et sur les pentes. */
float eng_worldmap_grass_cover(const EngWorldMap *map, double x, double z);

/* Touffes d'herbe à moins de `radius` (au plus 400 m) de (cx, cz), au plus `max` : une grille fixe
 * du monde de pas `spacing` (>= 0,2 m), chaque nœud décalé dans sa maille ; une touffe est gardée
 * avec la probabilité eng_worldmap_grass_cover. Les MÊMES touffes (position, taille, teinte)
 * quel que soit le centre : l'herbe ne change pas quand le joueur avance. `salt` distingue deux
 * semis (herbe dense de près, herbe clairsemée au loin). `height` : hauteur type (m). */
uint32_t eng_worldmap_grass(const EngWorldMap *map, double cx, double cz, float radius,
                            float spacing, float height, uint32_t salt, EngMapTuft *out,
                            uint32_t max);
/* Tronc ou rocher : bloque le joueur et les ennemis ; un buisson se traverse. */
bool eng_tree_is_solid(EngTreeKind kind);

/* Surface de l'eau (lacs, rivières) : maillage compact des cellules de la trame dont un coin est
 * sous `map->water`, à cette altitude, coordonnées de la carte ; uv.x = profondeur au sommet,
 * SIGNÉE (négative là où le relief dépasse : water.frag écarte ces fragments, la rive suit le
 * relief), normale +Y (à dessiner avec water.frag, engine/water.h). Carte sans eau : true,
 * modèle vide. Tableaux dans `arena`. false : carte non construite ou arène trop petite. */
bool eng_worldmap_water(const EngWorldMap *map, EngArena *arena, EngModel *out);
uint32_t eng_worldmap_wet_cells(const EngWorldMap *map);
/* Y a-t-il de l'eau (relief sous le niveau) à moins de `reach` mètres (en carré) de (x, z) ? */
bool eng_worldmap_near_water(const EngWorldMap *map, double x, double z, double reach);

/* Aperçu vu de dessus, `size` x `size` pixels RGB (3 octets, ligne 0 = nord) : relief ombré,
 * couleur des biomes, eau, chemins, lave, zones (vert : facile, orange : dur, rouge : boss) et
 * départ (blanc). false si un argument est invalide ou la carte non construite. */
bool eng_worldmap_render(const EngWorldMap *map, uint32_t size, uint8_t *rgb);

const char *eng_mapfeature_name(EngMapFeatureKind kind);

#endif
