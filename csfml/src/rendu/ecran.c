/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Géométrie de l'écran : les scènes sont conçues dans un repère 1280 × 720
 * centré dans la fenêtre ; ces fonctions disent ce qui en est visible et
 * permettent d'ancrer un élément au bord ou d'en réduire une zone.
 */

#include "gui_app.h"
#include "gui_config.h"
#include "gui_ecran.h"
#include "gui_types.h"

#define DEBORD_VOILE 60.f

sfFloatRect ecran_visible(const app_t *app)
{
    return app->ecran;
}

sfFloatRect ecran_couvrant(const app_t *app)
{
    return (sfFloatRect){app->ecran.left - DEBORD_VOILE, app->ecran.top - DEBORD_VOILE,
        app->ecran.width + 2.f * DEBORD_VOILE, app->ecran.height + 2.f * DEBORD_VOILE};
}

float decal_gauche(const app_t *app)
{
    return app->ecran.left > 0.f ? app->ecran.left : 0.f;
}

float decal_droite(const app_t *app)
{
    float d = app->ecran.left + app->ecran.width - LARGEUR;

    return d < 0.f ? d : 0.f;
}

sfVector2f vers_ecran(const app_t *app, float x, float y)
{
    return (sfVector2f){app->ecran.left + x * app->ecran.width / LARGEUR,
        app->ecran.top + y * app->ecran.height / HAUTEUR};
}

void zoom_debut(app_t *app, sfVector2f pivot, float k)
{
    sfVector2f c = sfView_getCenter(app->vue);
    sfVector2f t = sfView_getSize(app->vue);

    if (k <= 0.f)
        k = 1.f;
    sfView_setCenter(app->vue_zoom, (sfVector2f){pivot.x + (c.x - pivot.x) / k,
        pivot.y + (c.y - pivot.y) / k});
    sfView_setSize(app->vue_zoom, (sfVector2f){t.x / k, t.y / k});
    sfRenderWindow_setView(app->fenetre, app->vue_zoom);
    app->souris_hors_zoom = app->in.souris;
    app->in.souris = (sfVector2f){pivot.x + (app->in.souris.x - pivot.x) / k,
        pivot.y + (app->in.souris.y - pivot.y) / k};
    app->zoom = k;
}

void zoom_fin(app_t *app)
{
    sfRenderWindow_setView(app->fenetre, app->vue);
    app->in.souris = app->souris_hors_zoom;
    app->zoom = 1.f;
}
