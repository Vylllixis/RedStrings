/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Création, mode Yu-Gi-Oh! : la grille des classes devient une main de
 * cartes, et l'hologramme de l'aperçu une grande carte qui se retourne à
 * chaque changement de classe. Mêmes classes, même ordre, mêmes règles de
 * sélection que la grille normale : seul le dessin change.
 */

#include <math.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_etat.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_ui.h"
#include "gui_yugioh.h"
#include "creation_interne.h"

/* La main : cartes en éventail sous les onglets de catégorie */
#define MAIN_L 62.f
#define MAIN_H (MAIN_L * FACE_H / FACE_L)
#define MAIN_PAS_MAX 84.f
#define MAIN_ARC 0.8f
#define DONNE_DELAI 0.045f
#define DONNE_DUREE 0.45f

/* La grande carte de l'aperçu */
#define APERCU_L 170.f
#define APERCU_Y (SCENE_Y - 6.f)
#define DUREE_RETOURNEMENT 0.8f

static int cartes_de_categorie(categorie_t cat, classe_t ids[NB_CLASSES])
{
    int n = 0;

    for (int i = 0; i < NB_CLASSES; i++)
        if (classe_info((classe_t)i)->categorie == cat)
            ids[n++] = (classe_t)i;
    return n;
}

/** Pose d'une carte de la main : donne, éventail, survol, sélection. */
static carte3d_t pose_main(app_t *app, creation_t *c, classe_t cl, int i, int n,
    sfVector2f origine, float pas)
{
    float k = i - (n - 1) / 2.f;
    float donne = progression(c->t_main, 0.05f + i * DONNE_DELAI, DONNE_DUREE);
    float d = sortie_cubique(donne);
    float s = c->tuiles[cl].survol;
    float sel = c->tuiles[cl].appui;
    float m = fmaxf(s, sel);
    float echelle = 1.f + 0.06f * s + 0.14f * sel;
    carte3d_t p = carte3d_pose((sfVector2f){origine.x + i * pas + 320.f * (1.f - d),
        origine.y + MAIN_H / 2.f + k * k * MAIN_ARC - 8.f * s * (1.f - sel)
        - MAIN_H * (echelle - 1.f) / 2.f}, MAIN_L * echelle);

    p.rot_z = k * 0.05f * (1.f - m);
    p.rot_y = PI * (1.f - d);
    if (s > 0.01f && !app->in.dehors) {
        p.rot_y += bornerf((app->in.souris.x - p.centre.x) / MAIN_L, -1.f, 1.f) * 0.35f * s;
        p.rot_x -= bornerf((app->in.souris.y - p.centre.y) / MAIN_H, -1.f, 1.f) * 0.3f * s;
    }
    p.alpha = progression(c->t_main, 0.05f + i * DONNE_DELAI, 0.12f);
    p.gris = 0.2f * (1.f - m);
    p.reflet = 0.12f + 0.88f * sel;
    p.bord = sel;
    p.ombre = 0.35f + 0.65f * m;
    p.halo = avec_alpha(couleur_classe(cl), 0.15f * s + 0.5f * sel);
    return p;
}

/** Carte sous la souris, la plus en avant d'abord (sélection, puis de droite à gauche). */
static int carte_survolee(app_t *app, int n, int sel, sfVector2f origine, float pas)
{
    sfFloatRect r;

    for (int t = -1; t < n; t++) {
        int i = t < 0 ? sel : n - 1 - t;

        if (i < 0 || (t >= 0 && i == sel))
            continue;
        r = (sfFloatRect){origine.x + i * pas - MAIN_L / 2.f, origine.y, MAIN_L, MAIN_H};
        if (survole(app, r))
            return i;
    }
    return -1;
}

static void dessiner_main(app_t *app, creation_t *c, const classe_t *ids, int n,
    sfVector2f origine, float pas)
{
    int sel = -1;
    int dessus = -1;
    int ordre[NB_CLASSES];
    int k = 0;
    carte3d_t p;

    for (int i = 0; i < n; i++) {
        if (ids[i] == c->classe)
            sel = i;
        else if (c->tuiles[ids[i]].survol > 0.5f)
            dessus = i;
    }
    for (int i = 0; i < n; i++)
        if (i != sel && i != dessus)
            ordre[k++] = i;
    if (dessus >= 0)
        ordre[k++] = dessus;
    if (sel >= 0)
        ordre[k++] = sel;
    for (int j = 0; j < k; j++) {
        p = pose_main(app, c, ids[ordre[j]], ordre[j], n, origine, pas);
        carte3d(app, ids[ordre[j]], &p);
        carte3d_titre(app, ids[ordre[j]], &p);
        if (ordre[j] == sel && c->focus == FOCUS_CLASSE)
            crochets(app, (sfFloatRect){p.centre.x - p.largeur / 2.f - 6.f, p.centre.y
                - p.largeur * FACE_H / FACE_L / 2.f - 6.f, p.largeur + 12.f, p.largeur
                * FACE_H / FACE_L + 12.f}, 10.f, avec_alpha(C_BLANC, 0.6f + 0.3f
                * sinf(app->temps * 5.f)));
    }
}

void creation_main_cartes(app_t *app, creation_t *c, float y)
{
    categorie_t cat = classe_info(c->classe)->categorie;
    classe_t ids[NB_CLASSES];
    int n = cartes_de_categorie(cat, ids);
    float pas = n > 1 ? fminf(MAIN_PAS_MAX, (FW - MAIN_L) / (n - 1)) : 0.f;
    sfVector2f origine = {FX + FW / 2.f - pas * (n - 1) / 2.f, y - 2.f};
    int sel = -1;
    int survol = -1;

    if ((int)cat != c->cat_main) {
        c->cat_main = (int)cat;
        c->t_main = 0.f;
    }
    c->t_main += app->dt;
    for (int i = 0; i < NB_CLASSES; i++)
        c->tuiles[i].appui = approcher(c->tuiles[i].appui, c->classe == (classe_t)i
            ? 1.f : 0.f, 12.f, app->dt);
    for (int i = 0; i < n; i++)
        if (ids[i] == c->classe)
            sel = i;
    if (c->t_main > 0.3f)
        survol = carte_survolee(app, n, sel, origine, pas);
    for (int i = 0; i < n; i++)
        c->tuiles[ids[i]].survol = approcher(c->tuiles[ids[i]].survol, i == survol
            ? 1.f : 0.f, 14.f, app->dt);
    if (survol >= 0) {
        app->curseur_voulu = CURSEUR_MAIN;
        if (app->in.clic) {
            c->classe = ids[survol];
            c->focus = FOCUS_CLASSE;
        }
    }
    dessiner_main(app, c, ids, n, origine, pas);
}

/** Cercle d'invocation derrière la grande carte. */
static void cercle_invocation(app_t *app, sfColor col)
{
    sfVector2f c = {SCENE_X, APERCU_Y};
    float t = app->temps;

    lueur(app, c, (sfVector2f){230.f, 230.f}, avec_alpha(col, 0.16f));
    for (int k = 0; k < 3; k++)
        arc_cercle(app, c, 150.f, 2.f, (sfVector2f){t * 0.4f + k * 2.f * PI / 3.f,
            t * 0.4f + k * 2.f * PI / 3.f + 1.5f}, avec_alpha(col, 0.55f), 1);
    graduations(app, c, 162.f, 72, 4.f, -t * 0.06f, avec_alpha(C_BLANC, 0.16f));
}

/**
 * La grande carte : entre en scène en se redressant, se retourne d'un tour
 * complet à chaque changement de classe (ancienne face, dos, nouvelle face),
 * s'incline vers la souris et s'envole quand le personnage est créé.
 */
void creation_carte_apercu(app_t *app, creation_t *c)
{
    float f = progression(c->t_classe, 0.f, DUREE_RETOURNEMENT);
    float tour = 2.f * PI * entree_sortie_cubique(f);
    float e = sortie_cubique(progression(app->t_scene, 0.2f, 0.8f));
    float envol = c->sortie >= 0.f
        ? entree_cubique(1.f - c->sortie / DUREE_SORTIE_CREATION) : 0.f;
    sfColor col = melanger(couleur_classe(c->classe_prec), couleur_classe(c->classe),
        progression(c->t_classe, 0.f, 0.4f));
    carte3d_t k = carte3d_pose((sfVector2f){SCENE_X, APERCU_Y + 4.f
        * sinf(app->temps * 1.3f) + 70.f * (1.f - e) - 90.f * envol},
        APERCU_L * (0.86f + 0.14f * e) * (1.f + 0.07f * sinf(f * PI)));
    float vise_x = 0.f;
    float vise_y = 0.f;

    if (!app->in.dehors) {
        vise_x = -bornerf((app->in.souris.y - k.centre.y) / 260.f, -1.f, 1.f) * 0.16f;
        vise_y = bornerf((app->in.souris.x - k.centre.x) / 320.f, -1.f, 1.f) * 0.22f;
    }
    c->incl_x = approcher(c->incl_x, vise_x, 6.f, app->dt);
    c->incl_y = approcher(c->incl_y, vise_y, 6.f, app->dt);
    k.rot_y = tour + c->incl_y + 0.05f * sinf(app->temps * 0.7f) + envol * 2.f * PI;
    k.rot_x = c->incl_x + 0.9f * (1.f - e);
    k.alpha = e * (1.f - envol);
    k.reflet = 0.35f + 0.65f * (1.f - progression(c->t_classe, 0.55f, 0.6f));
    k.bord = 0.85f;
    k.ombre = 1.f;
    k.halo = avec_alpha(col, 0.45f);
    cercle_invocation(app, avec_alpha(col, e * (1.f - envol)));
    carte3d(app, tour < PI ? c->classe_prec : c->classe, &k);
}
