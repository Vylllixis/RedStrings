/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ grille.c ───────────────────────────────────────────────────────────
** │ But         Grille de cartes de personnage (liste, composition des
** │             équipes) : choisit son nombre de colonnes et son échelle
** │             selon la place visible, sans jamais déformer les cartes ;
** │             la souris suit la même mise à l'échelle.
** │ Contenu     chercher_marque, disposer_grille, grille_vers_ecran,
** │             grille_cartes
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_ecran.h"
#include "gui_types.h"
#include "gui_ui.h"

#define ECART 22.f
/* Jusqu'à 5 colonnes, réduite uniformément si la place manque */
#define GRILLE_COLONNES_MAX 5
#define GRILLE_MARGE_X 32.f
#define GRILLE_MARGE_BAS 12.f

static const marque_t *chercher_marque(const marque_t *marques, int nb, int id)
{
    for (int k = 0; k < nb; k++) {
        if (marques[k].id == id)
            return &marques[k];
    }
    return NULL;
}

/*
** ╭─ disposer_grille ────────────────────────────────────────────────────
** │ Choisit le nombre de colonnes et l'échelle de la grille.
** │
** │ 1. essaie de GRILLE_COLONNES_MAX colonnes jusqu'à 1
** │ 2. pour chacune, calcule l'échelle qui fait tenir les `n` cartes dans
** │    la zone visible (au plus 1, proportions gardées)
** │ 3. garde la plus grande échelle ; à égalité, le plus de colonnes
** ╰──────────────────────────────────────────────────────────────────────
*/
static void disposer_grille(const app_t *app, grille_t *g, int n, float y0)
{
    sfFloatRect e = ecran_visible(app);
    float place_l = e.width - 2.f * GRILLE_MARGE_X;
    float place_h = e.top + e.height - y0 - GRILLE_MARGE_BAS;
    float meilleur = -1.f;
    float k = 0.f;
    int lignes = 0;

    for (int c = GRILLE_COLONNES_MAX; c >= 1; c--) {
        lignes = n > 0 ? (n + c - 1) / c : 1;
        k = fminf(1.f, fminf(place_l / (c * CARTE_L + (c - 1) * ECART),
            place_h / (lignes * CARTE_H + (lignes - 1) * ECART)));
        if (k > meilleur + 0.001f) {
            meilleur = k;
            g->colonnes = c;
        }
    }
    g->zoom = fmaxf(meilleur, 0.1f);
    g->pivot = (sfVector2f){LARGEUR / 2.f, y0};
}

/** Position à l'écran d'un point de la grille (la grille est peut-être réduite). */
sfVector2f grille_vers_ecran(const grille_t *g, sfVector2f p)
{
    float k = g->zoom > 0.f ? g->zoom : 1.f;

    return (sfVector2f){g->pivot.x + (p.x - g->pivot.x) * k,
        g->pivot.y + (p.y - g->pivot.y) * k};
}

/*
** ╭─ grille_cartes ──────────────────────────────────────────────────────
** │ Dessine la grille animée de cartes. Renvoie l'id de la carte cliquée,
** │ ou -1.
** │
** │ 1. disposer_grille() choisit colonnes et zoom ; zoom_debut() /
** │    zoom_fin() encadrent le dessin, et la souris suit le même zoom
** │ 2. pour chaque carte : position cible (centrée ligne par ligne) ; la
** │    position réelle s'en approche avec approcher(), donc les cartes
** │    glissent quand la grille change
** │ 3. survol : curseur main, ressort de soulèvement ; clic : l'id est
** │    mémorisé
** │ 4. entrée en cascade : chaque carte arrive avec un rebond, un peu
** │    après la précédente
** ╰──────────────────────────────────────────────────────────────────────
*/
int grille_cartes(app_t *app, grille_t *g, const int *ids, int n, float y0,
    const marque_t *marques, int nb_marques)
{
    int clique = -1;
    float a = app->alpha;
    int id = 0;
    int nb_ligne = 0;
    float x = 0.f;
    float y = 0.f;
    float e = 0.f;
    int s = 0;
    int c = 0;

    disposer_grille(app, g, n, y0);
    c = g->colonnes;
    zoom_debut(app, g->pivot, g->zoom);
    for (int i = 0; i < n; i++) {
        id = ids[i];
        nb_ligne = n - (i / c) * c < c ? n - (i / c) * c : c;
        x = LARGEUR / 2.f - (nb_ligne * CARTE_L + (nb_ligne - 1) * ECART) / 2.f
            + (i % c) * (CARTE_L + ECART);
        y = y0 + (i / c) * (CARTE_H + ECART);
        if (!g->place[id])
            g->pos[id] = (sfVector2f){x, y};
        g->place[id] = 1;
        g->pos[id].x = approcher(g->pos[id].x, x, 12.f, app->dt);
        g->pos[id].y = approcher(g->pos[id].y, y, 12.f, app->dt);
        x = g->pos[id].x;
        y = g->pos[id].y;
        e = sortie_rebond(progression(app->t_scene, 0.15f + i * 0.05f, 0.55f));
        s = survole(app, (sfFloatRect){x, y, CARTE_L, CARTE_H});
        if (s)
            app->curseur_voulu = CURSEUR_MAIN;
        if (s && app->in.clic)
            clique = id;
        if (s && g->survol[id] < 0.02f && !app->souris_bloquee)
            son(app, SON_UI_SURVOL);
        g->survol[id] = approcher(g->survol[id], s ? 1.f : 0.f, 14.f, app->dt);
        maj_ressort(&g->lift[id], s ? -10.f : 0.f, 280.f, 18.f, app->dt);
        app->alpha = a * progression(app->t_scene, 0.15f + i * 0.05f, 0.3f);
        mini_carte(app, &app->equipe.data[id], (sfFloatRect){x, y + g->lift[id].x
            + 40.f * (1.f - e), CARTE_L, CARTE_H}, g->survol[id],
            chercher_marque(marques, nb_marques, id));
    }
    zoom_fin(app);
    app->alpha = a;
    return clique;
}
