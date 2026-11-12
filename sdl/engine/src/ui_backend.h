#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Couche interne entre le moteur (C) et les backends C++ de Dear ImGui (SDL3 pour les entrées,
 * SDL_GPU3 pour le rendu). Seul ui.c l'inclut. Un seul contexte ImGui existe à la fois. Compilée
 * seulement avec OW_UI (voir engine/CMakeLists.txt). */

#ifdef __cplusplus
extern "C" {
#endif

/* `window` NULL : mode sans fenêtre (tests) : ni événements ni taille automatiques, la taille et
 * les entrées sont données à la main. `device` : SDL_GPUDevice* ; `color_format` :
 * SDL_GPUTextureFormat de la cible où l'interface est dessinée. false (journalisé) en cas d'échec ;
 * tout est alors libéré. */
bool ow_ui_create(void *window, void *device, uint32_t color_format);
void ow_ui_destroy(void);

/* Un événement SDL (SDL_Event*). Retourne true si l'interface le veut pour elle (souris sur un
 * panneau, clavier dans un champ de saisie) : le jeu ne doit alors pas le voir. */
bool ow_ui_process_event(const void *sdl_event);

/* Commence une image d'interface. `width`/`height` : taille de l'affichage, pixels ; `dt` :
 * secondes depuis l'image précédente. */
void ow_ui_new_frame(int width, int height, float dt);
/* Termine l'image (ImGui::Render) : les données de dessin sont prêtes. Retourne le nombre de
 * sommets à dessiner. */
uint32_t ow_ui_render(void);
/* Envoi des sommets et textures au GPU : AVANT toute passe de rendu. */
void ow_ui_prepare(void *command_buffer);
/* Dessin dans une passe ouverte (SDL_GPURenderPass*). */
void ow_ui_draw(void *command_buffer, void *render_pass);

bool ow_ui_wants_mouse(void);
bool ow_ui_wants_keyboard(void);

/* Entrées synthétiques (tests, scripts) : position de la souris en pixels, bouton gauche. */
void ow_ui_add_mouse_pos(float x, float y);
void ow_ui_add_mouse_button(int button, bool down);

#ifdef __cplusplus
}
#endif
