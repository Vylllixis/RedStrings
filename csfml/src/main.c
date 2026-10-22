/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ main.c ─────────────────────────────────────────────────────────────
** │ But         Point d'entrée de la version graphique : lit l'argument
** │             éventuel, crée l'application, puis boucle jusqu'à la
** │             fermeture de la fenêtre.
** │ Contenu     main
** │ Code retour 0 si tout va bien, 84 en cas d'erreur
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <stdlib.h>
#include "gui_app.h"
#include "gui_jeu.h"
#include "gui_types.h"

/*
** ╭─ main ───────────────────────────────────────────────────────────────
** │ 1. mode_depuis_arguments() : aucun argument = mode normal, «
** │    Yu-Gi-Oh! » = mode cartes, autre chose = erreur
** │ 2. malloc de l'application, puis creer_app() : les deux sont vérifiés
** │ 3. boucle : evenements(), maj_app() avec le temps écoulé depuis
** │    l'image précédente, puis affichage de l'image
** │ 4. detruire_app() et free() : tout est libéré avant de sortir
** ╰──────────────────────────────────────────────────────────────────────
*/
int main(int argc, char **argv)
{
    mode_visuel_t mode = MODE_NORMAL;
    app_t *app = NULL;

    if (mode_depuis_arguments(argc, argv, &mode) != 0) {
        fprintf(stderr, "redstrings_csfml : argument inconnu « %s »\nusage : %s\n",
            argv[1], argv[0]);
        return 84;
    }
    app = malloc(sizeof(app_t));
    if (app == NULL)
        return 84;
    if (creer_app(app, mode) != 0) {
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
