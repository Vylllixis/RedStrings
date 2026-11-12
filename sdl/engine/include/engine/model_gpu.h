#pragma once

#include <stdbool.h>

#include "engine/arena.h"
#include "engine/environment.h"
#include "engine/lighting.h"
#include "engine/math.h"
#include "engine/model.h"
#include "engine/renderer.h"
#include "engine/shadow.h"

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
    EngBufferHandle vertices;
    EngBufferHandle indices;
    EngSamplerHandle sampler;
    EngTextureHandle white;
    EngSamplerHandle shadow_sampler; /* plus proche voisin, bords prolongés (carte d'ombre) */
    EngEnvironment
        environment;       /* éclairage par image ; par défaut : environnement uniforme blanc */
    bool owns_environment; /* true : le modèle détruit `environment` (c'est le défaut) */
    EngTextureHandle *textures;        /* par image : version sRGB (nulle si non utilisée ainsi) */
    EngTextureHandle *linear_textures; /* par image : version linéaire (nulle si non utilisée) */
    uint32_t texture_count;
    EngModelPart *parts;
    uint32_t part_count;
    EngModelGpuMaterial *materials;
    uint32_t material_count;
    float bounds_min[3];
    float bounds_max[3];
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
typedef struct EngModelShadow {
    EngTextureHandle map;
    EngShadowMap shadow;
    EngShadowParams params;
} EngModelShadow;

/* Comme eng_model_gpu_draw, avec l'ombre du soleil. `shadow` NULL, ou une carte nulle ou invalide :
 * aucune ombre (le soleil éclaire tout) ; un handle périmé fait rejeter les dessins par le
 * renderer. Seule la lumière DIRECTE du soleil est ombrée : l'éclairage par image et l'émissif ne
 * le sont pas. */
uint32_t eng_model_gpu_draw_shadowed(const EngModelGpu *gpu, EngRenderQueue *queue,
                                     EngPipelineHandle pipeline, mat4 model_matrix, mat4 view_proj,
                                     const EngLighting *lighting, const EngModelShadow *shadow);

/* Met en file un dessin par partie dans la PASSE D'OMBRE : `pipeline` est un pipeline depth_only
 * (eng_model_shadow_pipeline_desc), `light_view_proj` la matrice monde -> découpage du soleil
 * (EngShadowMap.view_proj). Renvoie le nombre de dessins acceptés. */
uint32_t eng_model_gpu_draw_depth(const EngModelGpu *gpu, EngRenderQueue *queue,
                                  EngPipelineHandle pipeline, mat4 model_matrix,
                                  mat4 light_view_proj);

/* Pipeline de la passe d'ombre pour les sommets EngModelVertex (shadow.*, sans élimination de
 * faces : les modèles ouverts projettent leur ombre depuis les deux côtés). */
void eng_model_shadow_pipeline_desc(EngPipelineDesc *desc);
