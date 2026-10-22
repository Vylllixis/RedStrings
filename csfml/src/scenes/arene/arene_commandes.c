/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Arène : choix de l'action et de la cible, au clavier et à la souris.
 */

#include <math.h>
#include <stdio.h>
#include "bataille.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_dessin.h"
#include "gui_ecran.h"
#include "gui_etat.h"
#include "gui_hud.h"
#include "gui_types.h"
#include "gui_ui.h"
#include "arene_interne.h"

static const char *NOMS_ACTIONS[NB_ACTIONS] = {"Attaquer", "Soigner", "Défendre"};

static const char *TOUCHES_ACTIONS[NB_ACTIONS] = {"1", "2", "3"};

static void choisir_cibles(app_t *app, arene_t *ar)
{
    ar->nb_cibles = bataille_cibles(&ar->bataille, (action_t)ar->action, ar->cibles);
    ar->cible = 0;
    if (ar->action == ACTION_DEFENDRE) {
        arene_lancer(app, ar, ACTION_DEFENDRE, ar->cibles[0]);
        return;
    }
    if (ar->nb_cibles == 0) {
        notifier(app, "✗", "Aucune cible valide", C_ROUGE);
        return;
    }
    ar->etat = VUE_CHOIX_CIBLE;
}

static void valider_action(app_t *app, arene_t *ar, int a)
{
    ar->action = a;
    if (!bataille_action_possible(&ar->bataille, (action_t)a)) {
        notifier(app, "✗", a == ACTION_SOIGNER ? "Aucun allié ne peut être soigné"
            : "Action impossible", C_ROUGE);
        return;
    }
    choisir_cibles(app, ar);
}

void arene_clavier(app_t *app, arene_t *ar)
{
    const int *k = app->in.touches;
    const int touches[NB_ACTIONS] = {sfKeyNum1, sfKeyNum2, sfKeyNum3};
    int sens = k[sfKeyRight] || k[sfKeyDown] ? 1 : (k[sfKeyLeft] || k[sfKeyUp] ? -1 : 0);

    if (ar->etat == VUE_CHOIX_ACTION) {
        if (k[sfKeyEscape])
            changer_scene(app, SCENE_MENU);
        ar->action = (ar->action + sens + NB_ACTIONS) % NB_ACTIONS;
        for (int i = 0; i < NB_ACTIONS; i++)
            if (k[touches[i]])
                valider_action(app, ar, i);
        if (k[sfKeyEnter] || k[sfKeySpace])
            valider_action(app, ar, ar->action);
    } else if (ar->etat == VUE_CHOIX_CIBLE) {
        if (ar->nb_cibles > 0)
            ar->cible = (ar->cible + sens + ar->nb_cibles) % ar->nb_cibles;
        if (k[sfKeyEscape])
            ar->etat = VUE_CHOIX_ACTION;
        else if (k[sfKeyEnter] || k[sfKeySpace])
            arene_lancer(app, ar, (action_t)ar->action, ar->cibles[ar->cible]);
    } else if (ar->etat == VUE_FIN) {
        if (k[sfKeyEnter] || k[sfKeyR])
            arene_demarrer(app, ar);
        else if (k[sfKeyEscape])
            changer_scene(app, SCENE_MENU);
    }
}

/** Souris : survoler une cible la choisit, cliquer confirme. */
void arene_souris_cibles(app_t *app, arene_t *ar)
{
    visuel_t *v = NULL;
    float dx = 0.f;
    float dy = 0.f;

    if (ar->etat != VUE_CHOIX_CIBLE)
        return;
    for (int i = 0; i < ar->nb_cibles; i++) {
        v = arene_vis(ar, ar->cibles[i]);
        dx = app->in.souris.x - v->pos.x;
        dy = app->in.souris.y - v->pos.y;
        if (dx * dx + dy * dy > v->rayon * v->rayon * 1.6f || app->souris_bloquee)
            continue;
        app->curseur_voulu = CURSEUR_MAIN;
        ar->cible = i;
        if (app->in.clic)
            arene_lancer(app, ar, (action_t)ar->action, ar->cibles[i]);
        return;
    }
}

/**
 * Combattant présenté à l'écran : pendant une animation, celui qui agit
 * (le moteur, lui, est déjà passé au tour suivant).
 */
static ref_t acteur_affiche(const arene_t *ar)
{
    if (ar->etat == VUE_ANIMATION && ar->i_file < ar->nb_file)
        return ar->file[ar->i_file].acteur;
    return bataille_actif(&ar->bataille);
}

/** Bouton d'action : touche, nom, et la statistique qui le gouverne. */
static int bouton_action(app_t *app, arene_t *ar, int a, sfFloatRect r, int dispo)
{
    const combattant_t *c = bataille_lire(&ar->bataille, acteur_affiche(ar));
    int choix = ar->action == a && (ar->etat == VUE_CHOIX_ACTION
        || ar->etat == VUE_CHOIX_CIBLE);
    int ouvert = ar->etat == VUE_CHOIX_ACTION;
    int s = survole(app, r) && ouvert;
    sfColor col = arene_couleur_action(a);
    float h = 0.f;
    float al = app->alpha;
    char info[32];

    ar->boutons[a].survol = approcher(ar->boutons[a].survol, s || choix ? 1.f : 0.f,
        14.f, app->dt);
    h = ar->boutons[a].survol;
    if (s)
        app->curseur_voulu = CURSEUR_MAIN;
    if (!dispo || !ouvert)
        app->alpha *= ouvert ? 0.45f : (choix ? 0.9f : 0.4f);
    panneau(app, (sfFloatRect){r.left, r.top - 3.f * h, r.width, r.height}, 10.f,
        melanger(C_SURFACE_HAUTE, col, 0.1f + 0.25f * h), C_SURFACE,
        melanger(C_BORD, col, 0.3f + 0.7f * h));
    if (h > 0.01f)
        rect_arrondi(app, (sfFloatRect){r.left + 10.f, r.top + r.height - 3.f - 3.f * h,
            (r.width - 20.f) * h, 3.f}, 0.f, col);
    touche_clavier(app, TOUCHES_ACTIONS[a], (sfVector2f){r.left + 22.f, r.top + 20.f
        - 3.f * h}, col, h);
    texte(app, POLICE_GRAS, NOMS_ACTIONS[a], 16, (sfVector2f){r.left + 42.f, r.top
        + 20.f - 3.f * h}, melanger(C_TEXTE_DOUX, C_TEXTE, h), ALIGNE_GAUCHE);
    if (a == ACTION_ATTAQUER)
        snprintf(info, sizeof(info), "Attaque %d", c->perso.atq);
    else if (a == ACTION_SOIGNER)
        snprintf(info, sizeof(info), dispo ? "Soin %d" : (c->perso.soin > 0
            ? "Personne à soigner" : "Soin nul"), c->perso.soin);
    else
        snprintf(info, sizeof(info), "-%d %% de dégâts", GARDE_REDUCTION);
    texte(app, POLICE_NORMALE, info, 12, (sfVector2f){r.left + 14.f, r.top + 44.f
        - 3.f * h}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    app->alpha = al;
    return s && app->in.clic && dispo;
}

void arene_panneau_commandes(app_t *app, arene_t *ar, float e)
{
    sfFloatRect v = ecran_visible(app);
    sfFloatRect r = {v.left + v.width - MARGE - CMD_L + 60.f * (1.f - e), v.top + v.height
        - MARGE - CMD_H + 12.f, CMD_L, CMD_H};
    const combattant_t *c = bataille_lire(&ar->bataille, acteur_affiche(ar));
    float bw = (CMD_L - 40.f - 2 * 10.f) / NB_ACTIONS;
    float x = 0.f;
    char buf[64];

    ombre_biseau(app, r, 18.f, 1.2f);
    panneau(app, r, 18.f, (sfColor){24, 29, 52, 238}, (sfColor){12, 14, 28, 245},
        C_BORD);
    crochets(app, (sfFloatRect){r.left - 5.f, r.top - 5.f, r.width + 10.f,
        r.height + 10.f}, 16.f, avec_alpha(arene_couleur_equipe(acteur_affiche(ar).equipe),
        0.6f));
    x = r.left + 20.f + etiquette(app, "TOUR DE", (sfVector2f){r.left + 20.f, r.top
        + 24.f}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE) + 10.f;
    x += texte(app, POLICE_GRAS, c->perso.nom, 17, (sfVector2f){x, r.top + 23.f},
        couleur_classe(c->perso.classe), ALIGNE_GAUCHE) + 10.f;
    texte(app, POLICE_NORMALE, nom_classe(c->perso.classe), 13, (sfVector2f){x, r.top
        + 24.f}, C_TEXTE_FAIBLE, ALIGNE_GAUCHE);
    if (ar->etat == VUE_ORDINATEUR) {
        snprintf(buf, sizeof(buf), "L'adversaire réfléchit%.*s",
            1 + (int)fmodf(app->temps * 3.f, 3.f), "...");
        texte(app, POLICE_NORMALE, buf, 15, (sfVector2f){r.left + 20.f, r.top + 76.f},
            C_TEXTE_DOUX, ALIGNE_GAUCHE);
        return;
    }
    for (int a = 0; a < NB_ACTIONS; a++)
        if (bouton_action(app, ar, a, (sfFloatRect){r.left + 20.f + a * (bw + 10.f),
            r.top + 44.f, bw, 58.f}, bataille_action_possible(&ar->bataille,
            (action_t)a)))
            valider_action(app, ar, a);
    if (ar->etat == VUE_CHOIX_CIBLE)
        texte(app, POLICE_NORMALE, "Choisis la cible  ·  ‹ ›  ·  Entrée  ·  Échap retour",
            12, (sfVector2f){r.left + r.width / 2.f, r.top + r.height - 12.f},
            melanger(arene_couleur_action(ar->action), C_TEXTE, 0.4f), ALIGNE_CENTRE);
}
