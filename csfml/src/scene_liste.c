/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

#define FICHE_L 520.f
#define FICHE_H 510.f

void entrerListe(app_t *app)
{
    liste_t *l = &app->liste;

    memset(l, 0, sizeof(liste_t));
    l->ouvert = -1;
    l->recherche.marge = 50.f;
}

static void ouvrir(liste_t *l, int id)
{
    l->ouvert = id;
    l->dernier = id;
    l->tModal = 0.f;
    memset(l->segments, 0, sizeof(l->segments));
}

static int filtrer(app_t *app, const char *recherche, int *ids)
{
    int n = 0;

    for (int i = 0; i < app->nbPersos; i++) {
        if (contientNom(app->persos[i].name, recherche))
            ids[n++] = i;
    }
    return n;
}

static void loupe(app_t *app, sfVector2f c, sfColor col)
{
    sfVector2f manche[2] = {{c.x + 3.5f, c.y + 3.5f}, {c.x + 9.f, c.y + 9.f}};

    cercle(app, (sfVector2f){c.x - 1.5f, c.y - 1.5f}, 7.5f, C_TRANSPARENT, col, 2.f);
    ligneEpaisse(app, manche, 2, 2.6f, col, 0);
}

/** Entrée dans la recherche : même logique que demanderPersonnage(). */
static void valider(app_t *app, liste_t *l, const int *ids, int n)
{
    int idx = trouverPersonnage(app->persos, app->nbPersos, l->recherche.texte);

    if (idx == -1 && n == 1)
        idx = ids[0];
    if (idx == -1) {
        notifier(app, "✗", "Personnage introuvable", C_ROUGE);
        l->recherche.secousse = 1.f;
        return;
    }
    ouvrir(l, idx);
}

static void naviguer(app_t *app, liste_t *l, int sens)
{
    ouvrir(l, (l->ouvert + sens + app->nbPersos) % app->nbPersos);
}

static void clavierModal(app_t *app, liste_t *l)
{
    if (app->in.touches[sfKeyEscape] || app->in.touches[sfKeyEnter])
        l->ouvert = -1;
    else if (app->in.touches[sfKeyLeft] && app->nbPersos > 1)
        naviguer(app, l, -1);
    else if (app->in.touches[sfKeyRight] && app->nbPersos > 1)
        naviguer(app, l, 1);
}

static int boutonsModal(app_t *app, liste_t *l, sfFloatRect r)
{
    int clic = 0;

    if (boutonRond(app, &l->fermer, (sfVector2f){r.left + r.width + 56.f, r.top + 24.f},
        "×")) {
        l->ouvert = -1;
        clic = 1;
    }
    if (app->nbPersos < 2)
        return clic;
    if (boutonRond(app, &l->precedent, (sfVector2f){r.left - 56.f, r.top
        + r.height / 2.f}, "‹")) {
        naviguer(app, l, -1);
        clic = 1;
    }
    if (boutonRond(app, &l->suivant, (sfVector2f){r.left + r.width + 56.f, r.top
        + r.height / 2.f}, "›")) {
        naviguer(app, l, 1);
        clic = 1;
    }
    return clic;
}

/** Fiche plein écran : équivalent de afficherPersonnage(). */
static void modal(app_t *app, liste_t *l, float a)
{
    int id = l->ouvert >= 0 ? l->ouvert : l->dernier;
    const int vals[3] = {app->persos[id].pv, app->persos[id].atk, app->persos[id].def};
    float m = 0.f;
    sfFloatRect r;
    char buf[32];

    l->modal = approcher(l->modal, l->ouvert >= 0 ? 1.f : 0.f, 12.f, app->dt);
    if (l->modal < 0.01f)
        return;
    l->tModal += app->dt;
    m = l->modal;
    app->alpha = a * m;
    rectArrondi(app, (sfFloatRect){-60.f, -60.f, LARGEUR + 120.f, HAUTEUR + 120.f}, 0.f,
        (sfColor){4, 5, 12, 215});
    r = (sfFloatRect){LARGEUR / 2.f - FICHE_L / 2.f, 92.f + 50.f * (1.f - m), FICHE_L,
        FICHE_H};
    lueur(app, (sfVector2f){LARGEUR / 2.f, r.top + r.height / 2.f}, (sfVector2f){520.f,
        420.f}, avecAlpha(couleurClasse(app->persos[id].classe), 0.12f));
    for (int i = 0; i < 3; i++)
        l->segments[i] = fminf((vals[i] + 4) / 5, fmaxf(0.f, (l->tModal - 0.15f
            - i * 0.12f) * 48.f));
    fiche(app, &app->persos[id], r, l->segments);
    snprintf(buf, sizeof(buf), "%d / %d", id + 1, app->nbPersos);
    texte(app, POLICE_GRAS, buf, 14, (sfVector2f){LARGEUR / 2.f, r.top + r.height + 32.f},
        C_TEXTE_DOUX, ALIGNE_CENTRE);
    texte(app, POLICE_NORMALE, "Échap pour fermer  ·  flèches pour changer de personnage",
        13, (sfVector2f){LARGEUR / 2.f, r.top + r.height + 58.f}, C_TEXTE_FAIBLE,
        ALIGNE_CENTRE);
    if (!boutonsModal(app, l, r) && app->in.clic && !app->sourisBloquee
        && !sfFloatRect_contains(&r, app->in.souris.x, app->in.souris.y))
        l->ouvert = -1;
    app->alpha = a;
}

static void recherche(app_t *app, liste_t *l, float a, int actif)
{
    float dy = 16.f * (1.f - sortieCubique(progression(app->tScene, 0.1f, 0.5f)));
    sfFloatRect r = {856.f, 96.f + dy, 360.f, 52.f};

    app->alpha = a * progression(app->tScene, 0.1f, 0.35f);
    l->recherche.focus = actif;
    champTexte(app, &l->recherche, r, "Rechercher un nom…", 29);
    loupe(app, (sfVector2f){r.left + 26.f, r.top + 26.f}, melanger(C_TEXTE_FAIBLE,
        C_CYAN, l->recherche.focusAnim));
    app->alpha = a;
}

void sceneListe(app_t *app)
{
    liste_t *l = &app->liste;
    int ids[MAX_PERSONNAGES];
    int bloque = app->sourisBloquee;
    int ouvert = l->ouvert >= 0;
    float a = app->alpha;
    char buf[96];
    int n = 0;
    int id = -1;

    if (ouvert)
        clavierModal(app, l);
    else if (app->in.touches[sfKeyEscape])
        changerScene(app, SCENE_MENU);
    app->sourisBloquee = bloque || ouvert || l->modal > 0.05f;
    snprintf(buf, sizeof(buf), "%d personnage%s  ·  clique sur une carte pour voir sa fiche",
        app->nbPersos, app->nbPersos > 1 ? "s" : "");
    enTete(app, &l->retour, "Personnages", buf);
    recherche(app, l, a, !ouvert);
    n = filtrer(app, l->recherche.texte, ids);
    if (!ouvert && app->in.touches[sfKeyEnter])
        valider(app, l, ids, n);
    id = grilleCartes(app, &l->grille, ids, n, 204.f, NULL, 0);
    if (id >= 0)
        ouvrir(l, id);
    if (n == 0) {
        snprintf(buf, sizeof(buf), "Aucun résultat pour « %s »", l->recherche.texte);
        texte(app, POLICE_NORMALE, buf, 18, (sfVector2f){LARGEUR / 2.f, 400.f},
            C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    }
    app->sourisBloquee = bloque || !ouvert || l->ouvert < 0;
    modal(app, l, a);
    app->sourisBloquee = bloque;
}
