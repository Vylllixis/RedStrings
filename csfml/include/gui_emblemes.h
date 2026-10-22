/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Emblèmes vectoriels des classes.
 */

#ifndef GUI_EMBLEMES_H
    #define GUI_EMBLEMES_H

    #include <SFML/Graphics.h>
    #include "personnage.h"
    #include "gui_types.h"

void embleme(app_t *app, classe_t c, sfVector2f centre, float taille,
    sfColor couleur);

#endif /* GUI_EMBLEMES_H */
