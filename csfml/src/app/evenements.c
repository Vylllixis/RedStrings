/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Lecture des événements clavier et souris de la fenêtre.
 */

#include <string.h>
#include "gui_app.h"
#include "gui_config.h"
#include "gui_types.h"

static void traiter_evenement(app_t *app, sfEvent *e)
{
    if (e->type == sfEvtClosed)
        app->ouvert = 0;
    if (e->type == sfEvtKeyPressed && e->key.code >= 0 && e->key.code < sfKeyCount) {
        app->in.touches[e->key.code] = 1;
        app->in.shift = e->key.shift;
    }
    if (e->type == sfEvtTextEntered && e->text.unicode >= 32
        && e->text.unicode != 127 && app->in.nb_texte < MAX_SAISIE)
        app->in.texte[app->in.nb_texte++] = e->text.unicode;
    if (e->type == sfEvtMouseButtonPressed && e->mouseButton.button == sfMouseLeft) {
        app->in.clic = 1;
        app->in.bas = 1;
    }
    if ((e->type == sfEvtMouseButtonReleased && e->mouseButton.button == sfMouseLeft)
        || e->type == sfEvtLostFocus)
        app->in.bas = 0;
    if (e->type == sfEvtMouseLeft)
        app->in.dehors = 1;
    if (e->type == sfEvtMouseEntered || e->type == sfEvtMouseMoved)
        app->in.dehors = 0;
}

void evenements(app_t *app)
{
    sfEvent e;

    app->in.clic = 0;
    app->in.nb_texte = 0;
    memset(app->in.touches, 0, sizeof(app->in.touches));
    while (sfRenderWindow_pollEvent(app->fenetre, &e))
        traiter_evenement(app, &e);
    app->in.souris = sfRenderWindow_mapPixelToCoords(app->fenetre,
        sfMouse_getPositionRenderWindow(app->fenetre),
        sfRenderWindow_getDefaultView(app->fenetre));
    if (app->in.dehors)
        app->in.souris = (sfVector2f){-9999.f, -9999.f};
}
