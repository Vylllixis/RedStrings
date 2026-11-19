#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/instancing.h"
#include "engine/model.h"
#include "engine/renderer.h"

/* Herbe animée (M3-09) : des brins instanciés (engine/instancing.h), un maillage de brin partagé,
 * le vent et la disparition à distance faits dans le vertex shader `grass.vert`. Un champ d'herbe
 * dense = un tableau d'EngInstance (engine/placement.h : eng_placement_grass_rule) et UN dessin.
 *
 * Modèle (docs/grass.md) :
 *   - le brin a 1 m de haut dans le repère du modèle ; l'échelle de l'instance le grandit ;
 *   - le vent incline la pointe : décalage = direction x force x échelle x y² x onde, où y est la
 *     hauteur dans le brin (la base ne bouge pas, la pointe bouge le plus) et
 *     onde = 0,5 + 0,5 sin(2 pi f t + k <direction, position de l'instance>), dans [0, 1] : une
 *     vague qui traverse le champ ;
 *   - la distance d du brin à la caméra règle son échelle : 1 jusqu'à fade_start, 0 à fade_end
 *     (smoothstep) : le brin rapetisse puis disparaît (triangles dégénérés), sans à-coup. */

typedef struct EngGrassStyle {
    float wind_direction[2]; /* x, z : non nulle ; normalisée à l'envoi */
    float wind_strength;     /* mètres de déplacement de la pointe d'un brin d'échelle 1, >= 0 */
    float wind_frequency;    /* hertz, >= 0 */
    float wave_scale;        /* radians par mètre : phase de l'onde le long du vent, >= 0 */
    float fade_start;        /* mètres, >= 0 */
    float fade_end;          /* mètres, > fade_start */
} EngGrassStyle;

#define ENG_GRASS_UNIFORM_FLOATS 12U /* trois float4 après les uniforms de eng_instanced */

/* vent de 0,25 m vers +x, 0,35 Hz, onde de 0,4 rad/m ; brins visibles jusqu'à 60 m, disparus à 90 m
 */
EngGrassStyle eng_grass_style_default(void);
bool eng_grass_style_valid(const EngGrassStyle *style);

/* Remplit le bloc d'uniforms du vertex shader : [0..2] caméra, [3] fade_start, [4..5] direction
 * normalisée, [6] force, [7] onde, [8] phase = 2 pi f t, [9] fade_end, [10..11] 0. `time_seconds`
 * est reporté modulo la période pour garder la précision. Style invalide : false (journalisé). */
bool eng_grass_pack(const EngGrassStyle *style, const float camera[3], double time_seconds,
                    float out[ENG_GRASS_UNIFORM_FLOATS]);

/* Échelle de disparition (0 à 1) à la distance `distance` de la caméra. */
float eng_grass_fade(const EngGrassStyle *style, float distance);

/* Référence CPU du vertex shader : position monde du sommet `local` (repère du brin) de `instance`.
 * Mêmes entrées que le shader ; sert aux tests. */
void eng_grass_vertex_world(const EngGrassStyle *style, const float camera[3], double time_seconds,
                            const EngInstance *instance, const float local[3], float out[3]);

/* Brin d'herbe : un ruban de `segments` segments (1 à 8), large de `width` mètres à la base et
 * pointu, haut de 1 m, DEUX faces (une par côté, normales opposées) pour être visible des deux
 * côtés sans variante de culling. 2 x (2 x segments + 1) sommets, 2 x (2 x segments - 1) triangles.
 * Une partie, `material` (NULL : vert herbe mat). Paramètres invalides ou arène pleine : false
 * (journalisé), modèle à zéro, arène restituée. */
bool eng_grass_blade_build(EngArena *arena, uint32_t segments, float width,
                           const EngModelMaterial *material, EngModel *model);
EngModelMaterial eng_grass_default_material(void);
uint32_t eng_grass_blade_vertex_count(uint32_t segments);
uint32_t eng_grass_blade_triangle_count(uint32_t segments);

/* Pipeline des brins : eng_instanced_pipeline_desc avec le vertex shader grass.vert. */
void eng_grass_pipeline_desc(EngPipelineDesc *desc);
