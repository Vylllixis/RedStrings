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
** │             maj_carte, carte_dessus (survol), grille_cartes
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_ecran.h"
#include "gui_types.h"
#include "gui_ui.h"

#define ECART 22.f
/* Jusqu'à 5 colonnes, réduite uniformément si la place manque */
#define GRILLE_COLONNES_MAX 5
#define GRILLE_MARGE_X 32.f
#define GRILLE_MARGE_BAS 12.f
/* Survol d'une carte : assez visible pour se voir d'un coup d'œil, sans masquer
 * les voisines (+7 % de 216 px = 8 px de chaque côté, l'écart en fait 22). */
#define SURVOL_ECHELLE 0.07f        /* agrandissement */
#define SURVOL_LEVEE 18.f           /* levée, en pixels de la grille */
#define SURVOL_INCLINAISON 2.f      /* degrés au plus, vers la souris */
#define SURVOL_ENTREE 16.f          /* vitesse d'apparition (lissage exponentiel) */
#define SURVOL_SORTIE 9.f           /* retour plus doux que l'entrée */

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
    if (g == NULL)
        return (sfVector2f){0.f, 0.f};

    float k = g->zoom > 0.f ? g->zoom : 1.f;

    return (sfVector2f){g->pivot.x + (p.x - g->pivot.x) * k,
        g->pivot.y + (p.y - g->pivot.y) * k};
}

/* Où et avec quelle opacité dessiner chaque carte, une fois la grille mise à jour. */
typedef struct {
    sfFloatRect r;
    float alpha;
    int id;         /* -1 : carte ignorée (indice hors de l'équipe) */
} pose_carte_t;

/* Survol, clic et ressorts d'une carte ; renvoie 1 si elle est cliquée. */
static int maj_carte(app_t *app, grille_t *g, int id, sfFloatRect zone)
{
    int s = survole(app, zone);

    if (s)
        app->curseur_voulu = CURSEUR_MAIN;
    if (s && g->survol[id] < 0.02f && !app->souris_bloquee)
        son(app, SON_UI_SURVOL);
    g->survol[id] = approcher(g->survol[id], s ? 1.f : 0.f,
        s ? SURVOL_ENTREE : SURVOL_SORTIE, app->dt);
    maj_ressort(&g->lift[id], s ? -SURVOL_LEVEE : 0.f, 260.f, 20.f, app->dt);
    return s && app->in.clic;
}

/*
** ╭─ carte_dessus ───────────────────────────────────────────────────────
** │ Dessine la carte survolée au-dessus de toutes les autres.
** │
** │ 1. sa position à l'écran (grille_vers_ecran) devient le pivot d'une
** │    vue à part, agrandie de SURVOL_ECHELLE : la carte grossit autour
** │    de son centre, sans déplacer ses voisines
** │ 2. la vue s'incline de quelques degrés vers la souris (SURVOL_INCLINAISON)
** │ 3. une ombre douce reste « au sol », à la place de repos de la carte :
** │    plus la carte monte, plus l'écart se voit (effet de profondeur)
** │ La zone de clic ne change pas : la carte agrandie ne peut pas faire
** │ clignoter le survol sur ses bords.
** ╰──────────────────────────────────────────────────────────────────────
*/
static void carte_dessus(app_t *app, const grille_t *g, const pose_carte_t *p,
    const marque_t *marque)
{
    float s = g->survol[p->id];
    float k = sortie_cubique(s);
    float ech = 1.f + SURVOL_ECHELLE * k;
    sfVector2f c = grille_vers_ecran(g, (sfVector2f){p->r.left + p->r.width / 2.f,
        p->r.top + p->r.height / 2.f});
    float dx = bornerf((app->in.souris.x - c.x) / (p->r.width * g->zoom), -1.f, 1.f);
    sfFloatRect local = {c.x - p->r.width / 2.f, c.y - p->r.height / 2.f, p->r.width,
        p->r.height};
    float sol = -g->lift[p->id].x / ech;

    zoom_debut(app, c, g->zoom * ech);
    sfView_setRotation(app->vue_zoom, dx * SURVOL_INCLINAISON * k);
    sfRenderWindow_setView(app->fenetre, app->vue_zoom);
    app->alpha = p->alpha;
    ombre(app, (sfFloatRect){local.left + 8.f, local.top + sol + 16.f, local.width - 16.f,
        local.height - 8.f}, 22.f, 0.6f * k);
    mini_carte(app, &app->equipe.data[p->id], local, s, marque);
    sfView_setRotation(app->vue_zoom, 0.f);
    zoom_fin(app);
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
** │ 3. survol (maj_carte) : curseur main, son discret, ressort de levée ;
** │    clic : l'id est mémorisé
** │ 4. entrée en cascade : chaque carte arrive avec un rebond, un peu
** │    après la précédente
** │ 5. toutes les cartes sont dessinées, sauf la plus survolée, dessinée
** │    en dernier par carte_dessus() : elle passe toujours au-dessus de ses
** │    voisines, même pendant qu'elle redescend
** ╰──────────────────────────────────────────────────────────────────────
*/
int grille_cartes(app_t *app, grille_t *g, const int *ids, int n, float y0,
    const marque_t *marques, int nb_marques)
{
    if (app == NULL || g == NULL || (ids == NULL && n > 0))
        return -1;
    if (marques == NULL || nb_marques < 0)
        nb_marques = 0;
    if (n < 0)
        n = 0;

    pose_carte_t poses[MAX_PERSONNAGES];
    int clique = -1;
    int dessus = -1;
    float a = app->alpha;
    int id = 0;
    int nb_ligne = 0;
    float x = 0.f;
    float y = 0.f;
    float e = 0.f;
    int c = 0;

    n = n > MAX_PERSONNAGES ? MAX_PERSONNAGES : n;
    disposer_grille(app, g, n, y0);
    c = g->colonnes;
    zoom_debut(app, g->pivot, g->zoom);
    for (int i = 0; i < n; i++) {
        id = ids[i];
        poses[i].id = id >= 0 && id < app->equipe.len && id < MAX_PERSONNAGES ? id : -1;
        if (poses[i].id < 0)
            continue;
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
        if (maj_carte(app, g, id, (sfFloatRect){x, y, CARTE_L, CARTE_H}))
            clique = id;
        poses[i].r = (sfFloatRect){x, y + g->lift[id].x + 40.f * (1.f - e), CARTE_L, CARTE_H};
        poses[i].alpha = a * progression(app->t_scene, 0.15f + i * 0.05f, 0.3f);
        if (g->survol[id] > 0.01f && (dessus == -1 || g->survol[id] > g->survol[poses[dessus].id]))
            dessus = i;
    }
    for (int i = 0; i < n; i++) {
        if (i == dessus || poses[i].id < 0)
            continue;
        app->alpha = poses[i].alpha;
        mini_carte(app, &app->equipe.data[poses[i].id], poses[i].r,
            g->survol[poses[i].id], chercher_marque(marques, nb_marques, poses[i].id));
    }
    zoom_fin(app);
    if (dessus >= 0)
        carte_dessus(app, g, &poses[dessus], chercher_marque(marques, nb_marques,
            poses[dessus].id));
    app->alpha = a;
    return clique;
}
