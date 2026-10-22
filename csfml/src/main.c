/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdlib.h>
#include "gui_app.h"
#include "gui_types.h"

int main(void)
{
    app_t *app = malloc(sizeof(app_t));

    if (app == NULL)
        return 84;
    if (creer_app(app) != 0) {
        detruire_app(app);
        free(app);
        return 84;
    }
    while (app->ouvert && sfRenderWindow_isOpen(app->fenetre)) {
        evenements(app);
        maj_app(app, sfTime_asSeconds(sfClock_restart(app->horloge)));
        sfRenderWindow_display(app->fenetre);
    }
    detruire_app(app);
    free(app);
    return 0;
}
