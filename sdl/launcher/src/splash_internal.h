#ifndef LAUNCHER_SPLASH_INTERNAL_H
#define LAUNCHER_SPLASH_INTERNAL_H

/* Écran de chargement : ce que se partagent splash.c (animation), splash_build.c (ressources) et
 * splash_capture.c (images hors écran et mesures). */

#include <SDL3/SDL.h>

#include "engine/arena.h"
#include "splash.h"

typedef struct SplashUniforms { /* cbuffer Splash de splash.frag : cinq float4 */
    float screen[4];
    float anim[4];
    float title_rect[4];
    float label_rect[4];
    float text_info[4];
} SplashUniforms;

/* Le pipeline (triangle plein écran, splash.frag) et l'échantillonneur. */
bool splash_build_pipeline(Splash *s, EngGpu *gpu, EngArena *scratch);
/* Le titre et la légende en champ de distance, dans une image à un canal. */
bool splash_build_text(Splash *s, EngArena *scratch);
/* Uniforms de l'instant `time_s` pour une cible de `width` x `height` pixels. */
void splash_fill_uniforms(const Splash *s, SplashUniforms *u, double time_s, float progress,
                          float outro, uint32_t width, uint32_t height);
/* La scène dans `target` : un triangle plein écran. */
void splash_draw(const Splash *s, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target,
                 const SplashUniforms *uniforms);

#endif
