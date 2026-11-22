#ifndef ENG_UI_MENU_H
#define ENG_UI_MENU_H

/* Dessin des menus du jeu (2026-11-22), avec Dear ImGui (listes de dessin, pas de fenêtres). La
 * logique est dans game_menu, l'animation dans menu_motion : ce module ne fait que dessiner leur
 * état et dire quelle ligne la souris survole.
 *
 *   - menu principal : les lignes sont posées sur le fil rouge, en bas de l'écran ; une perle
 *     glisse le long du fil jusqu'à la ligne choisie, et le fil vibre quand on le pince ;
 *   - parties : trois cartes suspendues au fil (une vignette du lieu, le nom, le niveau, le temps
 *     de jeu, la date, les boss vaincus) ; la carte visée vient à gauche, ses actions à droite ;
 *   - pause, options, contrôles : une colonne de lignes (curseurs, bascules), qui défile.
 *
 * Tailles proportionnelles à la hauteur de l'écran (référence 1080 px) × l'échelle du texte. Sans
 * OW_UI : ne dessine rien. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/game_menu.h"
#include "engine/menu_motion.h"
#include "engine/ui.h"

typedef struct EngMenuView {
    int width;
    int height;
    float text_scale;
    bool high_contrast;
    /* la scène du titre est dessinée derrière (avec le titre du jeu) ; sinon c'est le jeu, que le
     * menu assombrit, et le titre est écrit ici */
    bool on_title;
} EngMenuView;

typedef struct EngMenuHover {
    int32_t item; /* ligne survolée par la souris, -1 si aucune */
    int32_t dir;  /* côté de sa valeur : -1 gauche, +1 droite, 0 ailleurs */
} EngMenuHover;

/* Dessine le menu (ou l'écran qui finit de sortir). `motion` reçoit le nombre de lignes visibles de
 * la liste, pour le défilement. Entre eng_ui_begin_frame et eng_ui_end_frame. */
EngMenuHover eng_ui_draw_menu(EngUi *ui, const EngMenu *menu, EngMenuMotion *motion,
                              const EngMenuView *view);

/* Sommets émis par le dernier eng_ui_draw_menu (tests : « quelque chose est dessiné »). */
uint32_t eng_ui_menu_vertices(void);

#endif
