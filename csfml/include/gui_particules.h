/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Particules d'ambiance et d'impact.
 */

#ifndef GUI_PARTICULES_H
    #define GUI_PARTICULES_H

    #include <SFML/Graphics.h>
    #include "gui_types.h"

void emettre(app_t *app, sfVector2f pos, int n, sfColor c, float vitesse,
    float taille);
void maj_particules(app_t *app);
void dessiner_particules(app_t *app);

#endif /* GUI_PARTICULES_H */
