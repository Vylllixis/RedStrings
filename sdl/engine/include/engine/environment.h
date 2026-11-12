#pragma once

#include <stdbool.h>

#include "engine/ibl.h"
#include "engine/math.h"
#include "engine/renderer.h"

/* Environnement GPU (M1-16) : les textures calculées par engine/ibl.h, prêtes pour le renderer.
 *  - sky         : cubemap RGBA16F avec ses mips, pour la skybox (model.frag ne la lit pas) ;
 *  - irradiance  : cubemap RGBA16F, irradiance/pi (1 niveau) ;
 *  - prefiltered : cubemap RGBA16F, un niveau de mip par rugosité (niveau k : k / (n - 1)) ;
 *  - brdf_lut    : RG16F 64 x 64, (A, B) (ligne : rugosité, colonne : n·v).
 * Orientation et conventions : docs/ibl.md. */

/* Valeurs par défaut : celles dont l'erreur a été mesurée contre une intégration par force brute
 * sur une image HDR réelle (docs/ibl.md). */
#define ENG_ENVIRONMENT_DEFAULT_SKY_SIZE 256U
#define ENG_ENVIRONMENT_DEFAULT_IRRADIANCE_SIZE 32U
#define ENG_ENVIRONMENT_DEFAULT_PREFILTERED_SIZE 128U
#define ENG_ENVIRONMENT_DEFAULT_SAMPLES 512U

typedef struct EngEnvironment {
    EngTextureHandle sky;
    EngTextureHandle irradiance;
    EngTextureHandle prefiltered;
    EngTextureHandle brdf_lut;
    EngSamplerHandle sampler; /* linéaire, trilinéaire, bords prolongés : pour la skybox */
    EngBufferHandle
        dummy_vertex; /* le renderer exige un buffer de sommets : la skybox n'en lit pas */
    uint32_t prefiltered_levels;
} EngEnvironment;

typedef struct EngEnvironmentDesc {
    /* source : l'image équirectangulaire si `equirect_rgb` n'est pas NULL, sinon `sky` (NULL :
     * eng_ibl_sky_default). */
    const EngSkyDesc *sky;
    const float *equirect_rgb; /* floats RGB, première ligne = haut */
    uint32_t equirect_width;
    uint32_t equirect_height;
    /* tailles (puissances de 2) ; 0 = valeur par défaut ci-dessus */
    uint32_t sky_size;
    uint32_t irradiance_size;
    uint32_t prefiltered_size;
    uint32_t samples;
} EngEnvironmentDesc;

/* Charge une image équirectangulaire Radiance (.hdr, RGBE) en floats RGB (3 par texel, première
 * ligne = haut) à passer dans EngEnvironmentDesc.equirect_rgb. Libérer avec
 * eng_environment_free_hdr. false (journalisé) si le fichier manque ou n'est pas lisible. */
bool eng_environment_load_hdr(const char *path, float **rgb, uint32_t *width, uint32_t *height);
void eng_environment_free_hdr(float *rgb);

/* Calcule (CPU, plusieurs centaines de ms) puis envoie. Échec (false, journalisé) : rien n'est
 * resté alloué. */
bool eng_environment_create(EngEnvironment *env, EngRenderer *renderer,
                            const EngEnvironmentDesc *desc);
/* Environnement uniforme de radiance `rgb` : cubes de 1 x 1. Avec rgb = (1, 1, 1) et une ambiante
 * de couleur c, le résultat est « une lumière ambiante uniforme c » (le comportement d'avant
 * M1-16, avec la LUT à la place de l'approximation de Karis). */
bool eng_environment_create_uniform(EngEnvironment *env, EngRenderer *renderer, const float rgb[3]);
void eng_environment_destroy(EngEnvironment *env, EngRenderer *renderer);
bool eng_environment_is_valid(const EngEnvironment *env, const EngRenderer *renderer);

/* Pipeline de la skybox : triangle plein écran, shaders sky.*, sans profondeur (à mettre en file
 * AVANT la scène : tout ce qui est dessiné ensuite la recouvre). */
void eng_sky_pipeline_desc(EngPipelineDesc *desc);

/* Met en file le dessin du ciel. `view_proj` : monde -> découpage (le même que celui de la
 * scène) ; `camera` : position de la caméra (monde) ; `intensity` : multiplicateur (>= 0).
 * Retourne true si la file l'a accepté. */
bool eng_environment_draw_sky(const EngEnvironment *env, EngRenderQueue *queue,
                              EngPipelineHandle pipeline, mat4 view_proj, const float camera[3],
                              float intensity);
