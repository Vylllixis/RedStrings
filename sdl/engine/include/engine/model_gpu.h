#pragma once

#include <stdbool.h>

#include "engine/arena.h"
#include "engine/environment.h"
#include "engine/lighting.h"
#include "engine/math.h"
#include "engine/model.h"
#include "engine/renderer.h"
#include "engine/shadow.h"
#include "engine/skin.h"

/* Un EngModel (engine/model.h) envoyé au GPU et dessiné par le renderer : un buffer de sommets, un
 * buffer d'index (u32), une texture par image utilisée, un sampler, et une commande de dessin par
 * partie. Une fois créé, il ne dépend plus du modèle CPU (qu'on peut libérer).
 *
 * Shaders `model.vert` / `model.frag` (docs/shaders.md) : matériau PBR métal/rugosité (M1-15,
 * engine/pbr.h, docs/pbr.md) éclairé par le soleil et l'ambiante (engine/lighting.h). Les matériaux
 * sans texture utilisent une texture blanche de 1 x 1 : seul le facteur compte.
 *
 * Une image du modèle peut servir à plusieurs rôles ; le FORMAT dépend du rôle : couleur de base et
 * émissif sont des couleurs (sRGB, décodées par le GPU), métal/rugosité, normale et occlusion sont
 * des DONNÉES (linéaires, jamais décodées). Une image utilisée dans les deux familles donne donc
 * deux textures GPU. Les cinq textures d'un dessin occupent les emplacements 0 à 4 du fragment
 * shader, dans l'ordre de EngModelGpuMaterial ; les emplacements 5 à 7 sont l'environnement
 * (irradiance, préfiltrée, LUT BRDF : engine/environment.h). */

typedef struct EngModelGpuMaterial {
    float base_color[4];
    float metallic; /* facteurs du matériau (engine/model.h) */
    float roughness;
    float normal_scale;
    float occlusion_strength;
    float emissive[3];
    bool has_normal_map; /* sans normal map, la normale du sommet sert telle quelle */
    /* Les cinq textures sont TOUJOURS valides : celle de l'image, ou la blanche */
    EngTextureHandle texture;                    /* couleur de base (sRGB) */
    EngTextureHandle metallic_roughness_texture; /* G : rugosité, B : métal (linéaire) */
    EngTextureHandle normal_texture;             /* normale en espace tangent (linéaire) */
    EngTextureHandle occlusion_texture;          /* R : occlusion (linéaire) */
    EngTextureHandle emissive_texture;           /* émissif (sRGB) */
} EngModelGpuMaterial;

typedef struct EngModelGpu {
    EngTextureHandle *textures;        /* par image : version sRGB (nulle si non utilisée ainsi) */
    EngTextureHandle *linear_textures; /* par image : version linéaire (nulle si non utilisée) */
    EngModelPart *parts;
    EngModelGpuMaterial *materials;
    uint32_t texture_count;
    uint32_t part_count;
    uint32_t material_count;
    EngBufferHandle vertices;
    EngBufferHandle indices;
    EngSamplerHandle sampler;
    EngTextureHandle white;
    EngSamplerHandle shadow_sampler; /* plus proche voisin, bords prolongés (carte d'ombre) */
    float bounds_min[3];
    float bounds_max[3];
    EngEnvironment
        environment;       /* éclairage par image ; par défaut : environnement uniforme blanc */
    bool owns_environment; /* true : le modèle détruit `environment` (c'est le défaut) */
    bool skinned;          /* sommets de 56 octets : EngModelVertex + EngSkinVertex (M4-15) */
} EngModelGpu;

/* Décrit le pipeline qui dessine un EngModelGpu : sommets EngModelVertex (position, normale, UV,
 * tangente), shaders model.*,
 * profondeur activée, face arrière éliminée. L'appelant peut ajuster (nom, color_format...) puis le
 * créer avec eng_renderer_create_pipeline. */
void eng_model_pipeline_desc(EngPipelineDesc *desc);

/* Crée les ressources GPU. Les tableaux (parties, matériaux, handles) sont alloués dans `arena`
 * (aussi longtemps que le modèle GPU). `anisotropy` : filtrage anisotrope des textures (0 : aucun,
 * jusqu'à 16). Échec (false, journalisé) : tout ce qui avait été créé est détruit. */
bool eng_model_gpu_create(EngModelGpu *gpu, EngRenderer *renderer, EngArena *arena,
                          const EngModel *model, float anisotropy);

/* Comme eng_model_gpu_create, pour un personnage (M4-15) : `skin` (model->vertex_count éléments,
 * eng_model_parse_skinned) est entrelacé aux sommets (56 octets chacun : EngModelVertex puis
 * EngSkinVertex). Le modèle ne se dessine qu'avec les pipelines eng_model_skinned_pipeline_desc /
 * eng_model_skinned_shadow_pipeline_desc ET une palette de matrices (EngModelDrawOptions.
 * skin_matrices), sinon le dessin est refusé. `skin` NULL : échec. */
bool eng_model_gpu_create_skinned(EngModelGpu *gpu, EngRenderer *renderer, EngArena *arena,
                                  const EngModel *model, const EngSkinVertex *skin,
                                  float anisotropy);

/* Pipelines d'un modèle skinné : les mêmes que eng_model_pipeline_desc /
 * eng_model_shadow_pipeline_desc mais avec les shaders model_skinned.vert / shadow_skinned.vert
 * (docs/skinning.md) et le flux entrelacé de 56 octets. Le fragment shader est celui des modèles
 * ordinaires. */
void eng_model_skinned_pipeline_desc(EngPipelineDesc *desc);
void eng_model_skinned_shadow_pipeline_desc(EngPipelineDesc *desc);

/* Détruit les ressources. Sans effet sur un modèle GPU nul ou déjà détruit. */
void eng_model_gpu_destroy(EngModelGpu *gpu, EngRenderer *renderer);

/* Remplace l'environnement qui éclaire le modèle (le défaut, un ciel uniforme blanc, est détruit).
 * `env` reste à l'appelant : il doit vivre plus longtemps que le modèle GPU et n'est pas détruit
 * par lui. Refusé (false) si `env` n'est pas valide. L'intensité vient de l'ambiante de
 * l'éclairage (ambient_color × ambient_intensity) : 1 = l'environnement tel quel. */
bool eng_model_gpu_set_environment(EngModelGpu *gpu, EngRenderer *renderer,
                                   const EngEnvironment *env);

/* Met en file un dessin par partie. `model_matrix` : modèle → monde ; `view_proj` : monde →
 * découpage (colonne-majeur, comme mat4). `lighting` : éclairage de l'image (copié dans la file et
 * corrigé si nécessaire par eng_lighting_sanitize, `lighting` lui-même n'est pas modifié) ; NULL :
 * eng_lighting_default. Renvoie le nombre de dessins acceptés par la file (il
 * peut être inférieur au nombre de parties si la file est pleine : le renvoi est alors aussi compté
 * dans queue->dropped). */
uint32_t eng_model_gpu_draw(const EngModelGpu *gpu, EngRenderQueue *queue,
                            EngPipelineHandle pipeline, mat4 model_matrix, mat4 view_proj,
                            const EngLighting *lighting);

/* Ombre du soleil reçue par le modèle (M1-17, docs/shadows.md). `map` : la carte rendue par la
 * passe d'ombre (eng_renderer_create_shadow_map) ; `shadow` : la caméra du soleil qui l'a rendue
 * (eng_shadow_fit) ; `params` : biais, filtrage et force (corrigés par eng_shadow_params_sanitize
 * dans la copie envoyée au GPU). `size` de `shadow` doit être celui de la carte. */
#define ENG_MODEL_SHADOW_CASCADES 4U
typedef struct EngModelShadow {
    EngTextureHandle map;
    EngShadowMap shadow;
    EngShadowParams params;
    /* M7-09 : 2 à 4 cascades rangées dans un ATLAS 2 x 2 (`map` fait 2 x cascades[k].size de
     * côté ; cascade k dans le quart k, comme EngDrawCmd.shadow_tile = k + 1). Le shader prend,
     * pour chaque pixel, la cascade la plus fine qui le contient. 0 ou 1 : carte unique (`shadow`).
     */
    uint32_t cascade_count;
    EngShadowMap cascades[ENG_MODEL_SHADOW_CASCADES];
} EngModelShadow;

/* Comme eng_model_gpu_draw, avec l'ombre du soleil. `shadow` NULL, ou une carte nulle ou invalide :
 * aucune ombre (le soleil éclaire tout) ; un handle périmé fait rejeter les dessins par le
 * renderer. Seule la lumière DIRECTE du soleil est ombrée : l'éclairage par image et l'émissif ne
 * le sont pas. */
uint32_t eng_model_gpu_draw_shadowed(const EngModelGpu *gpu, EngRenderQueue *queue,
                                     EngPipelineHandle pipeline, mat4 model_matrix, mat4 view_proj,
                                     const EngLighting *lighting, const EngModelShadow *shadow);

/* Réglages d'un dessin particulier (terrain, M3-03) : tous facultatifs. */
typedef struct EngModelDrawOptions {
    /* Ajouté au facteur émissif de TOUTES les parties (linéaire, HDR ; M6 : arme d'un ennemi qui
     * s'allume pendant l'anticipation). Tout à 0 (défaut) : aucun effet. Multiplié par la texture
     * émissive du matériau s'il en a une (blanche sinon). */
    float emissive_add[3];
    /* Cinq handles qui REMPLACENT, pour toutes les parties, les cinq textures du matériau
     * (emplacements 0 à 4 du fragment shader) ; NULL : celles des matériaux. */
    const EngTextureHandle *textures;
    EngSamplerHandle sampler; /* sampler des emplacements 0 à 7 ; nul : celui du modèle */
    /* Trois valeurs libres pour le fragment shader (environment.yzw du cbuffer : le terrain y met
     * la taille d'une répétition de texture et la netteté du triplanaire). */
    float user[3];
    /* Parties à dessiner, dans cet ordre (indices < part_count du modèle ; hors limites : ignorés)
     * ; NULL : toutes. Sert au culling (M3-04) : seules les parties visibles sont mises en file. */
    const uint32_t *parts;
    uint32_t part_count; /* nombre d'éléments de `parts` (ignoré si NULL) */
    /* Flottants ajoutés APRÈS les uniforms d'éclairage de model.frag dans le bloc de fragment
     * (M3-06 : le brouillard du terrain) ; le fragment shader doit déclarer le cbuffer en
     * conséquence. NULL ou 0 : rien. Au plus ENG_MODEL_MAX_TAIL_FLOATS, arrondi vers le bas à un
     * multiple de 4 (des float4 entiers : le bloc de fragment est aligné sur 16 octets). */
    const float *fragment_tail;
    uint32_t fragment_tail_floats;
    /* Dessin INSTANCIÉ (M3-07, engine/instancing.h) : `instance_count` copies du maillage, placées
     * par le buffer `instance_buffer` (EngInstance), en un dessin par partie. Exige le pipeline de
     * eng_instanced_pipeline_desc. `model_matrix` est alors ignorée (chaque instance a la sienne).
     */
    EngBufferHandle instance_buffer;
    uint32_t instance_count;
    /* Flottants ajoutés APRÈS les uniforms de eng_instanced (view_proj, couleur) dans le bloc de
     * vertex d'un dessin INSTANCIÉ (M3-09 : vent et fondu de l'herbe, grass.vert) ; ignorés pour un
     * dessin non instancié. NULL ou 0 : rien. Au plus ENG_MODEL_MAX_VERTEX_TAIL_FLOATS, arrondi
     * vers le bas à un multiple de 4 (des float4 entiers). */
    const float *vertex_tail;
    uint32_t vertex_tail_floats;
    /* Palette d'un modèle SKINNÉ (M4-15) : `skin_joint_count` matrices 4x4 (16 flottants, colonnes
     * d'abord, contiguës) = eng_pose_skin_matrices, dans l'espace du personnage. Obligatoire pour
     * un modèle créé par eng_model_gpu_create_skinned (1 à ENG_SKELETON_MAX_JOINTS matrices, finies
     * ; sinon aucun dessin n'est mis en file) ; INTERDITE pour un modèle ordinaire. Seules
     * 3 lignes par articulation sont envoyées (la dernière est toujours 0 0 0 1). */
    const float *skin_matrices;
    uint32_t skin_joint_count;
} EngModelDrawOptions;
#define ENG_MODEL_MAX_TAIL_FLOATS 64U
#define ENG_MODEL_MAX_VERTEX_TAIL_FLOATS 16U

/* Comme eng_model_gpu_draw_shadowed, avec ces réglages (`options` NULL : aucun). */
uint32_t eng_model_gpu_draw_ex(const EngModelGpu *gpu, EngRenderQueue *queue,
                               EngPipelineHandle pipeline, mat4 model_matrix, mat4 view_proj,
                               const EngLighting *lighting, const EngModelShadow *shadow,
                               const EngModelDrawOptions *options);

/* Met en file un dessin par partie dans la PASSE D'OMBRE : `pipeline` est un pipeline depth_only
 * (eng_model_shadow_pipeline_desc), `light_view_proj` la matrice monde -> découpage du soleil
 * (EngShadowMap.view_proj). Renvoie le nombre de dessins acceptés. */
uint32_t eng_model_gpu_draw_depth(const EngModelGpu *gpu, EngRenderQueue *queue,
                                  EngPipelineHandle pipeline, mat4 model_matrix,
                                  mat4 light_view_proj);

/* Passe d'ombre d'un modèle skinné : comme eng_model_gpu_draw_depth, avec la palette (mêmes règles
 * que EngModelDrawOptions.skin_matrices). `pipeline` : eng_model_skinned_shadow_pipeline_desc. */
uint32_t eng_model_gpu_draw_depth_skinned(const EngModelGpu *gpu, EngRenderQueue *queue,
                                          EngPipelineHandle pipeline, mat4 model_matrix,
                                          mat4 light_view_proj, const float *skin_matrices,
                                          uint32_t skin_joint_count);

/* Pipeline de la passe d'ombre pour les sommets EngModelVertex (shadow.*, sans élimination de
 * faces : les modèles ouverts projettent leur ombre depuis les deux côtés). */
void eng_model_shadow_pipeline_desc(EngPipelineDesc *desc);
