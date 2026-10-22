/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Création : onglets de catégorie et grille des classes.
 */

#include <math.h>
#include <stdio.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_emblemes.h"
#include "gui_etat.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_ui.h"
#include "creation_interne.h"

/* Grille de classes : 4 colonnes x 2 lignes, assez pour la plus grande catégorie. */
#define GRILLE_COLONNES 4

#define TUILE_H 38.f

#define TUILE_ECART 5.f

/** Onglets de catégorie : celui de la classe choisie est allumé. */
void creation_onglets(app_t *app, creation_t *c, float y)
{
    float w = (FW - TUILE_ECART * (NB_CATEGORIES - 1)) / NB_CATEGORIES;
    categorie_t actuelle = classe_info(c->classe)->categorie;
    sfFloatRect r;
    float h = 0.f;
    float on = 0.f;
    int nb = 0;
    char buf[48];

    for (int k = 0; k < NB_CATEGORIES; k++) {
        r = (sfFloatRect){FX + k * (w + TUILE_ECART), y, w, 26.f};
        if (survole(app, r)) {
            app->curseur_voulu = CURSEUR_MAIN;
            if (app->in.clic && (categorie_t)k != actuelle) {
                c->classe = creation_premiere_classe((categorie_t)k);
                c->focus = FOCUS_CLASSE;
            }
        }
        c->onglets[k].survol = approcher(c->onglets[k].survol, survole(app, r) ? 1.f
            : 0.f, 14.f, app->dt);
        c->onglets[k].appui = approcher(c->onglets[k].appui, (categorie_t)k == actuelle
            ? 1.f : 0.f, 14.f, app->dt);
        h = c->onglets[k].survol;
        on = c->onglets[k].appui;
        nb = 0;
        for (int i = 0; i < NB_CLASSES; i++)
            nb += classe_info(i)->categorie == (categorie_t)k;
        panneau(app, r, 8.f, avec_alpha(C_CYAN, 0.04f * h + 0.16f * on),
            avec_alpha(C_CYAN, 0.02f * h + 0.05f * on), melanger(avec_alpha(C_BLANC,
            0.08f + 0.08f * h), avec_alpha(C_CYAN, 0.8f), on));
        rect_arrondi(app, (sfFloatRect){r.left + 8.f, r.top + r.height - 2.f,
            (r.width - 16.f) * on, 2.f}, 0.f, C_CYAN);
        snprintf(buf, sizeof(buf), "%s  %d", nom_categorie((categorie_t)k), nb);
        texte(app, POLICE_GRAS, buf, 13, (sfVector2f){r.left + r.width / 2.f,
            r.top + 13.f}, melanger(C_TEXTE_FAIBLE, C_TEXTE, fmaxf(on, h * 0.6f)),
            ALIGNE_CENTRE);
    }
}

/** Tuile compacte d'une classe : emblème coloré, nom, état de sélection. */
static void tuile_classe(app_t *app, creation_t *c, classe_t i, sfFloatRect r)
{
    sfColor col = couleur_classe(i);
    float h = c->tuiles[i].survol;
    float sel = c->tuiles[i].appui;
    sfVector2f med = {r.left + 22.f, r.top + r.height / 2.f};
    float place = r.width - 48.f;
    unsigned int taille = 13;
    char nom[48];

    if (sel > 0.01f)
        lueur(app, (sfVector2f){r.left + r.width / 2.f, med.y}, (sfVector2f){r.width
            * 0.75f, 60.f}, avec_alpha(col, 0.22f * sel));
    panneau(app, r, 8.f, melanger(C_SURFACE_HAUTE, col, 0.06f * h + 0.18f * sel),
        melanger(C_SURFACE, C_FOND, 0.4f), melanger(melanger(C_BORD,
        avec_alpha(col, 0.55f), h), col, sel));
    rect_arrondi(app, (sfFloatRect){r.left, r.top + 8.f, 3.f, r.height - 8.f}, 0.f,
        avec_alpha(col, 0.35f + 0.65f * fmaxf(sel, h)));
    hexagone(app, med, 13.f + sel, 0.f, melanger((sfColor){14, 16, 32, 255}, col,
        0.15f + 0.2f * sel), avec_alpha(col, 0.5f + 0.5f * fmaxf(sel, h)));
    embleme(app, i, med, 8.f, melanger(col, C_BLANC, 0.2f + 0.3f * sel));
    while (taille > 11 && largeur_texte(app, POLICE_GRAS, classe_info(i)->nom, taille) > place)
        taille--;
    tronquer(app, POLICE_GRAS, taille, classe_info(i)->nom, place, nom, sizeof(nom));
    texte(app, POLICE_GRAS, nom, taille, (sfVector2f){r.left + 40.f, med.y},
        melanger(C_TEXTE_DOUX, melanger(col, C_BLANC, 0.55f), fmaxf(sel, h)),
        ALIGNE_GAUCHE);
    if (c->focus == FOCUS_CLASSE && c->classe == i)
        crochets(app, (sfFloatRect){r.left - 4.f, r.top - 4.f, r.width + 8.f,
            r.height + 8.f}, 10.f, avec_alpha(C_BLANC, 0.6f + 0.3f
            * sinf(app->temps * 5.f)));
}

/** Classes de la catégorie de la classe choisie, en grille. */
void creation_grille_classes(app_t *app, creation_t *c, float y)
{
    float w = (FW - TUILE_ECART * (GRILLE_COLONNES - 1)) / GRILLE_COLONNES;
    categorie_t cat = classe_info(c->classe)->categorie;
    sfFloatRect r;
    int n = 0;
    int s = 0;

    for (int i = 0; i < NB_CLASSES; i++) {
        c->tuiles[i].appui = approcher(c->tuiles[i].appui, c->classe == (classe_t)i
            ? 1.f : 0.f, 12.f, app->dt);
        if (classe_info(i)->categorie != cat)
            continue;
        r = (sfFloatRect){FX + (n % GRILLE_COLONNES) * (w + TUILE_ECART),
            y + (n / GRILLE_COLONNES) * (TUILE_H + TUILE_ECART), w, TUILE_H};
        s = survole(app, r);
        if (s)
            app->curseur_voulu = CURSEUR_MAIN;
        if (s && app->in.clic) {
            c->classe = (classe_t)i;
            c->focus = FOCUS_CLASSE;
        }
        c->tuiles[i].survol = approcher(c->tuiles[i].survol, s ? 1.f : 0.f, 14.f,
            app->dt);
        maj_ressort(&c->lift[i], s ? -2.f : 0.f, 300.f, 18.f, app->dt);
        r.top += c->lift[i].x;
        tuile_classe(app, c, (classe_t)i, r);
        n++;
    }
}
