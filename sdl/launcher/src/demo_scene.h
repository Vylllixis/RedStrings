#pragma once

#include <stdbool.h>

#include "engine/arena.h"
#include "engine/camera.h"
#include "engine/debug_lines.h"
#include "engine/debug_lines_gpu.h"
#include "engine/environment.h"
#include "engine/lighting.h"
#include "engine/model_gpu.h"
#include "engine/renderer.h"
#include "engine/shadow.h"

/* Scène de démonstration provisoire du lanceur : des cubes texturés et un modèle glTF, dessinés
 * par le renderer avec une matrice MVP (uniforms), une texture PNG avec mipmaps et un depth buffer.
 * Elle utilise l'API publique du renderer comme le fera le jeu. Le renderer doit avoir été créé
 * avec depth_buffer = true. */

typedef struct DemoScene {
    EngBufferHandle vertices;
    EngBufferHandle indices;
    EngTextureHandle texture;
    EngSamplerHandle sampler;
    EngPipelineHandle pipeline;
    /* Modèle glTF (assets/models/demo.gltf) : s'il n'a pas pu être chargé, has_model est faux et la
     * scène garde son cube central. */
    bool has_model;
    EngModelGpu model;
    EngPipelineHandle model_pipeline;
    /* Éclairage par image (M1-16) : ciel procédural, skybox et éclairage du modèle. Sans lui
     * (shader absent, échec de création), le modèle garde son ciel uniforme blanc et pas de
     * skybox. */
    bool has_environment;
    EngEnvironment environment;
    EngPipelineHandle sky_pipeline;
    /* Ombre du soleil (M1-17) : un sol qui reçoit les ombres des cubes et du modèle, une carte
     * d'ombre rendue avant l'image. Sans eux (shaders, format de profondeur, sol impossibles), la
     * scène est dessinée sans ombre. */
    bool has_ground;
    EngModelGpu ground;
    bool has_shadow;
    EngTextureHandle shadow_map;
    uint32_t shadow_size;
    EngPipelineHandle model_shadow_pipeline;
    EngPipelineHandle cube_shadow_pipeline;
    /* Debug draw (M1-11) : lignes accumulées à chaque image puis envoyées par eng_debug_lines_gpu.
     */
    bool has_lines;
    EngDebugLines lines;
    EngDebugLinesGpu lines_gpu;
} DemoScene;

/* Réglages de la démonstration (options --no-mipmaps et --anisotropy du lanceur) : permettent de
 * comparer le rendu avec et sans mipmaps, avec et sans anisotropie. */
typedef struct DemoOptions {
    bool mipmaps; /* chaîne de mipmaps complète + filtrage trilinéaire ; faux : niveau 0 seul */
    float anisotropy; /* 0 ou 1 : désactivée ; jusqu'à 16 */
    const char
        *environment_path; /* image équirectangulaire .hdr du ciel ; NULL : ciel procédural */
    uint32_t environment_samples; /* échantillons du préfiltrage ; 0 : défaut du moteur */
} DemoOptions;

#define DEMO_SHADOW_SIZE 2048U /* côté de la carte d'ombre, en texels */

/* `permanent` : arène des données du modèle GPU (vit autant que la scène) ; `level` : reçoit
 * temporairement le modèle côté CPU ; `scratch` : travail et lecture des fichiers. `level` et
 * `scratch` sont restituées dans leur état d'origine. */
bool demo_scene_init(DemoScene *scene, EngRenderer *renderer, EngArena *permanent, EngArena *level,
                     EngArena *scratch, const DemoOptions *options);

/* Dessine la scène vue par `camera` : une grille de 7 × 7 cubes espacés de 8 m (repères pour se
 * déplacer), au centre, le modèle glTF (un tore et une sphère) qui tourne sur lui-même.
 * `time_seconds` : temps de simulation (pas fixes + alpha), qui pilote la rotation ; `aspect` :
 * largeur / hauteur de la fenêtre. `lighting` : éclairage du modèle glTF, matériaux PBR (la
 * position de la caméra y est écrasée par celle de `camera`, pour le spéculaire) ; NULL : éclairage
 * par défaut. Les cubes texturés ne sont pas éclairés (shader textured.*). Le ciel (skybox) est
 * dessiné en premier ; l'intensité de l'ambiante de `lighting` est l'intensité de l'environnement
 * (ciel et éclairage du modèle).
 *
 * Ombre (M1-17, docs/shadows.md) : `shadow` non NULL, l'ombre du soleil est calculée avec ces
 * réglages : le sol et le modèle la reçoivent, les cubes, le modèle et le sol la projettent (la
 * file d'ombre du `renderer` est remplie, la carte est rendue par eng_renderer_end_frame). Les
 * cubes texturés, sans éclairage, ne reçoivent pas d'ombre. `shadow` NULL ou ombres indisponibles :
 * aucune passe d'ombre. La carte couvre la sphère de rayon 42 m autour de l'origine, qui contient
 * le sol (±28 m) et la grille de cubes. */
void demo_scene_draw(const DemoScene *scene, EngRenderer *renderer, EngRenderQueue *queue,
                     double time_seconds, const EngCamera *camera, float aspect,
                     const EngLighting *lighting, const EngShadowParams *shadow);
/* Debug draw de la scène (touche F3), À APPELER APRÈS demo_scene_draw dans la même image : grille
 * au sol (une ligne plus claire tous les 10 cases), repère du monde, la boîte orientée qui entoure
 * le modèle glTF et son repère, une sphère autour de chaque cube de la grille d'angle. Les lignes
 * testées en profondeur sont cachées par les cubes ; les repères sont superposés. Sans effet si le
 * debug draw n'a pas pu être créé. */
void demo_scene_draw_debug(DemoScene *scene, EngRenderer *renderer, EngRenderQueue *queue,
                           double time_seconds, const EngCamera *camera, float aspect);
void demo_scene_shutdown(DemoScene *scene, EngRenderer *renderer);
