/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ evenements.c ───────────────────────────────────────────────────────
** │ But         Lecture des événements clavier et souris de la fenêtre.
** │ Contenu     redimensionner, touche_affichage (F11, Alt+Entrée,
** │             Maj+F11), traiter_evenement, evenements
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <string.h>
#include "gui_app.h"
#include "gui_config.h"
#include "gui_types.h"

/** Nouvelle taille ; en fenêtré, une fenêtre trop petite est ramenée au minimum. */
static void redimensionner(app_t *app, unsigned int l, unsigned int h)
{
    if (app->affichage == AFFICHAGE_FENETRE && (l < FENETRE_L_MIN || h < FENETRE_H_MIN)) {
        sfRenderWindow_setSize(app->fenetre, (sfVector2u){l < FENETRE_L_MIN
            ? FENETRE_L_MIN : l, h < FENETRE_H_MIN ? FENETRE_H_MIN : h});
        return;
    }
    appliquer_taille(app, l, h);
}

/** F11 et Alt+Entrée font la même chose ; Maj+F11 change le type de plein écran. */
static int touche_affichage(app_t *app, const sfKeyEvent *k)
{
    if (k->code == sfKeyF11 && k->shift) {
        changer_type_plein_ecran(app);
        return 1;
    }
    if (k->code == sfKeyF11 || (k->code == sfKeyEnter && k->alt)) {
        basculer_plein_ecran(app);
        return 1;
    }
    return 0;
}

static void traiter_evenement(app_t *app, sfEvent *e)
{
    if (e->type == sfEvtClosed)
        app->ouvert = 0;
    if (e->type == sfEvtResized)
        redimensionner(app, e->size.width, e->size.height);
    if (e->type == sfEvtKeyPressed && touche_affichage(app, &e->key))
        return;
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
    appliquer_demande_affichage(app);
    app->in.souris = sfRenderWindow_mapPixelToCoords(app->fenetre,
        sfMouse_getPositionRenderWindow(app->fenetre), app->vue_fixe);
    if (app->in.dehors)
        app->in.souris = (sfVector2f){-9999.f, -9999.f};
}
