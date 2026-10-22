/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Entrée et dessin de chaque scène.
 */

#ifndef GUI_SCENES_H
    #define GUI_SCENES_H

    #include "gui_types.h"

void entrer_menu(app_t *app);
void scene_menu(app_t *app);
void entrer_creation(app_t *app);
void scene_creation(app_t *app);
void entrer_liste(app_t *app);
void scene_liste(app_t *app);
void entrer_selection(app_t *app);
void scene_selection(app_t *app);
void entrer_arene(app_t *app);
void scene_arene(app_t *app);
void scene_au_revoir(app_t *app);

#endif /* GUI_SCENES_H */
