#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/math.h"
#include "engine/terrain.h"

/* Pyramide de vision (frustum) et culling CPU (M3-04). Six plans extraits de la matrice
 * vue-projection (méthode de Gribb et Hartmann) ; profondeur [0, 1] comme SDL_GPU
 * (docs/conventions.md). Chaque plan est (nx, ny, nz, d), normale UNITAIRE tournée vers l'intérieur
 * : un point p est du côté visible si n·p + d >= 0. Les plans sont déduits de la matrice telle
 * quelle : la convention de la projection (axe Y inversé ou non) n'a pas d'importance.
 *
 * Test d'une boîte : conservatif. Elle n'est rejetée que si ses huit coins sont du mauvais côté
 * d'UN MÊME plan (test du sommet le plus avancé) ; une boîte rejetée est donc forcément invisible,
 * une boîte acceptée peut l'être (un coin de la pyramide près d'une grande boîte), jamais
 * l'inverse. */

typedef struct EngFrustum {
    float planes[6][4]; /* gauche, droite, bas, haut, proche, lointain */
} EngFrustum;

typedef enum EngFrustumResult {
    ENG_FRUSTUM_OUTSIDE = 0, /* entièrement hors de la pyramide */
    ENG_FRUSTUM_INTERSECT,   /* à cheval sur au moins un plan (ou peut-être visible) */
    ENG_FRUSTUM_INSIDE,      /* entièrement à l'intérieur des six plans */
} EngFrustumResult;

/* Extrait les plans de `view_proj` (monde -> découpage, colonne-majeur). false si la matrice n'est
 * pas finie ou dégénérée (un plan sans normale) : `out` rejette alors tout (chaque plan vaut -1).
 */
bool eng_frustum_from_view_proj(EngFrustum *out, mat4 view_proj);

/* Position par rapport à la pyramide : le point est dedans ou sur un plan. */
bool eng_frustum_contains_point(const EngFrustum *frustum, const float point[3]);

/* Classe la boîte alignée sur les axes [min, max] (min <= max composante par composante). Une boîte
 * à coordonnée non finie ou inversée est ENG_FRUSTUM_OUTSIDE (jamais dessinée : elle ne peut pas
 * être prouvée visible). */
EngFrustumResult eng_frustum_classify_aabb(const EngFrustum *frustum, const float min[3],
                                           const float max[3]);

/* Vrai si la boîte n'est pas rejetée (INSIDE ou INTERSECT). */
bool eng_frustum_intersects_aabb(const EngFrustum *frustum, const float min[3], const float max[3]);

/* Chunks du terrain visibles : écrit leurs indices (croissants) dans `out` (au plus `capacity`) et
 * renvoie leur nombre. `out` peut recevoir tous les chunks : prévoir chunks_per_side² éléments ; un
 * tampon trop petit tronque le résultat (et le nombre renvoyé est celui des indices écrits). */
uint32_t eng_terrain_visible_chunks(const EngTerrain *terrain, const EngFrustum *frustum,
                                    uint32_t *out, uint32_t capacity);

/* Même test sur un tableau de boîtes de chunks (copie des `bounds` quand le EngTerrain CPU n'est
 * plus là : le lanceur ne garde que les boîtes). */
uint32_t eng_chunks_visible(const EngTerrainChunk *chunks, uint32_t chunk_count,
                            const EngFrustum *frustum, uint32_t *out, uint32_t capacity);
