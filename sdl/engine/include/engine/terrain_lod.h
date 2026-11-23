#ifndef ENG_TERRAIN_LOD_H
#define ENG_TERRAIN_LOD_H

/* LOD du terrain (M7-05), côté CPU : sélection CDLOD (Strugar 2009) sur un quadtree. Chaque nœud
 * de niveau k couvre leaf·2^k mètres et se dessine avec une même grille de N x N quads (donc des
 * quads de plus en plus grands avec la distance). Un nœud est subdivisé tant que la caméra est à
 * moins de `range[k-1]` de sa boîte. Les plages doublent d'un niveau au suivant, ce qui garantit
 * que deux nœuds voisins sélectionnés diffèrent d'AU PLUS un niveau ; le facteur de morph
 * (0 près, 1 au bout de la plage) fait glisser les sommets impairs vers leurs voisins pairs avant
 * le changement de niveau : pas de fissure, pas de saut (« popping »). */

#include <stdbool.h>
#include <stdint.h>

#define ENG_TLOD_MAX_LEVELS 12U
#define ENG_TLOD_MAX_NODES 4096U

typedef struct EngTlodNode {
    float min[2]; /* coin (x, z) */
    float size;
    uint32_t level; /* 0 = le plus fin */
    float morph_start;
    float morph_end;
} EngTlodNode;

typedef struct EngTlodParams {
    float origin[2]; /* coin du quadtree */
    float extent;    /* côté total (mètres) = leaf · 2^(levels-1) */
    float leaf;      /* côté d'un nœud de niveau 0 */
    uint32_t levels;
    float lod_range0;  /* plage du niveau 0 (mètres) ; doublée à chaque niveau */
    float morph_ratio; /* début du morph en fraction de la plage (0,7) */
    float height_min;  /* boîtes des nœuds (le relief) */
    float height_max;
} EngTlodParams;

typedef struct EngTlodSelection {
    EngTlodNode nodes[ENG_TLOD_MAX_NODES];
    uint32_t count;
    uint32_t per_level[ENG_TLOD_MAX_LEVELS];
    bool overflow;
} EngTlodSelection;

bool eng_tlod_params_valid(const EngTlodParams *p);
bool eng_tlod_select(const EngTlodParams *p, const float camera[3], EngTlodSelection *out);

/* Facteur de morph d'un sommet à `distance` de la caméra pour un nœud donné ([0 ; 1]). */
float eng_tlod_morph(const EngTlodNode *n, float distance);

#endif
