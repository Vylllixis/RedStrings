/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Éléments partagés entre les fichiers du mode Yu-Gi-Oh! : à n'inclure que
 * depuis ce dossier.
 */

#ifndef YUGIOH_INTERNE_H
    #define YUGIOH_INTERNE_H

    #include <SFML/Graphics.h>
    #include "gui_types.h"
    #include "gui_yugioh.h"

    /* Or des liserés et des orbes, commun à la face et au dos. */
    #define OR_CADRE ((sfColor){232, 206, 128, 255})

void yugioh_degrade(app_t *app, sfFloatRect r, sfColor haut, sfColor bas);
void carte3d_bord_lumineux(app_t *app, const carte3d_t *k, float a);

#endif /* YUGIOH_INTERNE_H */
