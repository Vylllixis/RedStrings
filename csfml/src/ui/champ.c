/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Champ de saisie de texte : caractères UTF-8, curseur, secousse en cas d'erreur.
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
#include "gui_utf8.h"

/** Ajoute les caractères tapés. L'espace est refusé (le nom sert d'identifiant). */
static void saisir(app_t *app, champ_t *c, int max)
{
    char u[4];
    int n = 0;
    int len = 0;
    int place = (int)sizeof(c->texte) - 1;

    if (max > place)
        max = place;
    for (int i = 0; i < app->in.nb_texte && i < MAX_SAISIE; i++) {
        n = app->in.texte[i] <= 32 ? 0 : utf8_encoder(app->in.texte[i], u);
        len = (int)strlen(c->texte);
        if (n == 0 || len + n > max) {
            c->secousse = 1.f;
            continue;
        }
        memcpy(c->texte + len, u, (size_t)n);
        c->texte[len + n] = '\0';
        c->tape = 0.f;
    }
    if (app->in.touches[sfKeyBackspace] && c->texte[0] != '\0') {
        utf8_supprimer_dernier(c->texte);
        c->tape = 1.f;
    }
}

static void dessiner_saisie(app_t *app, champ_t *c, float x0, float cy)
{
    char prefixe[sizeof(c->texte)];
    char dernier[sizeof(c->texte)];
    int nb = utf8_nb_caracteres(c->texte);
    float xd = x0 + position_caractere(app, POLICE_NORMALE, c->texte, 21, nb - 1);
    float xf = x0 + position_caractere(app, POLICE_NORMALE, c->texte, 21, nb);
    float e = sortie_rebond(progression(c->tape, 0.f, 0.24f));

    snprintf(prefixe, sizeof(prefixe), "%s", c->texte);
    utf8_supprimer_dernier(prefixe);
    snprintf(dernier, sizeof(dernier), "%s", c->texte + strlen(prefixe));
    texte(app, POLICE_NORMALE, prefixe, 21, (sfVector2f){x0, cy}, C_TEXTE,
        ALIGNE_GAUCHE);
    texte_echelle(app, POLICE_NORMALE, dernier, 21, (sfVector2f){(xd + xf) / 2.f,
        cy - 8.f * (1.f - e)}, melanger(C_CYAN, C_TEXTE, e), ALIGNE_CENTRE,
        lerpf(0.4f, 1.f, e));
    c->caret_x = approcher(c->caret_x, xf + 3.f, 30.f, app->dt);
}

int champ_texte(app_t *app, champ_t *c, sfFloatRect r, const char *aide,
    int max)
{
    int s = survole(app, r);
    float dx = 0.f;
    float cy = r.top + r.height / 2.f;
    float x0 = 0.f;
    float clignote = 0.f;
    char compteur[16];

    if (s)
        app->curseur_voulu = CURSEUR_TEXTE;
    if (c->focus)
        saisir(app, c, max);
    c->focus_anim = approcher(c->focus_anim, c->focus ? 1.f : 0.f, 12.f, app->dt);
    c->secousse = approcher(c->secousse, 0.f, 5.f, app->dt);
    c->tape += app->dt;
    dx = sinf(app->temps * 60.f) * 9.f * c->secousse;
    x0 = r.left + dx + (c->marge > 0.f ? c->marge : 20.f);
    lueur(app, (sfVector2f){r.left + r.width / 2.f, cy}, (sfVector2f){r.width * 0.6f,
        r.height * 1.6f}, avec_alpha(C_CYAN, 0.10f * c->focus_anim));
    panneau(app, (sfFloatRect){r.left + dx, r.top, r.width, r.height}, 12.f,
        melanger(C_CHAMP, (sfColor){18, 30, 52, 245}, c->focus_anim), C_CHAMP,
        melanger(melanger(C_BORD, avec_alpha(C_CYAN, 0.85f), c->focus_anim),
        C_ROUGE, c->secousse * 1.5f));
    rect_arrondi(app, (sfFloatRect){r.left + dx + 12.f, r.top + r.height - 2.f,
        (r.width - 24.f) * sortie_cubique(c->focus_anim), 2.f}, 0.f,
        avec_alpha(C_CYAN, 0.9f * c->focus_anim));
    if (c->texte[0] == '\0') {
        texte(app, POLICE_NORMALE, aide, 19, (sfVector2f){x0, cy}, C_TEXTE_FAIBLE,
            ALIGNE_GAUCHE);
        c->caret_x = approcher(c->caret_x, x0 - dx, 30.f, app->dt);
    } else {
        dessiner_saisie(app, c, x0 - dx, cy);
    }
    clignote = c->tape < 0.5f ? 1.f : 0.55f + 0.45f * cosf((c->tape - 0.5f) * 5.f);
    rect_arrondi(app, (sfFloatRect){c->caret_x + dx, cy - 13.f, 2.f, 26.f}, 1.f,
        avec_alpha(C_CYAN, c->focus_anim * clignote));
    snprintf(compteur, sizeof(compteur), "%d/%d", utf8_nb_caracteres(c->texte), max);
    texte(app, POLICE_NORMALE, compteur, 13, (sfVector2f){r.left + dx + r.width - 18.f,
        cy}, avec_alpha(C_TEXTE_FAIBLE, 0.4f + 0.6f * c->focus_anim), ALIGNE_DROITE);
    return s && app->in.clic;
}
