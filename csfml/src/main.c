/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdlib.h>
#include "gui.h"

int main(void)
{
    app_t *app = malloc(sizeof(app_t));

    if (app == NULL)
        return 84;
    if (creerApp(app) != 0) {
        detruireApp(app);
        free(app);
        return 84;
    }
    while (app->ouvert && sfRenderWindow_isOpen(app->fenetre)) {
        evenements(app);
        majApp(app, sfTime_asSeconds(sfClock_restart(app->horloge)));
        sfRenderWindow_display(app->fenetre);
    }
    detruireApp(app);
    free(app);
    return 0;
}
