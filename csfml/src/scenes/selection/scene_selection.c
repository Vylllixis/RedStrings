/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Composition des deux équipes avant un combat : d'abord la tienne, puis
 * celle de l'adversaire (jouée par l'ordinateur). Souris ou clavier.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "personnage.h"
#include "bataille.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_etat.h"
#include "gui_hud.h"
#include "gui_particules.h"
#include "gui_scenes.h"
#include "gui_types.h"
#include "gui_ui.h"

#define GRILLE_Y 196.f
#define PAR_LIGNE 5

static const char *BADGES[NB_EQUIPES][TAILLE_EQUIPE_MAX] = {
    {"ALLIÉ 1", "ALLIÉ 2", "ALLIÉ 3", "ALLIÉ 4"},
    {"ADVERSAIRE 1", "ADVERSAIRE 2", "ADVERSAIRE 3", "ADVERSAIRE 4"},
};

void entrer_selection(app_t *app)
{
    selection_t *s = &app->selection;

    memset(s, 0, sizeof(selection_t));
    s->depart = -1.f;
}

/** Rang du personnage `id` dans l'équipe `e`, ou -1. */
static int rang(const selection_t *s, int e, int id)
{
    for (int i = 0; i < s->nb[e]; i++) {
        if (s->ids[e][i] == id)
            return i;
    }
    return -1;
}

static int nb_libres(app_t *app, const selection_t *s)
{
    int n = 0;

    for (int i = 0; i < app->equipe.len; i++)
        n += !est_ko(&app->equipe.data[i]) && rang(s, 0, i) == -1 && rang(s, 1, i) == -1;
    return n;
}

/** Ajoute ou retire le personnage `id` de l'équipe en cours de composition. */
static void basculer(app_t *app, selection_t *s, int id)
{
    int e = s->etape;
    int r = rang(s, e, id);
    sfVector2f c = {s->grille.pos[id].x + CARTE_L / 2.f, s->grille.pos[id].y + 90.f};

    if (r != -1) {
        for (int i = r; i < s->nb[e] - 1; i++)
            s->ids[e][i] = s->ids[e][i + 1];
        s->nb[e]--;
        return;
    }
    if (est_ko(&app->equipe.data[id]))
        notifier(app, "✗", "Ce personnage est K.O.", C_ROUGE);
    else if (rang(s, 1 - e, id) != -1)
        notifier(app, "✗", "Il fait déjà partie de ton équipe", C_ROUGE);
    else if (s->nb[e] >= TAILLE_EQUIPE_MAX)
        notifier(app, "✗", "Une équipe compte 4 personnages au plus", C_ROUGE);
    else {
        s->ids[e][s->nb[e]++] = id;
        emettre(app, c, 26, e == 0 ? C_CYAN : C_ROUGE, 380.f, 3.f);
    }
}

static void valider(app_t *app, selection_t *s)
{
    if (s->nb[s->etape] == 0) {
        notifier(app, "✗", "Choisis au moins un personnage", C_ROUGE);
    } else if (s->etape == 0 && nb_libres(app, s) == 0) {
        notifier(app, "✗", "Laisse au moins un personnage à l'adversaire", C_ROUGE);
    } else if (s->etape == 0) {
        s->etape = 1;
        s->curseur = 0;
    } else {
        s->depart = 0.45f;
    }
}

static void clavier(app_t *app, selection_t *s)
{
    const int *k = app->in.touches;
    int n = app->equipe.len;

    if (k[sfKeyEscape] && s->etape == 1) {
        s->etape = 0;
        s->nb[1] = 0;
    } else if (k[sfKeyEscape]) {
        changer_scene(app, SCENE_MENU);
    }
    if (n <= 0)
        return;
    if (k[sfKeyRight])
        s->curseur = (s->curseur + 1) % n;
    if (k[sfKeyLeft])
        s->curseur = (s->curseur + n - 1) % n;
    if (k[sfKeyDown] && s->curseur + PAR_LIGNE < n)
        s->curseur += PAR_LIGNE;
    if (k[sfKeyUp] && s->curseur >= PAR_LIGNE)
        s->curseur -= PAR_LIGNE;
    if (k[sfKeySpace])
        basculer(app, s, s->curseur);
    if (k[sfKeyEnter])
        valider(app, s);
}

static void etape(app_t *app, sfVector2f c, const char *num, const char *label,
    sfColor col, float actif)
{
    lueur(app, c, (sfVector2f){44.f, 44.f}, avec_alpha(col, 0.35f * actif));
    hexagone(app, c, 15.f, 0.f, melanger(C_SURFACE_HAUTE, col, actif),
        avec_alpha(col, 0.4f + 0.6f * actif));
    texte(app, POLICE_GRAS, num, 13, c, actif > 0.5f ? C_BLANC : C_TEXTE_DOUX,
        ALIGNE_CENTRE);
    texte(app, POLICE_GRAS, label, 15, (sfVector2f){c.x + 26.f, c.y},
        melanger(C_TEXTE_FAIBLE, C_TEXTE, actif), ALIGNE_GAUCHE);
}

static void etapes(app_t *app, selection_t *s, float a)
{
    char buf[48];
    float p = 0.f;

    s->etape_anim = approcher(s->etape_anim, (float)s->etape, 10.f, app->dt);
    p = bornerf(s->etape_anim, 0.f, 1.f);
    app->alpha = a * progression(app->t_scene, 0.15f, 0.4f);
    rect_arrondi(app, (sfFloatRect){1012.f, 83.f, 46.f, 2.f}, 0.f,
        melanger(C_CYAN, C_ROUGE, p));
    snprintf(buf, sizeof(buf), "Ton équipe %d", s->nb[0]);
    etape(app, (sfVector2f){860.f, 84.f}, "1", buf, C_CYAN, 1.f - 0.5f * p);
    snprintf(buf, sizeof(buf), "Adversaires %d", s->nb[1]);
    etape(app, (sfVector2f){1080.f, 84.f}, "2", buf, C_ROUGE, 0.3f + 0.7f * p);
    if (bouton_plein(app, &s->valider, (sfFloatRect){856.f, 116.f, 360.f, 48.f},
        s->etape == 0 ? "Valider mon équipe" : "Lancer le combat", "Entrée",
        s->etape == 0 ? C_CYAN : C_ROUGE, s->nb[s->etape] > 0))
        valider(app, s);
    app->alpha = a;
}

/** Crochets du curseur clavier autour de la carte choisie. */
static void curseur(app_t *app, selection_t *s)
{
    sfVector2f p = s->grille.pos[s->curseur];
    sfColor c = s->etape == 0 ? C_CYAN : C_ROUGE;

    crochets(app, (sfFloatRect){p.x - 9.f, p.y + s->grille.lift[s->curseur].x - 9.f,
        CARTE_L + 18.f, CARTE_H + 18.f}, 22.f, avec_alpha(c, 0.65f + 0.35f
        * sinf(app->temps * 5.f)));
}

void scene_selection(app_t *app)
{
    selection_t *s = &app->selection;
    int ids[MAX_PERSONNAGES];
    marque_t marques[NB_EQUIPES * TAILLE_EQUIPE_MAX];
    int nb_marques = 0;
    int id = -1;

    if (s->depart < 0.f)
        clavier(app, s);
    for (int e = 0; e < NB_EQUIPES; e++)
        for (int i = 0; i < s->nb[e]; i++)
            marques[nb_marques++] = (marque_t){s->ids[e][i], e == 0 ? C_CYAN : C_ROUGE,
                BADGES[e][i]};
    for (int i = 0; i < app->equipe.len; i++)
        ids[i] = i;
    en_tete(app, &s->retour, "Combat", s->etape == 0
        ? "Choisis ton équipe : 1 à 4 personnages (Espace ou clic)."
        : "Choisis maintenant l'équipe adverse, jouée par l'ordinateur.");
    etapes(app, s, app->alpha);
    id = grille_cartes(app, &s->grille, ids, app->equipe.len, GRILLE_Y, marques,
        nb_marques);
    if (id >= 0 && s->depart < 0.f) {
        s->curseur = id;
        basculer(app, s, id);
    }
    if (app->equipe.len > 0)
        curseur(app, s);
    if (s->depart >= 0.f) {
        s->depart -= app->dt;
        if (s->depart < 0.f)
            changer_scene(app, SCENE_ARENE);
    }
}
