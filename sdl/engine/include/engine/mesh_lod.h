#ifndef ENG_MESH_LOD_H
#define ENG_MESH_LOD_H

/* LOD des meshes (M7-06) : simplification par regroupement de sommets sur une grille (vertex
 * clustering, Rossignac-Borrel) et sélection par distance avec hystérésis.
 *
 * ÉCART ASSUMÉ avec le tracker (« meshoptimizer ») : la dépendance n'est pas disponible hors ligne
 * ici. Le regroupement est plus grossier qu'un effondrement d'arêtes par quadriques (il ne
 * préserve pas aussi bien les silhouettes) mais il est simple, déterministe, borné en erreur
 * (≤ diagonale d'une case de grille) et suffit pour des niveaux lointains. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ENG_MESH_LOD_MAX 4U

/* Simplifie (positions xyz, index de triangles). `cells` = cases par côté de la boîte englobante.
 * Écrit les nouveaux sommets/index (capacités en éléments) ; retourne false si elles ne suffisent
 * pas. Les triangles devenus dégénérés sont retirés. Non réentrant (tampons statiques) : outil de
 * cuisson, à appeler depuis un seul thread. */
bool eng_mesh_simplify(const float *positions, uint32_t vertex_count, const uint32_t *indices,
                       uint32_t index_count, uint32_t cells, float *out_positions,
                       uint32_t out_vertex_cap, uint32_t *out_vertex_count, uint32_t *out_indices,
                       uint32_t out_index_cap, uint32_t *out_index_count);

/* Sélection : niveau i pour distance >= thresholds[i-1] (thresholds croissants, count-1 valeurs).
 * Hystérésis : pour remonter vers un niveau plus fin, il faut repasser `hysteresis` (fraction,
 * ex. 0,1 = 10 %) SOUS le seuil ; une caméra immobile sur un seuil ne fait pas clignoter le LOD. */
uint32_t eng_mesh_lod_select(uint32_t current, float distance, const float *thresholds,
                             uint32_t count, float hysteresis);

#endif
