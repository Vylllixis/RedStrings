/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Pastilles et notifications empilables (toasts).
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_ui.h"

#define DUREE_TOAST 2.8f

/** Pastille (badge) centrée sur `pos`. Renvoie sa largeur. */
float pastille(app_t *app, const char *s, sfVector2f pos, sfColor c, int plein)
{
    float w = 0.f;

    app->espacement = 1.7f;
    w = largeur_texte(app, POLICE_GRAS, s, 12) + 26.f;
    rect_arrondi_bord(app, (sfFloatRect){pos.x - w / 2.f, pos.y - 13.f, w, 26.f}, 13.f,
        plein ? c : avec_alpha(c, 0.14f), plein ? C_TRANSPARENT : avec_alpha(c, 0.45f));
    texte(app, POLICE_GRAS, s, 12, pos, plein ? C_BLANC : melanger(c, C_BLANC, 0.3f),
        ALIGNE_CENTRE);
    app->espacement = 0.f;
    return w;
}

void notifier(app_t *app, const char *icone, const char *message, sfColor c)
{
    toast_t *t = &app->toasts[0];

    memmove(&app->toasts[1], &app->toasts[0], sizeof(toast_t) * (MAX_TOASTS - 1));
    snprintf(t->message, sizeof(t->message), "%s", message);
    snprintf(t->icone, sizeof(t->icone), "%s", icone);
    t->couleur = c;
    t->t = 0.f;
    t->y = -50.f;
    t->actif = 1;
}

static void dessiner_toast(app_t *app, toast_t *t)
{
    float f = 1.f - progression(t->t, DUREE_TOAST - 0.35f, 0.35f);
    float w = largeur_texte(app, POLICE_NORMALE, t->message, 16) + 80.f;
    sfFloatRect r = {LARGEUR / 2.f - w / 2.f, t->y - 14.f * (1.f - f), w, 46.f};
    float cy = r.top + r.height / 2.f;

    app->alpha = fminf(progression(t->t, 0.f, 0.2f), f);
    ombre_biseau(app, r, 12.f, 1.2f);
    panneau(app, r, 12.f, melanger((sfColor){24, 28, 50, 250}, t->couleur, 0.12f),
        (sfColor){16, 19, 36, 250}, avec_alpha(t->couleur, 0.55f));
    lueur(app, (sfVector2f){r.left + 26.f, cy}, (sfVector2f){40.f, 40.f},
        avec_alpha(t->couleur, 0.3f));
    cercle(app, (sfVector2f){r.left + 26.f, cy}, 14.f, avec_alpha(t->couleur, 0.2f),
        C_TRANSPARENT, 0.f);
    texte(app, POLICE_SYMBOLES, t->icone, 14, (sfVector2f){r.left + 26.f, cy},
        t->couleur, ALIGNE_CENTRE);
    texte(app, POLICE_NORMALE, t->message, 16, (sfVector2f){r.left + 50.f, cy},
        C_TEXTE, ALIGNE_GAUCHE);
    rect_arrondi(app, (sfFloatRect){r.left + 12.f, r.top + r.height - 2.f,
        (w - 24.f) * (1.f - t->t / DUREE_TOAST), 2.f}, 0.f,
        avec_alpha(t->couleur, 0.8f));
    app->alpha = 1.f;
}

void dessiner_toasts(app_t *app)
{
    toast_t *t = NULL;

    for (int i = MAX_TOASTS - 1; i >= 0; i--) {
        t = &app->toasts[i];
        if (!t->actif)
            continue;
        t->t += app->dt;
        if (t->t > DUREE_TOAST) {
            t->actif = 0;
            continue;
        }
        t->y = approcher(t->y, 24.f + i * 56.f, 13.f, app->dt);
        dessiner_toast(app, t);
    }
}
