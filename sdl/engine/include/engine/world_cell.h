#ifndef ENG_WORLD_CELL_H
#define ENG_WORLD_CELL_H

/* Partition du monde (M7-01) : grille infinie de cellules carrées de 256 m. Une cellule porte son
 * relief (65 x 65 hauteurs, une tous les 4 m, bords partagés avec les voisines), ses objets
 * (rochers, arbres) et ses points de gameplay (camps). Le contenu est une FONCTION PURE de
 * (graine, coordonnées) : deux cellules voisines générées séparément se raccordent exactement, et
 * une cellule déchargée puis régénérée est identique au bit près.
 *
 * Repère : la cellule (cx, cz) couvre x ∈ [cx·256 ; (cx+1)·256[, z idem. Coordonnées monde en
 * double (précision au-delà de 10 km), locales à la cellule en float.
 *
 * Format binaire « RSCL » v1 (petit-boutiste) : voir docs/world-streaming.md. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/biome.h"
#include "engine/noise.h"

#define ENG_CELL_SIZE 256.0
#define ENG_CELL_RES 65U /* échantillons par côté */
#define ENG_CELL_SPACING 4.0F
#define ENG_CELL_MAX_OBJECTS 48U

typedef struct EngCellCoord {
    int32_t x;
    int32_t z;
} EngCellCoord;

typedef enum EngCellObjectKind {
    ENG_CELL_ROCK = 0,
    ENG_CELL_TREE = 1,
    ENG_CELL_CAMP = 2, /* point d'apparition d'un camp d'ennemis */
    ENG_CELL_OBJECT_KINDS
} EngCellObjectKind;

typedef struct EngCellObject {
    uint8_t kind;
    float pos[2]; /* local à la cellule, mètres */
    float y;      /* hauteur du sol à cet endroit */
    float yaw;
    float scale; /* rayon d'obstacle pour les rochers et arbres */
} EngCellObject;

typedef struct EngCellData {
    EngCellCoord coord;
    float heights[ENG_CELL_RES * ENG_CELL_RES]; /* [z * RES + x] */
    float height_min;
    float height_max;
    EngCellObject objects[ENG_CELL_MAX_OBJECTS];
    uint32_t object_count;
} EngCellData;

typedef struct EngWorldGen {
    int32_t seed;
    float altitude; /* amplitude du relief, mètres */
    EngNoiseParams noise;
    uint32_t rocks_per_cell;
    uint32_t trees_per_cell;
    uint32_t camp_every; /* un camp toutes les `camp_every` cellules environ (0 : aucun) */
    EngBiomeMap biomes; /* grande carte : relief et arbres selon le biome ; désactivée par défaut */
    /* Carte du monde dessinée à la main (engine/world_map.h, construite) : si non NULL, le relief
     * vient de la carte et les cellules n'ont ni rochers, ni arbres, ni camps procéduraux (les
     * arbres viennent de la carte). Lue en parallèle par les tâches : ne pas la modifier. */
    const struct EngWorldMap *map;
} EngWorldGen;

EngWorldGen eng_world_gen_default(int32_t seed);

EngCellCoord eng_cell_of(double x, double z);
bool eng_cell_equal(EngCellCoord a, EngCellCoord b);

/* Génère la cellule ; false si un argument est invalide. Thread-safe (aucun état global). */
bool eng_cell_generate(const EngWorldGen *gen, EngCellCoord c, EngCellData *out);

/* Hauteur du générateur au point monde (x, z), sans cellule : le bruit multiplié par l'altitude
 * et par l'échelle du biome. C'est la valeur que eng_cell_generate écrit aux sommets. */
float eng_world_gen_height(const EngWorldGen *gen, double x, double z);

/* Hauteur interpolée (bilinéaire) au point local (lx, lz) ∈ [0 ; 256]. */
float eng_cell_height(const EngCellData *cell, float lx, float lz);

uint64_t eng_cell_hash(const EngCellData *cell);

/* Sérialisation « RSCL » v1. `eng_cell_encoded_size` donne la taille exacte. */
size_t eng_cell_encoded_size(const EngCellData *cell);
size_t eng_cell_encode(const EngCellData *cell, void *out, size_t capacity);
bool eng_cell_decode(const void *data, size_t size, EngCellData *out);

#endif
