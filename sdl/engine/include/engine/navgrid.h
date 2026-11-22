#ifndef ENG_NAVGRID_H
#define ENG_NAVGRID_H

/* Maillage de navigation en grille + A* (M6-03/04).
 *
 * ÉCART ASSUMÉ avec le tracker (« navmesh Recast ») : Recast/Detour n'ont pas d'API C et ne sont
 * pas disponibles hors ligne dans l'environnement de développement ; la grille régulière (cellules
 * bloquées = obstacles gonflés du rayon de l'agent) donne les mêmes services pour un camp de
 * test (chemin, contournement, lissage) avec un code minuscule et déterministe. Le jour où Recast
 * est branché, `eng_nav_path` garde sa signature. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/ai_world.h"
#include "engine/debug_lines.h"

#define ENG_NAV_MAX_DIM 128U
#define ENG_NAV_MAX_CELLS (ENG_NAV_MAX_DIM * ENG_NAV_MAX_DIM)

typedef struct EngNavGrid {
    float origin[2]; /* coin (x, z) minimal */
    float cell;
    uint32_t w;
    uint32_t h;
    uint8_t blocked[ENG_NAV_MAX_CELLS];
    /* travail de A* (réutilisé à chaque requête) */
    float g[ENG_NAV_MAX_CELLS];
    float f[ENG_NAV_MAX_CELLS];
    int32_t parent[ENG_NAV_MAX_CELLS];
    uint8_t state[ENG_NAV_MAX_CELLS];
    int32_t heap_pos[ENG_NAV_MAX_CELLS];
    uint32_t heap[ENG_NAV_MAX_CELLS];
    uint32_t heap_n;
    uint32_t scratch[ENG_NAV_MAX_CELLS];
} EngNavGrid;

/* Couvre [min ; max] avec des cellules de `cell` mètres (au plus 128 x 128). Une cellule est
 * bloquée si son centre est à moins de `agent_radius` d'un obstacle. */
bool eng_nav_build(EngNavGrid *nav, const EngAiWorld *world, const float min[2], const float max[2],
                   float cell, float agent_radius);

/* Relief (M6-03, terrain) : hauteur du sol en (x, z). */
typedef float (*EngNavHeightFn)(void *user, float x, float z);

/* Bloque les cellules trop raides : pente estimée par différences centrées sur ±1/2 cellule en x
 * et en z (une marche plus haute que la demi-cellule compte comme une pente raide). Les cellules
 * déjà bloquées le restent. Retourne le nombre de cellules nouvellement bloquées ; 0 si un argument
 * est invalide (`max_slope_deg` dans ]0 ; 90[). À appeler après eng_nav_build. */
uint32_t eng_nav_block_steep(EngNavGrid *nav, EngNavHeightFn height, void *user,
                             float max_slope_deg);

/* Bloque les îlots praticables de moins de `min_cells` cellules (4-connexité) : poches isolées
 * entre deux pentes, où un ennemi déplacé ou poussé resterait coincé. Comme la surface minimale de
 * région de Recast. Retourne le nombre de cellules bloquées ; utilise le travail de A*. */
uint32_t eng_nav_remove_islands(EngNavGrid *nav, uint32_t min_cells);

/* Point praticable (dans la grille et hors cellule bloquée) ? */
bool eng_nav_walkable(const EngNavGrid *nav, const float p[2]);

/* Point praticable le plus proche de `p` (M6-03 : postes d'un camp posés sur une pente) : `p`
 * lui-même s'il est praticable, sinon le centre de la cellule libre la plus proche (au plus 12
 * cellules). false (out inchangé) si `p` est hors de la grille ou s'il n'y en a aucune. */
bool eng_nav_nearest_walkable(const EngNavGrid *nav, const float p[2], float out[2]);

/* Chemin lissé de `from` vers `to` : points d'étape (sans `from`), dernier = `to` (ou le point
 * praticable le plus proche si `to` est bloqué). Retourne le nombre de points, 0 si aucun chemin.
 */
uint32_t eng_nav_path(EngNavGrid *nav, const float from[2], const float to[2], float out[][2],
                      uint32_t max_points);

/* Segment entièrement praticable ? */
bool eng_nav_line_free(const EngNavGrid *nav, const float a[2], const float b[2]);

/* Debug : contour des cellules bloquées (croix) à la hauteur `y`. Retourne le nombre de lignes. */
uint32_t eng_nav_draw(const EngNavGrid *nav, EngDebugLines *lines, float y);

#endif
