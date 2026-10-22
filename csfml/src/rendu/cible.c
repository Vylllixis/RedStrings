/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Cible du dessin : la fenêtre, ou une texture quand app->cible est posée.
 * Toutes les primitives passent par ici, ce qui permet d'« imprimer » une
 * image une seule fois dans une texture (les faces des cartes, par exemple).
 */

#include "gui_app.h"
#include "gui_dessin.h"
#include "gui_types.h"

void cible_primitives(app_t *app, const sfVertex *v, size_t n, sfPrimitiveType type,
    const sfRenderStates *etats)
{
    if (app->cible != NULL)
        sfRenderTexture_drawPrimitives(app->cible, v, n, type, etats);
    else
        sfRenderWindow_drawPrimitives(app->fenetre, v, n, type, etats);
}

void cible_convexe(app_t *app, const sfConvexShape *forme, const sfRenderStates *etats)
{
    if (app->cible != NULL)
        sfRenderTexture_drawConvexShape(app->cible, forme, etats);
    else
        sfRenderWindow_drawConvexShape(app->fenetre, forme, etats);
}

void cible_cercle(app_t *app, const sfCircleShape *forme, const sfRenderStates *etats)
{
    if (app->cible != NULL)
        sfRenderTexture_drawCircleShape(app->cible, forme, etats);
    else
        sfRenderWindow_drawCircleShape(app->fenetre, forme, etats);
}

void cible_sprite(app_t *app, const sfSprite *sprite, const sfRenderStates *etats)
{
    if (app->cible != NULL)
        sfRenderTexture_drawSprite(app->cible, sprite, etats);
    else
        sfRenderWindow_drawSprite(app->fenetre, sprite, etats);
}

void cible_texte(app_t *app, const sfText *t, const sfRenderStates *etats)
{
    if (app->cible != NULL)
        sfRenderTexture_drawText(app->cible, t, etats);
    else
        sfRenderWindow_drawText(app->fenetre, t, etats);
}
