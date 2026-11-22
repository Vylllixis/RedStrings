#ifndef LAUNCHER_TITLE_H
#define LAUNCHER_TITLE_H

/* Écran-titre (2026-11-22). Avant : le menu principal était dessiné par-dessus le jeu déjà lancé.
 * Maintenant : derrière le menu, c'est la scène animée de l'écran de chargement qui continue
 * (shaders/splash.frag.hlsl : l'aube, le chevalier, le fil rouge, le titre), au même instant, sans
 * coupure : la corde de chargement devient la barre du menu, le titre se range, et la scène se
 * recadre et s'assombrit selon l'écran ouvert (engine/menu_motion.h).
 *
 * La scène est dessinée dans la passe de l'interface, juste avant elle : le lanceur ne dessine pas
 * le jeu tant que l'écran-titre est affiché. Sans écran de chargement (--no-splash, tests), il n'y
 * a pas de scène : le menu s'affiche sur le jeu, assombri, comme avant. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/ui.h"
#include "game_flow.h"
#include "splash.h"

typedef struct Title {
    const Splash *scene; /* pipeline et titre de l'écran de chargement ; NULL : pas de scène */
    EngUi *ui;           /* l'interface, dessinée après la scène dans la même passe */
    double time_s;       /* temps de la scène : il continue celui de l'écran de chargement */
    float uniforms[32];  /* cbuffer de l'image en cours (SplashUniforms : huit float4) */
    uint64_t frames;     /* images où la scène a été dessinée */
    bool active;         /* la scène est dessinée cette image */
} Title;

/* `scene` : l'écran de chargement une fois fini (NULL ou pas prêt : pas de scène). */
void title_init(Title *t, const Splash *scene, EngUi *ui);
/* La scène est-elle disponible ? */
bool title_ready(const Title *t);
/* Une image : avance le temps de `dt` secondes et prépare la scène selon le menu. À appeler une
 * fois par image, avant la fin de l'image du renderer. `flow` NULL : pas de scène. */
void title_frame(Title *t, const GameFlow *flow, float dt, uint32_t width, uint32_t height);

/* Hooks pour eng_renderer_set_overlay (user = Title*) : la scène, puis l'interface. */
bool title_overlay_prepare(void *title, struct SDL_GPUCommandBuffer *commands);
void title_overlay_draw(void *title, struct SDL_GPUCommandBuffer *commands,
                        struct SDL_GPURenderPass *pass);

#endif
