/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ audio_suivi.c ──────────────────────────────────────────────────────
** │ But         Traduire ce qui change à l'écran en événements sonores,
** │             sans toucher au code des écrans : on compare l'état de
** │             l'image avec celui de l'image précédente.
** │ Contenu     audio_entrer_scene, suivre_menu (+ code Konami),
** │             suivre_creation, suivre_selection, suivre_liste,
** │             suivre_arene, audio_suivre, audio_impact
** │ Pourquoi    un changement d'état n'arrive qu'une fois : le son ne peut
** │             pas être déclenché deux fois par le cycle d'affichage
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "audio_interne.h"
#include "gui_app.h"
#include "gui_jeu.h"
#include "gui_ui.h"

#define PAN_EQUIPE 0.4f     /* ton équipe à gauche, l'adversaire à droite */

static const int KONAMI[] = {sfKeyUp, sfKeyUp, sfKeyDown, sfKeyDown, sfKeyLeft,
    sfKeyRight, sfKeyLeft, sfKeyRight, sfKeyB, sfKeyA};

#define NB_KONAMI ((int)(sizeof(KONAMI) / sizeof(KONAMI[0])))

/* Nouvel écran : la musique suit, et le premier état observé ne fait pas de bruit. */
void audio_entrer_scene(app_t *app, scene_t s)
{
    if (app == NULL)
        return;

    app->audio.suivi.scene = -1;
    if (s == SCENE_ARENE)
        musique(app, MUSIQUE_COMBAT);
    else if (s == SCENE_AU_REVOIR)
        musique_arreter(app, 1.4f);
    else
        musique(app, MUSIQUE_MENU);
}

static void instantane(app_t *app, audio_suivi_t *v)
{
    v->menu_selection = app->menu.selection;
    v->nb_personnages = app->equipe.len;
    v->sel_etape = app->selection.etape;
    v->sel_nb[0] = app->selection.nb[0];
    v->sel_nb[1] = app->selection.nb[1];
    v->sel_curseur = app->selection.curseur;
    v->sel_part = app->selection.depart > 0.f;
    v->liste_ouvert = app->liste.ouvert;
    v->liste_trouves = -1;
    v->crea_classe = (int)app->creation.classe;
    v->crea_focus = app->creation.focus;
    for (int i = 0; i < NB_STATS; i++)
        v->crea_valeurs[i] = app->creation.valeurs[i];
    v->arene_etat = -1;                 // l'entrée dans l'arène doit sonner
    v->arene_action = app->arene.action;
    v->arene_cible = app->arene.cible;
}

/* ↑ ↑ ↓ ↓ ← → ← → B A, sur le menu : un clin d'œil, sans aucun effet sur le jeu. */
static void suivre_konami(app_t *app, audio_suivi_t *v)
{
    const int *k = app->in.touches;
    int appui = k[sfKeyUp] || k[sfKeyDown] || k[sfKeyLeft] || k[sfKeyRight]
        || k[sfKeyA] || k[sfKeyB];

    if (!appui)
        return;
    if (k[KONAMI[v->konami]])
        v->konami++;
    else if (k[sfKeyUp])                // « ↑ ↑ ↑ » : les deux derniers ↑ restent un bon début
        v->konami = v->konami == 2 ? 2 : 1;
    else
        v->konami = 0;
    if (v->konami < NB_KONAMI)
        return;
    v->konami = 0;
    son(app, SON_SECRET_KONAMI);
    notifier(app, "★", "Code Konami : rien à gagner, mais quel style !", C_OR);
}

static void suivre_menu(app_t *app, audio_suivi_t *v)
{
    if (app->menu.selection != v->menu_selection)
        son(app, SON_UI_DEPLACEMENT);
    v->menu_selection = app->menu.selection;
    suivre_konami(app, v);
}

static void suivre_creation(app_t *app, audio_suivi_t *v)
{
    const creation_t *c = &app->creation;

    if (app->equipe.len > v->nb_personnages)
        son(app, SON_PERSONNAGE_CREE);
    if ((int)c->classe != v->crea_classe)
        son(app, classes[c->classe].categorie != classes[v->crea_classe].categorie
            ? SON_UI_ONGLET : SON_CLASSE_CHANGEMENT);
    if (c->focus != v->crea_focus && c->focus >= 0)
        son(app, SON_UI_DEPLACEMENT);
    for (int i = 0; i < NB_STATS; i++) {
        if (c->valeurs[i] != v->crea_valeurs[i])
            son_regle(app, SON_UI_VALEUR, 0.75f + 0.75f * (float)c->valeurs[i] / STAT_MAX,
                0.f);
        v->crea_valeurs[i] = c->valeurs[i];
    }
    v->nb_personnages = app->equipe.len;
    v->crea_classe = (int)c->classe;
    v->crea_focus = c->focus;
}

static void suivre_selection(app_t *app, audio_suivi_t *v)
{
    const selection_t *s = &app->selection;
    int part = s->depart > 0.f;

    for (int e = 0; e < NB_EQUIPES; e++) {
        if (s->nb[e] > v->sel_nb[e])
            son_regle(app, SON_CARTE_SELECTION, 1.f, e == 0 ? -PAN_EQUIPE : PAN_EQUIPE);
        else if (s->nb[e] < v->sel_nb[e])
            son_regle(app, SON_CARTE_RETRAIT, 1.f, e == 0 ? -PAN_EQUIPE : PAN_EQUIPE);
        v->sel_nb[e] = s->nb[e];
    }
    if (s->etape > v->sel_etape || (part && !v->sel_part))
        son(app, SON_EQUIPE_VALIDEE);
    else if (s->etape < v->sel_etape)
        son(app, SON_UI_FERMETURE);
    if (s->curseur != v->sel_curseur)
        son(app, SON_UI_DEPLACEMENT);
    v->sel_etape = s->etape;
    v->sel_curseur = s->curseur;
    v->sel_part = part;
}

static int nb_trouves(app_t *app)
{
    int n = 0;

    for (int i = 0; i < app->equipe.len; i++)
        n += contient_nom(app->equipe.data[i].nom, app->liste.recherche.texte);
    return n;
}

static void suivre_liste(app_t *app, audio_suivi_t *v)
{
    const liste_t *l = &app->liste;
    int n = nb_trouves(app);

    if (l->ouvert >= 0 && v->liste_ouvert < 0)
        son(app, SON_CARTE_OUVERTURE);
    else if (l->ouvert >= 0 && l->ouvert != v->liste_ouvert)
        son(app, SON_CARTE_DEFILEMENT);
    else if (l->ouvert < 0 && v->liste_ouvert >= 0)
        son(app, SON_UI_FERMETURE);
    if (n == 0 && v->liste_trouves > 0 && l->recherche.texte[0] != '\0')
        son(app, SON_RECHERCHE_VIDE);
    v->liste_ouvert = l->ouvert;
    v->liste_trouves = n;
}

/* L'arène : chaque changement d'état du combat a son son. */
static void suivre_arene(app_t *app, audio_suivi_t *v)
{
    const arene_t *ar = &app->arene;
    int e = (int)ar->etat;

    if (e != v->arene_etat && e == VUE_INTRO)
        son(app, SON_COMBAT_DEBUT);
    else if (e != v->arene_etat && e == VUE_CHOIX_ACTION)
        son(app, v->arene_etat == VUE_CHOIX_CIBLE ? SON_UI_FERMETURE : SON_COMBAT_TOUR);
    else if (e != v->arene_etat && e == VUE_CHOIX_CIBLE)
        son(app, SON_UI_VALIDER);
    else if (e != v->arene_etat && e == VUE_ANIMATION)
        son_regle(app, SON_COMBAT_ACTION, 1.f, bataille_actif(&ar->bataille).equipe == 0
            ? -PAN_EQUIPE : PAN_EQUIPE);
    else if (e != v->arene_etat && e == VUE_FIN)
        son(app, ar->bataille.gagnant == 0 ? SON_COMBAT_VICTOIRE : SON_COMBAT_DEFAITE);
    else if ((e == VUE_CHOIX_ACTION && ar->action != v->arene_action)
        || (e == VUE_CHOIX_CIBLE && ar->cible != v->arene_cible))
        son(app, SON_UI_DEPLACEMENT);
    v->arene_etat = e;
    v->arene_action = ar->action;
    v->arene_cible = ar->cible;
}

/* Une fois par image, après le dessin de l'écran. */
void audio_suivre(app_t *app)
{
    if (app == NULL)
        return;

    audio_suivi_t *v = &app->audio.suivi;

    if (!app->audio.pret)
        return;
    if (v->scene != (int)app->scene) {
        instantane(app, v);
        v->scene = (int)app->scene;
        if (app->scene != SCENE_ARENE)
            return;
    }
    if (app->scene == SCENE_MENU)
        suivre_menu(app, v);
    if (app->scene == SCENE_CREATION)
        suivre_creation(app, v);
    if (app->scene == SCENE_SELECTION)
        suivre_selection(app, v);
    if (app->scene == SCENE_LISTE)
        suivre_liste(app, v);
    if (app->scene == SCENE_ARENE)
        suivre_arene(app, v);
}

/*
** ╭─ audio_impact ───────────────────────────────────────────────────────
** │ Le son d'un événement du moteur, au moment de son impact à l'écran.
** │
** │ - attaque : coup paré si la cible était en garde, coup lourd s'il
** │   retire au moins la moitié des PV max (un moment marquant, pas la
** │   règle : avec des statistiques ordinaires, un coup en retire
** │   souvent 40 à 50 %), coup normal sinon
** │ - soin, garde, fin de garde, K.O. : leur son propre
** │ - le son vient du côté de la cible (ton équipe à gauche)
** ╰──────────────────────────────────────────────────────────────────────
*/
void audio_impact(app_t *app, const evenement_t *ev, int pv_max)
{
    if (app == NULL || ev == NULL)
        return;

    float pan = ev->cible.equipe == 0 ? -PAN_EQUIPE : PAN_EQUIPE;
    son_t s = SON_COMBAT_IMPACT;

    if (ev->type == EV_ATTAQUE && ev->garde)
        s = SON_COMBAT_IMPACT_GARDE;
    else if (ev->type == EV_ATTAQUE && ev->valeur * 2 >= pv_max)
        s = SON_COMBAT_IMPACT_LOURD;
    else if (ev->type == EV_SOIN)
        s = SON_COMBAT_SOIN;
    else if (ev->type == EV_GARDE)
        s = SON_COMBAT_GARDE;
    else if (ev->type == EV_GARDE_FIN)
        s = SON_COMBAT_GARDE_FIN;
    else if (ev->type == EV_KO)
        s = SON_COMBAT_KO;
    else if (ev->type != EV_ATTAQUE)
        return;
    son_regle(app, s, 1.f, pan);
}
