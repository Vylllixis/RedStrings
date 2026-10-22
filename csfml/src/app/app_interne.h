/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Éléments partagés entre les fichiers de « app » : à n'inclure que depuis ce dossier.
 */

#ifndef APP_INTERNE_H
    #define APP_INTERNE_H

    #include "gui_app.h"
    #include "gui_types.h"

void app_init_poussieres(app_t *app);
int app_creer_ressources(app_t *app);

#endif /* APP_INTERNE_H */
