#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/arena.h"
#include "engine/image.h"

/* Modèles glTF 2.0 (M1-10), côté CPU : aucun GPU, donc testable sans fenêtre. Le chargement
 * aplatit le fichier en UN tableau de sommets, UN tableau d'index et une liste de « parties » (une
 * par primitive, un matériau chacune) : c'est ce que le renderer dessine (engine/model_gpu.h).
 *
 * Formats : .gltf (JSON, tampons externes ou en data: URI) et .glb. Pris en charge :
 *   - triangles (mode 4) avec POSITION ; NORMAL et TEXCOORD_0 facultatifs ; index u8/u16/u32 ou
 *     absents ; accessors normalisés ou « sparse » (lus par cgltf) ;
 *   - la hiérarchie des nœuds : la matrice MONDE de chaque nœud qui porte un maillage est
 *     appliquée aux sommets (et la matrice normale aux normales) au chargement. Un maillage
 *     utilisé par deux nœuds est donc copié deux fois. Tous les nœuds du fichier sont chargés,
 *     qu'ils soient dans la scène par défaut ou non ;
 *   - TANGENT (vec4) facultatif ; sinon les tangentes sont CALCULÉES à partir des UV (voir
 *     EngModelVertex) ;
 *   - matériaux PBR métal/rugosité (M1-15) : couleur de base, métal, rugosité, normale,
 *     occlusion, émissif (facteurs et textures, UV 0 seulement ; KHR_materials_emissive_strength).
 * Ignorés (avec un message) : modes autres que triangles, animations, skins, caméras, lumières,
 * autres extensions de matériau (clearcoat, transmission, KHR_texture_transform...), double face,
 * transparence. Refusés : données incohérentes (index hors
 * limites, tampon trop court, longueur d'index non multiple de 3), matrice de nœud singulière,
 * extensions obligatoires non gérées (compression Draco / meshopt, KHR_texture_basisu).
 *
 * Conventions : celles du projet (docs/conventions-maths.md), qui sont celles de glTF — main
 * droite, Y vers le haut, mètres, triangles anti-horaires ; UV (0, 0) en haut à gauche, v vers le
 * bas : aucune conversion. Une matrice de nœud qui inverse l'orientation (échelle négative) inverse
 * l'ordre des sommets des triangles pour qu'ils restent anti-horaires. */

typedef struct EngModelVertex {
    float position[3]; /* mètres, dans le repère du modèle (matrices de nœuds appliquées) */
    float normal[3];   /* unitaire (calculée par lissage si le fichier n'en a pas) */
    float uv[2];       /* (0, 0) si le fichier n'en a pas */
    /* Repère tangent pour les normal maps : xyz = tangente (unitaire, orthogonale à la normale,
     * dans le sens des u croissants), w = +1 ou -1, le signe de la bitangente :
     * bitangente = cross(normale, tangente) * w, en haut de l'image pour un UV non inversé (le v
     * de glTF va vers le bas). Lue dans le fichier si TANGENT existe, sinon calculée par sommet
     * (somme des tangentes des triangles, puis orthogonalisation) ; sans UV, un axe quelconque
     * orthogonal à la normale. */
    float tangent[4];
} EngModelVertex;

/* Matériau PBR métal/rugosité de glTF 2.0. Toutes les `*_image` sont des indices dans
 * EngModel.images, ou -1 : pas de texture. Rôles des textures : couleur de base et émissif sont en
 * sRGB, les autres en LINÉAIRE (le renderer crée la texture avec le bon format,
 * engine/model_gpu.h). Valeurs par défaut de la spécification (aussi pour un matériau sans bloc
 * pbrMetallicRoughness et pour le matériau 0) : couleur 1, métal 1, rugosité 1, rien d'émissif,
 * normal_scale et occlusion_strength à 1. */
typedef struct EngModelMaterial {
    float base_color[4]; /* facteur RGBA, linéaire (1, 1, 1, 1 par défaut) */
    int32_t image;   /* couleur de base (RGB sRGB, alpha) : indice dans EngModel.images, ou -1 */
    float metallic;  /* facteur, [0, 1] */
    float roughness; /* facteur, [0, 1] */
    int32_t
        metallic_roughness_image; /* rugosité dans G, métal dans B (linéaire) ; R et A ignorés */
    float normal_scale;           /* échelle des composantes x et y de la normale lue */
    int32_t normal_image;         /* normale en espace tangent, RGB = (x, y, z) * 0,5 + 0,5 */
    float occlusion_strength;     /* [0, 1] : 0 ignore la texture */
    int32_t occlusion_image;      /* occlusion ambiante dans R (linéaire) */
    float emissive[3];            /* facteur, linéaire, déjà multiplié par l'intensité (KHR) */
    int32_t emissive_image;       /* émissif (RGB sRGB) */
} EngModelMaterial;

typedef struct EngModelPart {
    uint32_t first_index; /* premier index de la partie dans EngModel.indices */
    uint32_t index_count; /* multiple de 3 */
    uint32_t material;    /* indice dans EngModel.materials */
} EngModelPart;

typedef struct EngModel {
    EngModelVertex *vertices;
    uint32_t vertex_count;
    uint32_t *indices; /* indices[i] < vertex_count ; indices GLOBAUX (pas relatifs à la partie) */
    uint32_t index_count;
    EngModelPart *parts;
    uint32_t part_count;
    /* materials[0] est le matériau par défaut (blanc, sans texture) des primitives sans matériau ;
     * le matériau glTF n est materials[n + 1]. */
    EngModelMaterial *materials;
    uint32_t material_count;
    /* Une entrée par image du fichier ; seules celles qu'un matériau utilise sont décodées (les
     * autres ont pixels == NULL). Propriété du modèle : eng_model_free. */
    EngImage *images;
    uint32_t image_count;
    float bounds_min[3]; /* boîte englobante des sommets */
    float bounds_max[3];
} EngModel;

#define ENG_MODEL_MAX_VERTICES (1U << 22)
#define ENG_MODEL_MAX_INDICES (1U << 24)
#define ENG_MODEL_MAX_PARTS 8192U

/* Analyse un fichier .gltf / .glb déjà en mémoire. Les tableaux du modèle sont alloués dans
 * `arena` (valides jusqu'à son reset) ; `scratch` sert aux calculs temporaires et à la lecture des
 * images externes (restituée dans son état d'origine). `base_path` : chemin du fichier d'origine,
 * pour retrouver les tampons et images externes ; NULL si tout est inclus dans les données (un
 * fichier externe est alors une erreur). Échec (false, journalisé) : `model` est mis à zéro,
 * `arena` restituée dans son état d'origine, rien n'est à libérer. */
bool eng_model_parse(EngArena *arena, EngArena *scratch, const void *data, size_t size,
                     const char *base_path, EngModel *model);

/* Lit puis analyse un fichier (chemin relatif à l'exécutable, voir engine/files.h). */
bool eng_model_load(EngArena *arena, EngArena *scratch, const char *path, EngModel *model);

/* Modèle procédural : une boîte alignée sur les axes (24 sommets, une normale par face, faces dans
 * le sens direct vu de l'extérieur), d'une seule partie. `material` : NULL pour le matériau par
 * défaut de glTF (blanc, métal 1, rugosité 1). Les tableaux sont alloués dans `arena`. Boîte vide
 * ou non numérique, ou arène trop petite : false (journalisé), `model` est mis à zéro et l'arène
 * restituée. Aucune image : eng_model_free est sans effet dessus. */
bool eng_model_make_box(EngArena *arena, const float box_min[3], const float box_max[3],
                        const EngModelMaterial *material, EngModel *model);

/* Libère les images décodées. Les tableaux appartiennent à l'arène. Sans effet sur un modèle nul
 * ou déjà libéré. */
void eng_model_free(EngModel *model);
