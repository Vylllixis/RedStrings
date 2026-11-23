#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "engine/debug_draw.h"
#include "engine/input.h"

/* Couche plateforme : fenêtre et événements. Cet en-tête n'expose pas SDL, pour que game/
 * puisse l'inclure sans dépendre de SDL. */

typedef struct EngWindowDesc {
    const char *title;
    int width;
    int height;
    bool resizable;
    bool fullscreen;
    bool vsync; /* demander la synchronisation verticale (affichage logiciel seulement) */
    /* Ne pas créer la surface logicielle : la fenêtre sera rattachée au GPU (une fenêtre ne peut
     * pas avoir à la fois une surface et une swapchain). Voir eng_platform_use_software_surface
     * pour revenir à l'affichage logiciel si le GPU est indisponible. */
    bool no_software_surface;
} EngWindowDesc;

/* Filtre d'événements (M1-12 : l'interface de debug les reçoit avant le jeu). Appelé pour CHAQUE
 * événement SDL (`sdl_event` : SDL_Event*, opaque ici). Retourne true si l'événement est consommé :
 * ni le jeu ni les touches système ne le voient. Les relâchements de touche et de bouton ne sont
 * jamais retirés au jeu (une touche ne reste pas « coincée »), même si le filtre retourne true. */
typedef bool (*EngEventHook)(void *user, const void *sdl_event);

typedef struct EngPlatform {
    struct SDL_Window *window;
    struct SDL_Gamepad *gamepad; /* manette ouverte, NULL si aucune */
    EngInput input;              /* entrées : voir input.h */
    int width; /* taille courante en pixels (pas en points : compte l'échelle HiDPI) */
    int height;
    bool fullscreen;
    bool vsync; /* synchronisation verticale obtenue (sinon : repli sur une petite pause) */
    bool mouse_captured; /* souris capturée (mode relatif : curseur caché, déplacements infinis) */
    bool debug_draw; /* affichage de debug (lignes, grille...) : F3 le bascule ; actif au départ */
    bool debug_ui; /* interface de debug (panneau Dear ImGui) : F1 la bascule ; masquée au départ */
    bool debug_freeze;       /* F5 : simulation gelée (débogage pas à pas du combat, M5-16) */
    uint32_t debug_steps;    /* F6 : pas à jouer pendant le gel (chaque pression en ajoute un) */
    EngEventHook event_hook; /* filtre d'événements, NULL si aucun */
    void *event_hook_user;
    bool quit_requested;
    /* Échap demande la fermeture (vrai au départ : outils sans menu). Le jeu, qui a un menu, le met
     * à faux : Échap y ouvre le menu ou revient en arrière (ENG_MENU_KEY_BACK), et la fenêtre ne
     * se ferme que par « Quitter », la croix de la fenêtre ou Alt+F4. */
    bool escape_quits;
    bool text_input;   /* saisie de texte active (eng_platform_set_text_input) */
    uint32_t text_len; /* octets en attente dans `text` */
    char text[64]; /* texte tapé depuis le dernier eng_platform_take_text : UTF-8, '\b' = effacer */
} EngPlatform;

/* Initialise SDL et ouvre la fenêtre. false en cas d'échec (message sur stderr). */
bool eng_platform_init(EngPlatform *p, const EngWindowDesc *desc);

/* Ferme la fenêtre et SDL. Sans effet sur une structure déjà arrêtée. */
void eng_platform_shutdown(EngPlatform *p);

/* Traite tous les événements en attente. Retourne false quand il faut quitter. */
bool eng_platform_pump(EngPlatform *p);

/* Crée la surface logicielle (fenêtre ouverte avec no_software_surface) et demande la
 * synchronisation verticale si `vsync`. À n'appeler que si le GPU n'est PAS rattaché. */
void eng_platform_use_software_surface(EngPlatform *p, bool vsync);

void eng_platform_toggle_fullscreen(EngPlatform *p);

/* Demande une taille de fenêtre (en points). Prise en compte par le système, donc à un rythme qui
 * lui est propre : la taille réelle se lit dans `width` et `height` après eng_platform_pump. false
 * si la fenêtre n'existe pas, si la taille n'est pas positive ou si SDL refuse. Sert aux tests de
 * redimensionnement et à l'option --stress-resize du lanceur. */
bool eng_platform_set_window_size(EngPlatform *p, int width, int height);

/* Capture la souris (mode relatif de SDL : curseur caché, déplacements sans limite d'écran) ou la
 * libère. Renvoie false si SDL refuse (mode non disponible) : l'état ne change alors pas. F2 la
 * bascule ; elle est aussi libérée quand la fenêtre perd le focus. */
bool eng_platform_set_mouse_capture(EngPlatform *p, bool captured);

/* Saisie de texte (nom d'une partie) : active, le texte tapé (caractères composés compris) et les
 * effacements (Retour arrière, répétition comprise) s'accumulent dans `text`, dans l'ordre.
 * eng_platform_take_text les copie dans `out` (terminé par 0, au plus `capacity` - 1 octets, sans
 * couper un caractère) et vide la mémoire ; retourne le nombre d'octets copiés. */
void eng_platform_set_text_input(EngPlatform *p, bool active);
uint32_t eng_platform_take_text(EngPlatform *p, char *out, uint32_t capacity);

/* Change le titre de la fenêtre (alloue : à appeler rarement, pas à chaque image). */
void eng_platform_set_title(EngPlatform *p, const char *title);

/* PROVISOIRE (jusqu'à M1) : dessine un fond uni et le carré demandé sur la surface logicielle
 * de la fenêtre. */
void eng_platform_present(EngPlatform *p, const EngDebugSquare *square);
