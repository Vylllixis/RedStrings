#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Couche plateforme : seul module (avec le futur renderer) qui parle à SDL. */

typedef struct EngWindowDesc {
    const char *title;
    int width;
    int height;
    bool resizable;
    bool fullscreen;
} EngWindowDesc;

typedef struct EngPlatform {
    struct SDL_Window *window;
    int width; /* taille courante en pixels (pas en points : compte l'échelle HiDPI) */
    int height;
    bool fullscreen;
    bool quit_requested;
} EngPlatform;

/* Initialise SDL et ouvre la fenêtre. false en cas d'échec (message sur stderr). */
bool eng_platform_init(EngPlatform *p, const EngWindowDesc *desc);

/* Ferme la fenêtre et SDL. Sans effet sur une structure déjà arrêtée. */
void eng_platform_shutdown(EngPlatform *p);

/* Traite tous les événements en attente. Retourne false quand il faut quitter. */
bool eng_platform_pump(EngPlatform *p);

void eng_platform_toggle_fullscreen(EngPlatform *p);

/* PROVISOIRE (jusqu'à M1) : dessine un fond uni et un carré mobile sur la surface
 * logicielle de la fenêtre, pour vérifier visuellement que la boucle tourne. */
void eng_platform_present(EngPlatform *p, uint64_t frame);
