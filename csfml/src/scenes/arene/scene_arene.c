/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * L'arène : combat en équipe, mis en scène comme les combats « vus de
 * trois quarts » des anciens jeux de rôle. Ton équipe au premier plan en
 * bas à gauche, l'adversaire plus loin en haut à droite.
 *
 * Séparation stricte :
 *  - bataille.c décide (dégâts, soins, garde, tours, victoire) ;
 *  - ce fichier reçoit les événements et les JOUE dans le temps. Les PV
 *    affichés et le journal ne rattrapent l'état réel qu'au moment de
 *    l'impact ; aucune valeur n'est recalculée ni réappliquée ici.
 */

#include <string.h>
#include "bataille.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_config.h"
#include "gui_etat.h"
#include "gui_scenes.h"
#include "gui_types.h"
#include "gui_ui.h"
#include "arene_interne.h"

#define DELAI_ORDINATEUR 0.75f

visuel_t *arene_vis(arene_t *ar, ref_t r)
{
    return &ar->vis[r.equipe][r.index];
}

int arene_meme(ref_t a, ref_t b)
{
    return a.equipe == b.equipe && a.index == b.index;
}

sfColor arene_couleur_equipe(int e)
{
    return e == 0 ? C_CYAN : C_ROUGE;
}

sfColor arene_couleur_action(int a)
{
    const sfColor couleurs[NB_ACTIONS] = {C_ROUGE, C_SOIN, C_BLEU};

    return couleurs[a];
}

void arene_demarrer(app_t *app, arene_t *ar)
{
    bataille_t *b = &ar->bataille;
    const combattant_t *c = NULL;
    visuel_t *v = NULL;

    if (bataille_init(b, ar->equipes[0], ar->tailles[0], ar->equipes[1],
        ar->tailles[1]) == -1) {
        notifier(app, "✗", "Équipes invalides", C_ROUGE);
        changer_scene(app, SCENE_MENU);
        return;
    }
    memset(ar->vis, 0, sizeof(ar->vis));
    memset(ar->popups, 0, sizeof(ar->popups));
    for (int e = 0; e < NB_EQUIPES; e++) {
        for (int i = 0; i < b->equipes[e].taille; i++) {
            c = &b->equipes[e].membres[i];
            v = &ar->vis[e][i];
            v->pv_vis = c->pv;
            v->pv_barre = (float)c->pv;
            v->pv_fantome = (float)c->pv;
        }
    }
    ar->etat = VUE_INTRO;
    ar->t_etat = 0.f;
    ar->nb_file = 0;
    ar->i_file = 0;
    ar->journal_vu = b->nb_journal;
}

void entrer_arene(app_t *app)
{
    arene_t *ar = &app->arene;
    selection_t *s = &app->selection;

    memset(ar, 0, sizeof(arene_t));
    for (int e = 0; e < NB_EQUIPES; e++) {
        ar->tailles[e] = s->nb[e];
        for (int i = 0; i < s->nb[e]; i++)
            ar->equipes[e][i] = app->equipe.data[s->ids[e][i]];
    }
    arene_demarrer(app, ar);
}

void arene_debut_tour(arene_t *ar)
{
    ar->t_tour = 0.f;
    ar->t_etat = 0.f;
    ar->etat = bataille_actif(&ar->bataille).equipe == 0 ? VUE_CHOIX_ACTION
        : VUE_ORDINATEUR;
    ar->action = ACTION_ATTAQUER;
}

void arene_lancer(app_t *app, arene_t *ar, action_t a, ref_t cible)
{
    int n = bataille_agir(&ar->bataille, a, cible, ar->file);

    if (n < 0) {
        notifier(app, "✗", "Action impossible", C_ROUGE);
        return;
    }
    ar->nb_file = n;
    ar->i_file = 0;
    ar->t_evt = 0.f;
    ar->impact_fait = 0;
    ar->etat = VUE_ANIMATION;
}

static void maj_etat(app_t *app, arene_t *ar)
{
    action_t a = ACTION_ATTAQUER;
    ref_t cible = {0, 0};

    ar->t_etat += app->dt;
    ar->t_tour += app->dt;
    if (ar->etat == VUE_INTRO && ar->t_etat >= DUREE_INTRO)
        arene_debut_tour(ar);
    if (ar->etat == VUE_ORDINATEUR && ar->t_etat >= DELAI_ORDINATEUR) {
        bataille_ia(&ar->bataille, &a, &cible);
        arene_lancer(app, ar, a, cible);
    }
    arene_jouer_evenements(app, ar);
}

void scene_arene(app_t *app)
{
    arene_t *ar = &app->arene;
    ref_t ordre[NB_EQUIPES * TAILLE_EQUIPE_MAX];
    ref_t actif = bataille_actif(&ar->bataille);
    int montre_actif = ar->etat == VUE_CHOIX_ACTION || ar->etat == VUE_CHOIX_CIBLE
        || ar->etat == VUE_ORDINATEUR;
    float intro = ar->etat == VUE_INTRO ? ar->t_etat : DUREE_INTRO;
    float e = sortie_cubique(progression(intro, 0.5f, 0.5f));
    float a = app->alpha;
    int n = 0;

    arene_clavier(app, ar);
    arene_souris_cibles(app, ar);
    maj_etat(app, ar);
    arene_maj_visuels(app, ar);
    arene_placer(app, ar);
    arene_dessiner_arene(app, ar);
    n = arene_ordre_dessin(ar, ordre);
    for (int i = 0; i < n; i++)
        arene_dessiner_combattant(app, ar, ordre[i], montre_actif && arene_meme(ordre[i], actif));
    arene_dessiner_effet(app, ar);
    for (int i = 0; i < n; i++)
        arene_plaque(app, ar, ordre[i], montre_actif && arene_meme(ordre[i], actif));
    if (ar->etat == VUE_CHOIX_CIBLE)
        arene_reticules(app, ar);
    app->alpha = a * e;
    arene_ordre_tours(app, ar);
    arene_panneau_equipe(app, ar, 1, (sfFloatRect){MARGE - 40.f * (1.f - e), 24.f, 360.f,
        44.f + ar->bataille.equipes[1].taille * LIGNE_A});
    arene_journal(app, ar, 90.f + ar->bataille.equipes[1].taille * LIGNE_A);
    arene_panneau_equipe(app, ar, 0, (sfFloatRect){LARGEUR - MARGE - CMD_L + 40.f * (1.f - e),
        HAUTEUR - MARGE - CMD_H - 14.f - (44.f + ar->bataille.equipes[0].taille * LIGNE_J),
        CMD_L, 44.f + ar->bataille.equipes[0].taille * LIGNE_J});
    if (ar->etat != VUE_FIN)
        arene_panneau_commandes(app, ar, e);
    app->alpha = a;
    arene_banniere(app, ar);
    arene_dessiner_popups(app, ar);
    if (ar->etat == VUE_FIN)
        arene_ecran_fin(app, ar);
}
