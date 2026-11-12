#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/math.h"

/* Debug draw (M1-11), côté CPU : on accumule des lignes colorées dans le monde (segments, boîtes,
 * sphères, axes, grille...) pendant l'image ; engine/debug_lines_gpu.h les envoie au GPU et les
 * dessine. Ce module ne touche ni SDL ni le GPU : il se teste (et se réutilise) seul.
 *
 *     EngDebugLines lines;  eng_debug_lines_init(&lines, &arena, 16384);
 *     chaque image :
 *         eng_debug_lines_clear(&lines);
 *         eng_debug_aabb(&lines, min, max, ENG_DEBUG_YELLOW);
 *         eng_debug_lines_set_overlay(&lines, true);       // par-dessus tout, sans profondeur
 *         eng_debug_axes(&lines, transform, 1.0F);
 *         eng_debug_lines_set_overlay(&lines, false);
 *
 * Deux listes : lignes TESTÉES en profondeur (cachées par la géométrie, défaut) et lignes
 * SUPERPOSÉES (toujours visibles). Chaque liste a `capacity` lignes. Une forme tient en entier ou
 * n'est pas ajoutée du tout (jamais de boîte à moitié dessinée) ; les lignes perdues sont comptées
 * dans `dropped`. Une forme dont un paramètre n'est pas fini (NaN, infini) ou invalide (rayon
 * négatif) est refusée et comptée dans `rejected` : un NaN envoyé au GPU ne se verrait pas, il
 * ferait juste disparaître la ligne. Unités : mètres, repère du monde (docs/conventions.md). */

/* Couleur RGBA 8 bits (non prémultipliée), telle que lue par le GPU (EngDebugVertex). Le canal
 * alpha n'a pas d'effet pour l'instant : les lignes sont opaques. */
typedef struct EngDebugColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} EngDebugColor;

#define ENG_DEBUG_RGB(red, green, blue) ((EngDebugColor){(red), (green), (blue), 255})
#define ENG_DEBUG_WHITE ENG_DEBUG_RGB(255, 255, 255)
#define ENG_DEBUG_GRAY ENG_DEBUG_RGB(128, 128, 128)
#define ENG_DEBUG_RED ENG_DEBUG_RGB(255, 40, 40)
#define ENG_DEBUG_GREEN ENG_DEBUG_RGB(40, 255, 40)
#define ENG_DEBUG_BLUE ENG_DEBUG_RGB(60, 90, 255)
#define ENG_DEBUG_YELLOW ENG_DEBUG_RGB(255, 230, 40)
#define ENG_DEBUG_CYAN ENG_DEBUG_RGB(40, 230, 255)
#define ENG_DEBUG_MAGENTA ENG_DEBUG_RGB(255, 60, 230)

/* Sommet envoyé au GPU : 16 octets. Position : TEXCOORD0 (float3) ; couleur : TEXCOORD1
 * (ENG_VERTEX_UBYTE4_NORM). */
typedef struct EngDebugVertex {
    float position[3];
    EngDebugColor color;
} EngDebugVertex;

#define ENG_DEBUG_CIRCLE_SEGMENTS 32U  /* segments par défaut (paramètre 0) */
#define ENG_DEBUG_MIN_SEGMENTS 3U      /* plancher : un triangle */
#define ENG_DEBUG_MAX_SEGMENTS 256U    /* plafond */
#define ENG_DEBUG_MAX_GRID_CELLS 1024U /* demi-largeur maximale d'une grille, en cases */
#define ENG_DEBUG_GRID_MAJOR_EVERY 10  /* une ligne « majeure » toutes les 10 cases */
#define ENG_DEBUG_MAX_LINES (1U << 22) /* capacité maximale d'une liste */

typedef struct EngDebugLines {
    EngDebugVertex *depth;   /* 2 sommets par ligne, lignes testées en profondeur */
    EngDebugVertex *overlay; /* lignes superposées */
    uint32_t depth_count;    /* en SOMMETS (2 par ligne) */
    uint32_t overlay_count;
    uint32_t capacity; /* lignes par liste */
    uint32_t dropped;  /* lignes perdues : liste pleine, depuis le dernier clear */
    uint32_t rejected; /* formes refusées : paramètres invalides, depuis le dernier clear */
    bool overlay_mode; /* les prochaines formes vont dans la liste superposée */
} EngDebugLines;

/* Alloue deux listes de `max_lines` lignes dans `arena` (2 × max_lines × 32 octets). false si
 * `max_lines` est nul ou dépasse ENG_DEBUG_MAX_LINES, ou si l'arène est trop petite. */
bool eng_debug_lines_init(EngDebugLines *lines, EngArena *arena, uint32_t max_lines);

/* Vide les deux listes et remet `dropped` et `rejected` à zéro ; le mode (superposé ou non) est
 * conservé. À appeler au début de chaque image. */
void eng_debug_lines_clear(EngDebugLines *lines);

/* true : les formes suivantes sont superposées (visibles à travers tout) ; false : testées en
 * profondeur (défaut). */
void eng_debug_lines_set_overlay(EngDebugLines *lines, bool overlay);

/* Nombre de lignes de la liste testée / superposée / des deux. */
uint32_t eng_debug_lines_depth_count(const EngDebugLines *lines);
uint32_t eng_debug_lines_overlay_count(const EngDebugLines *lines);
uint32_t eng_debug_lines_count(const EngDebugLines *lines);

/* ── Formes ── Toutes retournent true si la forme a été ajoutée en entier. */

bool eng_debug_line(EngDebugLines *lines, const vec3 from, const vec3 to, EngDebugColor color);

/* Boîte alignée sur les axes, de `min` à `max` (12 lignes). Pas d'ordre imposé : si min > max sur
 * un axe, la boîte est simplement la même. */
bool eng_debug_aabb(EngDebugLines *lines, const vec3 min, const vec3 max, EngDebugColor color);

/* Boîte orientée : le cube [-half, +half]³ (demi-dimensions `half`, positives ou nulles) transformé
 * par `transform` (12 lignes). Une échelle dans `transform` se compose avec `half`. */
bool eng_debug_box(EngDebugLines *lines, mat4 transform, const vec3 half, EngDebugColor color);

/* Cercle de centre `center` dans le plan engendré par `axis_u` et `axis_v` (vecteurs DÉJÀ mis à
 * l'échelle du rayon : le point d'angle t est center + u·cos t + v·sin t). `segments` : 0 =
 * ENG_DEBUG_CIRCLE_SEGMENTS, sinon borné à [3, 256]. `segments` lignes. */
bool eng_debug_circle(EngDebugLines *lines, const vec3 center, const vec3 axis_u, const vec3 axis_v,
                      EngDebugColor color, uint32_t segments);

/* Sphère : trois cercles perpendiculaires (plans XY, YZ, XZ), 3 × segments lignes. Rayon négatif :
 * refusée. */
bool eng_debug_sphere(EngDebugLines *lines, const vec3 center, float radius, EngDebugColor color,
                      uint32_t segments);

/* Repère de `transform` : trois lignes depuis son origine, X en rouge, Y en vert, Z en bleu,
 * longues de `size` fois la longueur de chaque axe de la matrice (3 lignes). */
bool eng_debug_axes(EngDebugLines *lines, mat4 transform, float size);

/* Grille horizontale (plan XZ) à la hauteur de `center`, de (2 × cells + 1) lignes dans chaque
 * direction, espacées de `cell_size`, centrée sur `center`. La ligne d'indice i (de -cells à
 * +cells) est tracée en `major_color` si i est multiple de ENG_DEBUG_GRID_MAJOR_EVERY. `cells` est
 * borné par ENG_DEBUG_MAX_GRID_CELLS (au-delà : refusée), `cell_size` doit être > 0. */
bool eng_debug_grid(EngDebugLines *lines, const vec3 center, uint32_t cells, float cell_size,
                    EngDebugColor color, EngDebugColor major_color);

/* Flèche de `from` à `to` : la ligne et quatre barbes à l'extrémité (5 lignes), longues de 15 % de
 * la flèche. Flèche de longueur nulle : la ligne seule. */
bool eng_debug_arrow(EngDebugLines *lines, const vec3 from, const vec3 to, EngDebugColor color);

/* Frustum (volume visible) d'une matrice `view_proj` (proj × vue) : ses 8 coins reconstruits par
 * l'inverse (12 lignes). Refusé si la matrice n'est pas inversible. */
bool eng_debug_frustum(EngDebugLines *lines, mat4 view_proj, EngDebugColor color);
