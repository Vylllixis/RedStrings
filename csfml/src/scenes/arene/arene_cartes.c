/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ arene_cartes.c ─────────────────────────────────────────────────────
** │ But         Arène, mode Yu-Gi-Oh! : les cartes accompagnent le combat
** │             sans jamais le piloter. Invocation à l'entrée, carte
** │             activée quand le moteur joue une attaque, un soin ou une
** │             garde, cartes de ton équipe à la fin.
** │ Règle       tout suit les événements déjà résolus par bataille.c
** │ Contenu     invoquer, arene_cartes_invocation, action_de,
** │             arene_carte_action, arene_cartes_fin
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include "personnage.h"
#include "bataille.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_etat.h"
#include "gui_types.h"
#include "gui_yugioh.h"
#include "arene_interne.h"

/* L'invocation commence quand le jeton a presque fini d'entrer en formation. */
#define INVOC_RETARD (0.6f * ENTREE_DUREE)
#define INVOC_DUREE 0.8f
#define INVOC_FIN (ENTREE_DEBUT + (TAILLE_EQUIPE_MAX - 1) * ENTREE_PAS_RANG \
    + ENTREE_PAS_EQUIPE + INVOC_RETARD + INVOC_DUREE)
#define CARTE_JOUEUR_L 84.f
#define CARTE_ADVERSE_L 58.f

/** L'entrée en scène : chaque carte s'ouvre au-dessus de son jeton puis y plonge. */
static void invoquer(app_t *app, arene_t *ar, ref_t r, float t)
{
    visuel_t *v = arene_vis(ar, r);
    classe_t cl = bataille_lire(&ar->bataille, r)->perso.classe;
    float ouvre = sortie_cubique(progression(t, 0.f, 0.28f));
    float plonge = entree_cubique(progression(t, 0.48f, 0.3f));
    sfVector2f haut = {v->pos.x, v->pos.y - v->rayon * 2.4f};
    sfVector2f ici = {lerpf(v->pos.x, haut.x, ouvre), lerpf(v->pos.y, haut.y, ouvre)};
    carte3d_t k = carte3d_pose((sfVector2f){lerpf(ici.x, v->pos.x, plonge),
        lerpf(ici.y, v->pos.y, plonge)}, (r.equipe == 0 ? CARTE_JOUEUR_L : CARTE_ADVERSE_L)
        * (0.55f + 0.45f * ouvre) * (1.f - 0.8f * plonge));

    k.rot_y = -PI * (1.f - ouvre);
    k.alpha = ouvre * (1.f - plonge * plonge);
    k.reflet = 1.f;
    k.bord = 1.f - plonge;
    k.halo = avec_alpha(arene_couleur_equipe(r.equipe), 0.5f);
    carte3d(app, cl, &k);
    if (plonge > 0.6f)
        lueur(app, v->pos, (sfVector2f){v->rayon * 3.f, v->rayon * 3.f},
            avec_alpha(couleur_classe(cl), 0.8f * (1.f - plonge) / 0.4f));
}

void arene_cartes_invocation(app_t *app, arene_t *ar)
{
    float t = 0.f;

    if (!yugioh_actif(app) || ar->t_cartes > INVOC_FIN)
        return;
    for (int e = 0; e < NB_EQUIPES; e++) {
        for (int i = 0; i < ar->bataille.equipes[e].taille; i++) {
            t = ar->t_cartes - (ENTREE_DEBUT + i * ENTREE_PAS_RANG + e * ENTREE_PAS_EQUIPE
                + INVOC_RETARD);
            if (t >= 0.f && t <= INVOC_DUREE)
                invoquer(app, ar, (ref_t){e, i}, t);
        }
    }
}

/** Action jouée par l'événement : même couleur que son bouton. */
static int action_de(type_evenement_t type)
{
    if (type == EV_SOIN)
        return ACTION_SOIGNER;
    if (type == EV_GARDE)
        return ACTION_DEFENDRE;
    return ACTION_ATTAQUER;
}

/** Carte de l'acteur, activée à côté de lui pendant l'événement en cours. */
void arene_carte_action(app_t *app, arene_t *ar)
{
    const evenement_t *ev = NULL;
    visuel_t *v = NULL;
    float entre = 0.f;
    float sort = 0.f;
    float cote = 1.f;
    carte3d_t k;

    if (!yugioh_actif(app) || ar->etat != VUE_ANIMATION || ar->i_file >= ar->nb_file)
        return;
    ev = &ar->file[ar->i_file];
    if (ev->type != EV_ATTAQUE && ev->type != EV_SOIN && ev->type != EV_GARDE)
        return;
    sort = progression(ar->t_evt, 0.42f, 0.25f);
    if (sort >= 1.f)
        return;
    entre = sortie_cubique(progression(ar->t_evt, 0.f, 0.16f));
    v = arene_vis(ar, ev->acteur);
    cote = ev->acteur.equipe == 0 ? 1.f : -1.f;
    k = carte3d_pose((sfVector2f){v->pos.x + cote * v->rayon * 1.9f, v->pos.y - v->rayon
        * 1.1f - 20.f * entre}, (ev->acteur.equipe == 0 ? CARTE_JOUEUR_L : CARTE_ADVERSE_L)
        * 0.9f * (0.7f + 0.3f * entre));
    k.rot_y = cote * PI / 2.f * (1.f - entre);
    k.rot_z = cote * 0.08f;
    k.alpha = entre * (1.f - sort);
    k.reflet = 1.f;
    k.bord = 1.f;
    k.ombre = 0.6f;
    k.halo = avec_alpha(arene_couleur_action(action_de(ev->type)), 0.6f);
    carte3d(app, bataille_lire(&ar->bataille, ev->acteur)->perso.classe, &k);
}

/** Fin du combat : les cartes de ton équipe brillent (victoire) ou se grisent (défaite). */
void arene_cartes_fin(app_t *app, arene_t *ar, float cy)
{
    int gagne = ar->bataille.gagnant == 0;
    int n = ar->bataille.equipes[0].taille;
    const combattant_t *c = NULL;
    int brille = 0;
    float e = 0.f;
    float rang = 0.f;
    carte3d_t k;

    if (!yugioh_actif(app))
        return;
    for (int i = 0; i < n; i++) {
        e = sortie_cubique(progression(ar->t_etat, 0.25f + i * 0.09f, 0.55f));
        rang = i - (n - 1) / 2.f;
        c = bataille_lire(&ar->bataille, (ref_t){0, i});
        brille = gagne && combattant_vivant(c);
        k = carte3d_pose((sfVector2f){LARGEUR / 2.f + rang * 96.f, cy - 178.f
            + (gagne ? -4.f * sinf(app->temps * 2.f + i) : 24.f * e) + 40.f * (1.f - e)}, 82.f);
        k.rot_y = -PI * (1.f - e);
        k.rot_z = rang * (gagne ? 0.08f : 0.16f);
        k.rot_x = gagne ? 0.f : 0.5f * e;
        k.alpha = e * (gagne ? 1.f : 0.85f);
        k.reflet = brille ? 1.f : 0.f;
        k.bord = brille ? 1.f : 0.f;
        k.gris = brille ? 0.f : 1.f;
        k.ombre = 0.8f;
        k.halo = brille ? avec_alpha(C_CYAN, 0.5f) : C_TRANSPARENT;
        carte3d(app, c->perso.classe, &k);
        if (brille)
            carte3d_titre(app, c->perso.classe, &k);
    }
}
