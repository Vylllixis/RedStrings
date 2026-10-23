/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ scene_liste.c ──────────────────────────────────────────────────────
** │ But         Écran « Afficher un personnage » : recherche par nom,
** │             grille des personnages et fiche plein écran.
** │ Équivaut à  demander_personnage() et afficher_personnage() de la
** │             console
** │ Contenu     entrer_liste, filtrer, valider, naviguer, clavier_modal,
** │             boutons_modal, carte_fiche, modal, recherche, scene_liste
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_ecran.h"
#include "gui_etat.h"
#include "gui_jeu.h"
#include "gui_scenes.h"
#include "gui_types.h"
#include "gui_ui.h"
#include "gui_yugioh.h"

#define FICHE_L 520.f
#define FICHE_H 510.f
/* Mode Yu-Gi-Oh! : la carte du personnage, à gauche de sa fiche */
#define CARTE_FICHE_L 290.f
#define CARTE_FICHE_ECART 36.f

void entrer_liste(app_t *app)
{
    if (app == NULL)
        return;

    liste_t *l = &app->liste;

    memset(l, 0, sizeof(liste_t));
    l->ouvert = -1;
    l->recherche.marge = 50.f;
}

static void ouvrir(liste_t *l, int id)
{
    l->ouvert = id;
    l->dernier = id;
    l->t_modal = 0.f;
    memset(l->segments, 0, sizeof(l->segments));
}

static int filtrer(app_t *app, const char *recherche, int *ids)
{
    int n = 0;

    for (int i = 0; i < app->equipe.len; i++) {
        if (contient_nom(app->equipe.data[i].nom, recherche))
            ids[n++] = i;
    }
    return n;
}

static void loupe(app_t *app, sfVector2f c, sfColor col)
{
    sfVector2f manche[2] = {{c.x + 3.5f, c.y + 3.5f}, {c.x + 9.f, c.y + 9.f}};

    cercle(app, (sfVector2f){c.x - 1.5f, c.y - 1.5f}, 7.5f, C_TRANSPARENT, col, 2.f);
    ligne_epaisse(app, manche, 2, 2.6f, col, 0);
}

/** Entrée dans la recherche : même logique que demander_personnage(). */
static void valider(app_t *app, liste_t *l, const int *ids, int n)
{
    int idx = trouver_personnage(app->equipe.data, app->equipe.len, l->recherche.texte);

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
    if (app->equipe.len > 0)
        ouvrir(l, (l->ouvert + sens + app->equipe.len) % app->equipe.len);
}

static void clavier_modal(app_t *app, liste_t *l)
{
    if (app->in.touches[sfKeyEscape] || app->in.touches[sfKeyEnter])
        l->ouvert = -1;
    else if (app->in.touches[sfKeyLeft] && app->equipe.len > 1)
        naviguer(app, l, -1);
    else if (app->in.touches[sfKeyRight] && app->equipe.len > 1)
        naviguer(app, l, 1);
}

static int boutons_modal(app_t *app, liste_t *l, sfFloatRect r)
{
    int clic = 0;

    if (bouton_rond(app, &l->fermer, (sfVector2f){r.left + r.width + 56.f, r.top + 24.f},
        "×")) {
        l->ouvert = -1;
        clic = 1;
    }
    if (app->equipe.len < 2)
        return clic;
    if (bouton_rond(app, &l->precedent, (sfVector2f){r.left - 56.f, r.top
        + r.height / 2.f}, "‹")) {
        naviguer(app, l, -1);
        clic = 1;
    }
    if (bouton_rond(app, &l->suivant, (sfVector2f){r.left + r.width + 56.f, r.top
        + r.height / 2.f}, "›")) {
        naviguer(app, l, 1);
        clic = 1;
    }
    return clic;
}

/** Fiche plein écran : équivalent de afficher_personnage(). */
/** Carte de la classe du personnage : se retourne à l'ouverture et à chaque changement. */
static void carte_fiche(app_t *app, const liste_t *l, const personnage_t *p, sfFloatRect r)
{
    float e = sortie_cubique(progression(l->t_modal, 0.05f, 0.6f));
    carte3d_t k = carte3d_pose((sfVector2f){r.left - CARTE_FICHE_ECART - CARTE_FICHE_L / 2.f
        - 60.f * (1.f - e), r.top + r.height / 2.f}, CARTE_FICHE_L);

    k.rot_y = -PI * (1.f - e);
    if (!app->in.dehors) {
        k.rot_y += bornerf((app->in.souris.x - k.centre.x) / 400.f, -1.f, 1.f) * 0.14f;
        k.rot_x = -bornerf((app->in.souris.y - k.centre.y) / 400.f, -1.f, 1.f) * 0.1f;
    }
    k.rot_z = -0.03f * e;
    k.reflet = 0.3f + 0.7f * (1.f - progression(l->t_modal, 0.5f, 0.7f));
    k.bord = 0.7f;
    k.ombre = 1.f;
    k.gris = est_ko(p) ? 1.f : 0.f;
    k.halo = avec_alpha(couleur_classe(p->classe), 0.4f);
    carte3d(app, p->classe, &k);
}

static void modal(app_t *app, liste_t *l, float a)
{
    int id = l->ouvert >= 0 ? l->ouvert : l->dernier;
    const personnage_t *p = NULL;
    float m = 0.f;
    sfFloatRect r;
    sfFloatRect zone;
    char buf[32];

    l->modal = approcher(l->modal, l->ouvert >= 0 ? 1.f : 0.f, 12.f, app->dt);
    if (l->modal < 0.01f || id >= app->equipe.len)
        return;
    p = &app->equipe.data[id];
    l->t_modal += app->dt;
    m = l->modal;
    app->alpha = a * m;
    rect_arrondi(app, ecran_couvrant(app), 0.f,
        (sfColor){4, 5, 12, 215});
    r = (sfFloatRect){LARGEUR / 2.f - FICHE_L / 2.f, 92.f + 50.f * (1.f - m), FICHE_L,
        FICHE_H};
    if (yugioh_actif(app))
        r.left += (CARTE_FICHE_L + CARTE_FICHE_ECART) / 2.f;
    zone = r;
    if (yugioh_actif(app)) {
        zone.left -= CARTE_FICHE_L + CARTE_FICHE_ECART;
        zone.width += CARTE_FICHE_L + CARTE_FICHE_ECART;
    }
    lueur(app, (sfVector2f){LARGEUR / 2.f, r.top + r.height / 2.f}, (sfVector2f){520.f,
        420.f}, avec_alpha(couleur_classe(p->classe), 0.12f));
    for (int i = 0; i < NB_STATS; i++)
        l->segments[i] = fminf((valeur_stat(p, (stat_t)i) + 4) / 5, fmaxf(0.f, (l->t_modal - 0.15f
            - i * 0.12f) * 48.f));
    fiche(app, p, r, l->segments);
    if (yugioh_actif(app))
        carte_fiche(app, l, p, r);
    snprintf(buf, sizeof(buf), "%d / %d", id + 1, app->equipe.len);
    texte(app, POLICE_GRAS, buf, 14, (sfVector2f){LARGEUR / 2.f, r.top + r.height + 32.f},
        C_TEXTE_DOUX, ALIGNE_CENTRE);
    texte(app, POLICE_NORMALE, "Échap pour fermer  ·  flèches pour changer de personnage",
        13, (sfVector2f){LARGEUR / 2.f, r.top + r.height + 58.f}, C_TEXTE_FAIBLE,
        ALIGNE_CENTRE);
    if (!boutons_modal(app, l, zone) && app->in.clic && !app->souris_bloquee
        && !sfFloatRect_contains(&zone, app->in.souris.x, app->in.souris.y))
        l->ouvert = -1;
    app->alpha = a;
}

static void recherche(app_t *app, liste_t *l, float a, int actif)
{
    float dy = 16.f * (1.f - sortie_cubique(progression(app->t_scene, 0.1f, 0.5f)));
    sfFloatRect r = {856.f + decal_droite(app), 96.f + dy, 360.f, 52.f};

    app->alpha = a * progression(app->t_scene, 0.1f, 0.35f);
    l->recherche.focus = actif;
    champ_texte(app, &l->recherche, r, "Rechercher un nom…", NOM_LONGUEUR);
    loupe(app, (sfVector2f){r.left + 26.f, r.top + 26.f}, melanger(C_TEXTE_FAIBLE,
        C_CYAN, l->recherche.focus_anim));
    app->alpha = a;
}

void scene_liste(app_t *app)
{
    if (app == NULL)
        return;

    liste_t *l = &app->liste;
    int ids[MAX_PERSONNAGES];
    int bloque = app->souris_bloquee;
    int ouvert = l->ouvert >= 0;
    float a = app->alpha;
    char buf[96];
    int n = 0;
    int id = -1;

    if (ouvert)
        clavier_modal(app, l);
    else if (app->in.touches[sfKeyEscape])
        changer_scene(app, SCENE_MENU);
    app->souris_bloquee = bloque || ouvert || l->modal > 0.05f;
    snprintf(buf, sizeof(buf), "%d personnage%s  ·  clique sur une carte pour voir sa fiche",
        app->equipe.len, app->equipe.len > 1 ? "s" : "");
    en_tete(app, &l->retour, "Personnages", buf);
    recherche(app, l, a, !ouvert);
    n = filtrer(app, l->recherche.texte, ids);
    if (!ouvert && app->in.touches[sfKeyEnter])
        valider(app, l, ids, n);
    id = grille_cartes(app, &l->grille, ids, n, 204.f, NULL, 0);
    if (id >= 0)
        ouvrir(l, id);
    if (n == 0) {
        snprintf(buf, sizeof(buf), "Aucun résultat pour « %s »", l->recherche.texte);
        texte(app, POLICE_NORMALE, buf, 18, (sfVector2f){LARGEUR / 2.f, 400.f},
            C_TEXTE_FAIBLE, ALIGNE_CENTRE);
    }
    app->souris_bloquee = bloque || !ouvert || l->ouvert < 0;
    modal(app, l, a);
    app->souris_bloquee = bloque;
}
