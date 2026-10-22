/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Création et destruction de l'application.
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "roster.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_scenes.h"
#include "gui_types.h"
#include "app_interne.h"

int creer_app(app_t *app)
{
    sfContextSettings reglages = {0, 0, 8, 1, 1, sfContextDefault, sfFalse};
    sfVideoMode mode = {LARGEUR, HAUTEUR, 32};

    memset(app, 0, sizeof(app_t));
    roster_init(&app->equipe);
    srand(time(NULL));
    app->fenetre = sfRenderWindow_create(mode, "Redstrings",
        sfTitlebar | sfClose, &reglages);
    if (app->fenetre == NULL)
        return 1;
    sfRenderWindow_setVerticalSyncEnabled(app->fenetre, sfTrue);
    if (app_creer_ressources(app) != 0)
        return 1;
    app->additif.blendMode = sfBlendAdd;
    app->additif.transform = sfTransform_Identity;
    app->additif.texture = NULL;
    app->additif.shader = NULL;
    app_init_poussieres(app);
    app->alpha = 1.f;
    app->t_transition = -1.f;
    app->ouvert = 1;
    app->scene = SCENE_MENU;
    entrer_menu(app);
    return 0;
}

void detruire_app(app_t *app)
{
    if (app->fenetre)
        sfRenderWindow_destroy(app->fenetre);
    for (int i = 0; i < NB_POLICES; i++) {
        if (app->polices[i])
            sfFont_destroy(app->polices[i]);
    }
    for (int i = 0; i < NB_CURSEURS; i++) {
        if (app->curseurs[i])
            sfCursor_destroy(app->curseurs[i]);
    }
    if (app->texte)
        sfText_destroy(app->texte);
    if (app->forme)
        sfConvexShape_destroy(app->forme);
    if (app->cercle)
        sfCircleShape_destroy(app->cercle);
    if (app->sprite)
        sfSprite_destroy(app->sprite);
    if (app->tex_lueur)
        sfTexture_destroy(app->tex_lueur);
    if (app->tex_vignette)
        sfTexture_destroy(app->tex_vignette);
    if (app->horloge)
        sfClock_destroy(app->horloge);
    if (app->vue)
        sfView_destroy(app->vue);
    roster_liberer(&app->equipe);
}
