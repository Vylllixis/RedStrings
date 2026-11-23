#pragma once

#include <cglm/cglm.h>
#include <stdbool.h>
#include <stdint.h>

#include "engine/fog.h"
#include "engine/instancing.h"
#include "engine/model_gpu.h"
#include "engine/renderer.h"

/* Rendu des ARBRES RÉALISTES (2026-11-22, docs/arbres-realistes.md) : des modèles produits par
 * l'outil treebake (engine/tree_bake.h) à partir d'arbres scannés. Un tronc en maillage (écorce
 * photographiée, relief) et un feuillage en CARTES : des rectangles qui portent la photographie
 * d'un bouquet de feuilles, découpée par son alpha.
 *
 * Les arbres sont instanciés (engine/instancing.h) : un dessin par partie du modèle pour toutes les
 * copies d'une liste. Shaders tree.vert / tree.frag, et tree_shadow.* pour la passe d'ombre.
 *
 * Ce que ces shaders font de plus que instanced.vert / model.frag :
 *   - le VENT : le tronc plie (de plus en plus vers la cime), le bout des branches frémit ;
 *   - la DÉCOUPE : une partie dont le matériau a un seuil d'alpha (EngModelMaterial.alpha_cutoff,
 *     glTF « MASK ») ne dessine pas ses pixels transparents ; le seuil est TRAMÉ (il varie d'un
 *     pixel à l'autre, de plus en plus avec la distance) : un feuillage fin garde sa densité de
 *     loin au lieu de disparaître ; les cartes ont deux faces ;
 *   - la lumière dans le FEUILLAGE : elle « enveloppe » les feuilles (pas de face noire), les
 *     traverse (contre-jour vert-jaune), et le relief des cartes est mêlé à la forme d'ensemble
 *     du houppier pour qu'il se lise comme un volume ;
 *   - l'OMBRE du houppier sur lui-même : la part du ciel vue par chaque sommet, calculée par
 *     treebake et rangée dans |tangente.w| (le signe reste celui de la bitangente), assombrit
 *     l'ambiante et le soleil au cœur de l'arbre ; les branches intérieures sont sombres ;
 *   - l'ombre portée est DÉCOUPÉE elle aussi : le soleil passe entre les feuilles ;
 *   - le BROUILLARD du terrain (engine/fog.h) : les arbres lointains se fondent dans l'air.
 *
 * Un modèle sans matière découpée et dont les tangentes ont |w| = 1 est dessiné comme par
 * model.frag (aux deux faces près : rien n'est éliminé). */

/* Vent d'une image. */
typedef struct EngTreeWind {
    float direction[2]; /* x, z : non nulle ; normalisée à l'envoi */
    float strength;     /* mètres de déplacement de la cime d'un arbre de 10 m, >= 0 */
    float flutter;      /* mètres de frémissement au bout des branches, >= 0 */
    float time_s;       /* secondes */
} EngTreeWind;

/* Mesures du modèle, dans son repère (le pied du tronc en (0, 0, 0), Y vers le haut). */
typedef struct EngTreeShape {
    float height;  /* hauteur du modèle, > 0 */
    float crown_y; /* hauteur du centre du houppier */
    float radius;  /* portée horizontale des branches, > 0 */
} EngTreeShape;

/* Réglages de l'éclairage du feuillage (parties découpées) ; les deux occlusions valent aussi pour
 * le tronc. */
typedef struct EngTreeLook {
    float translucency;    /* lumière qui traverse les feuilles, de 0 (opaques) à 1 */
    float crown_normal;    /* part de la forme du houppier dans la normale des feuilles, 0 à 1 */
    float wrap;            /* lumière « enveloppante » : 0 (Lambert) à 1 */
    float sky_occlusion;   /* force de l'ombre du houppier sur l'ambiante, 0 à 1 */
    float sun_occlusion;   /* la même, sur la lumière directe du soleil */
    float dither_near;     /* largeur du tramage du seuil de découpe, de près (0 : seuil net) */
    float dither_far;      /* ... à `dither_distance` mètres et au-delà */
    float dither_distance; /* mètres, > 0 */
    float shadow_offset; /* mètres : une feuille ne reçoit que l'ombre de ce qui est plus loin */
    float edge_fade; /* |cos| de l'angle sous lequel une carte vue par la tranche réapparaît */
    float leaf_gain; /* éclat : facteur de la couleur des feuilles (photographiées sombres), > 0 */
    float leaf_reflection; /* part du reflet du ciel gardée sur les feuilles, de 0 à 1 */
    /* Densité de l'OMBRE du feuillage, de 0 à 1 : part des texels d'ombre écrits. Sous 1, le
     * sous-bois et le cœur du houppier gardent un peu de soleil (une pénombre, grâce au filtrage
     * de l'ombre). */
    float shadow_density;
} EngTreeLook;

#define ENG_TREE_VERTEX_FLOATS 8U /* deux float4 après les uniforms de eng_instanced */
#define ENG_TREE_LOOK_FLOATS 12U  /* trois float4 après les uniforms de model.frag */
/* puis le brouillard, comme le terrain */
#define ENG_TREE_FRAGMENT_FLOATS (ENG_TREE_LOOK_FLOATS + ENG_FOG_UNIFORM_FLOATS)

/* translucidité 0,6 ; houppier 0,45 ; enveloppe 0,35 ; occlusions 0,75 et 0,4 ; tramage de 0,15 à
 * 0,9 sur 60 m ; ombre décalée de 0,3 m ; fondu des cartes sous 0,25 ; éclat 1,45 ; reflet 0,35 ;
 * ombre dense à 0,72. */
EngTreeLook eng_tree_look_default(void);
/* Vent de 0,22 m vers +x, frémissement de 0,035 m, temps 0. */
EngTreeWind eng_tree_wind_default(void);
/* Mesures tirées de la boîte englobante du modèle : hauteur = sommet de la boîte, houppier centré
 * à `crown_height` (part de la hauteur, de 0 à 1), rayon = plus grand écart horizontal à l'axe. */
EngTreeShape eng_tree_shape_from_bounds(const float bounds_min[3], const float bounds_max[3],
                                        float crown_height);

/* Remplit les uniforms du vertex shader : [0..1] direction normalisée, [2] force, [3] temps,
 * [4] hauteur, [5] centre du houppier, [6] rayon, [7] frémissement. Les valeurs invalides (non
 * finies, négatives, direction nulle, hauteur ou rayon <= 0) sont remplacées par un vent nul ou
 * une forme d'un mètre : le dessin reste sain. */
void eng_tree_pack_vertex(const EngTreeShape *shape, const EngTreeWind *wind,
                          float out[ENG_TREE_VERTEX_FLOATS]);
/* Remplit les uniforms ajoutés au fragment shader : [0] translucidité, [1] houppier, [2] et [3]
 * occlusions (ciel, soleil), [4] à [6] tramage (près, loin, distance), [7] décalage de l'ombre,
 * [8] fondu des cartes, [9] enveloppe, [10] éclat, [11] reflet, chacun ramené dans son domaine ;
 * puis les ENG_FOG_UNIFORM_FLOATS flottants de `fog` (eng_fog_pack) ou des zéros (pas de
 * brouillard). `look` NULL : eng_tree_look_default. */
void eng_tree_pack_fragment(const EngTreeLook *look, const float *fog,
                            float out[ENG_TREE_FRAGMENT_FLOATS]);

/* Référence CPU du vertex shader : position monde du sommet `local` (repère du modèle) de
 * `instance`. `packed` : les uniforms d'eng_tree_pack_vertex. Sert aux tests. Formules :
 *   h      = clamp(y / hauteur, 0, 1)
 *   phase  = 0,37 x_instance + 0,23 z_instance
 *   rafale = 0,6 + 0,4 sin(0,7 t + phase)
 *   pli    = force x (hauteur x échelle / 10) x h² x rafale
 *   portée = clamp(distance à l'axe / rayon, 0, 1)
 *   onde   = sin(2,3 t + phase + 1,7 x + 2,9 y + 1,3 z)
 *   frémi  = frémissement x portée x h x onde x échelle
 *   monde  = rotation(local x échelle) + position + direction x (pli + 0,4 frémi) + Y x 0,6 frémi
 */
void eng_tree_vertex_world(const float packed[ENG_TREE_VERTEX_FLOATS], const EngInstance *instance,
                           const float local[3], float out[3]);

/* Référence CPU du tramage de tree.frag : bruit à gradient entrelacé (Jimenez, 2014) du pixel
 * (x, y), dans [0, 1[. Le seuil de découpe d'un pixel est seuil + (bruit - 0,5) x largeur. */
float eng_tree_dither(float pixel_x, float pixel_y);

/* Pipeline des arbres : comme eng_instanced_pipeline_desc, shaders tree.vert / tree.frag, AUCUNE
 * face éliminée (les cartes se voient des deux côtés). */
void eng_tree_pipeline_desc(EngPipelineDesc *desc);
/* Pipeline de la passe d'ombre des arbres : profondeur seule, shaders tree_shadow.*, sommets lus
 * aux emplacements 0 (position) et 1 (UV), instances aux emplacements 2 et 3. */
void eng_tree_shadow_pipeline_desc(EngPipelineDesc *desc);

/* Un dessin d'arbres : une liste d'instances du même modèle. */
typedef struct EngTreeDraw {
    EngBufferHandle instances; /* eng_instances_upload */
    uint32_t count;
    EngTreeShape shape;
    EngTreeWind wind;
    const EngTreeLook *look; /* NULL : eng_tree_look_default */
    const float
        *fog; /* ENG_FOG_UNIFORM_FLOATS flottants d'eng_fog_pack ; NULL : pas de brouillard */
    /* Parties à dessiner (indices < part_count du modèle ; les autres sont ignorés) : un modèle de
     * treebake range ses niveaux de détail en parties. NULL : toutes. */
    const uint32_t *parts;
    uint32_t part_count;
} EngTreeDraw;

/* Met en file un dessin par partie de `gpu` pour les instances de `draw` (pipeline
 * d'eng_tree_pipeline_desc). Mêmes éclairage et ombre que eng_model_gpu_draw_ex. Renvoie le nombre
 * de dessins acceptés ; 0 si `draw` est NULL ou vide. */
uint32_t eng_tree_draw(const EngModelGpu *gpu, EngRenderQueue *queue, EngPipelineHandle pipeline,
                       mat4 view_proj, const EngLighting *lighting, const EngModelShadow *shadow,
                       const EngTreeDraw *draw);

/* Met en file, dans la PASSE D'OMBRE, un dessin par partie (pipeline
 * d'eng_tree_shadow_pipeline_desc) : même vent que l'image, parties découpées par leur texture de
 * couleur. Renvoie le nombre de dessins acceptés (0 pour un modèle skinné, journalisé). */
uint32_t eng_tree_draw_depth(const EngModelGpu *gpu, EngRenderQueue *queue,
                             EngPipelineHandle pipeline, mat4 light_view_proj,
                             const EngTreeDraw *draw);
