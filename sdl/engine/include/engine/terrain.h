#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/heightmap.h"
#include "engine/model.h"

/* Maillage du terrain par chunks (M3-02), côté CPU : un champ de hauteurs (engine/heightmap.h)
 * devient un EngModel que le renderer dessine comme n'importe quel modèle (engine/model_gpu.h :
 * matériau PBR, ombre du soleil, éclairage par image).
 *
 * Disposition. UN tableau de sommets partagé pour tout le terrain (un sommet par échantillon du
 * champ, size x size), UN tableau d'index (deux triangles par cellule), et UNE PARTIE PAR CHUNK
 * (EngModel.parts[i] <-> EngTerrain.chunks[i], i = cz * chunks_per_side + cx). Un chunk est un bloc
 * de `chunk_cells` x `chunk_cells` cellules ; deux chunks voisins partagent la rangée de sommets de
 * leur bord : les positions, normales, UV et tangentes y sont identiques PAR CONSTRUCTION (ce sont
 * les mêmes sommets), il n'y a donc ni fissure ni changement d'éclairage au raccord (critère
 * « sans couture »). Les normales sont calculées sur la grille GLOBALE, jamais par chunk.
 *
 * Repère. Le sommet de l'échantillon (gx, gz) est en
 *   x = origin_x + gx * spacing,  y = altitude (mètres),  z = origin_z + gz * spacing
 * (main droite, Y vers le haut : conventions du projet). Triangles anti-horaires vus d'en haut
 * (normale géométrique vers +Y). La diagonale de chaque cellule alterne en damier (pas de direction
 * privilégiée dans l'éclairage). Normale : différences centrées sur la grille (unilatérales au
 * bord), (-dh/dx, 1, -dh/dz) normalisée. UV : (gx, gz) / (size - 1), donc (0, 0) à (1, 1) sur TOUT
 * le terrain (coordonnées de la splatmap, M3-03) ; le tiling des matériaux se fera dans le shader.
 * Tangente : le sens des u croissants, w = +1.
 *
 * Limites : celles d'EngModel (4 194 304 sommets, 16 777 216 index) : size <= 1673 ; au-delà, le
 * streaming de M7. */

#define ENG_TERRAIN_DEFAULT_CHUNK_CELLS 64U

typedef struct EngTerrainParams {
    uint32_t chunk_cells; /* cellules par côté d'un chunk ; 0 : 64. (size - 1) doit en être un
                             multiple */
    float origin_x;       /* position monde du sommet (0, 0), mètres */
    float origin_z;
    const EngModelMaterial *material; /* NULL : herbe grise-verte, rugosité 0,95, métal 0 */
} EngTerrainParams;

/* Boîte englobante d'un chunk : TOUS les sommets qu'il utilise (bords compris), exactement. */
typedef struct EngTerrainChunk {
    float bounds_min[3];
    float bounds_max[3];
} EngTerrainChunk;

typedef struct EngTerrain {
    EngModel
        model; /* materials[0] : défaut de glTF ; materials[1] : le terrain (toutes les parties) */
    EngTerrainChunk *chunks;
    uint32_t chunks_per_side;
    uint32_t chunk_cells;
} EngTerrain;

/* Matériau pour un terrain mélangé par splatmap (engine/terrain_gpu.h) : facteur de couleur blanc
 * (la couleur vient des textures de couche), métal 0, rugosité 0,95, sans texture. */
EngModelMaterial eng_terrain_splat_material(void);

/* Vérifie les paramètres pour ce champ (journalisé) : champ valide, (size - 1) multiple de
 * chunk_cells, origine finie, limites du modèle respectées, matériau valide. */
bool eng_terrain_params_valid(const EngHeightmap *map, const EngTerrainParams *params);

/* Construit le terrain. Tableaux alloués dans `arena` (environ 48 octets par échantillon pour les
 * sommets et 24 par échantillon pour les index : 75 Mio pour 1025 x 1025) ; échec (false,
 * journalisé) : `out` mis à zéro et `arena` restituée. Les hauteurs sont lues dans `map`, qui n'a
 * pas besoin de survivre au terrain. */
bool eng_terrain_build(EngArena *arena, const EngHeightmap *map, const EngTerrainParams *params,
                       EngTerrain *out);

/* Indice du chunk (cx, cz) ; UINT32_MAX hors de la grille de chunks. */
/* M7-05 : niveaux de détail d'un terrain CARRÉ (une cellule du monde streamé). `grid` : le modèle
 * d'eng_terrain_build pour un champ de `size` x `size` (une partie). Résultat (dans `arena`) : les
 * mêmes sommets + 4 x (size - 1) sommets de JUPE (ceux du bord, abaissés de `skirt_depth` m), et
 * `levels` parties (une par niveau, toutes dans le même tampon d'index) : le niveau l relie un
 * échantillon sur 2^l (même diagonale en damier, anti-horaires vus d'en haut), suivi de sa jupe
 * (triangles des deux faces) : entre deux cellules de niveaux différents, les sommets de bord ne
 * coïncident plus et la jupe bouche la fente. `size - 1` doit être divisible par 2^(levels-1) ;
 * 1 <= levels <= ENG_TERRAIN_LOD_MAX_LEVELS. false (arena restituée) si invalide ou mémoire
 * insuffisante. */
#define ENG_TERRAIN_LOD_MAX_LEVELS 4U
bool eng_terrain_build_lod(EngArena *arena, const EngModel *grid, uint32_t size, uint32_t levels,
                           float skirt_depth, EngModel *out);

uint32_t eng_terrain_chunk_index(const EngTerrain *terrain, uint32_t cx, uint32_t cz);
