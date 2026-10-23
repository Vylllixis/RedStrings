/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ bataille_console.c ─────────────────────────────────────────────────
** │ But         Le combat en équipe dans le terminal : demander les
** │             actions du joueur, afficher l'état et le journal.
** │ Règle       toutes les règles viennent du moteur (bataille.c) ; ce
** │             fichier ne fait que demander et afficher
** │ Sections    Affichage · Tour du joueur · Boucle de combat
** │ Voir aussi  composition_console.c : le choix des deux équipes
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include "bataille.h"
#include "bataille_console.h"
#include "boite.h"
#include "composition_console.h"
#include "couleurs.h"
#include "saisie.h"

#define CASES_PV 12

static const char *NOMS_ACTIONS[NB_ACTIONS] = {
    [ACTION_ATTAQUER] = "Attaquer",
    [ACTION_SOIGNER] = "Soigner",
    [ACTION_DEFENDRE] = "Défendre",
};

/* ------------------------------------------------------------------ */
/*  Affichage                                                          */
/* ------------------------------------------------------------------ */

static void afficher_combattant(const bataille_t *b, ref_t r)
{
    const combattant_t *c = bataille_lire(b, r);
    ref_t actif = bataille_actif(b);
    int pleines = c->pv_max > 0 ? (c->pv * CASES_PV + c->pv_max - 1) / c->pv_max : 0;
    char nom[64];
    char classe[64];

    aligner_texte(nom, sizeof(nom), c->perso.nom, 14);
    aligner_texte(classe, sizeof(classe), classes[c->perso.classe].nom, 15);
    printf("  %s%d %s%s" RESET "%s%s" RESET, actif.equipe == r.equipe
        && actif.index == r.index ? GRAS "▶" : " ", r.index + 1,
        combattant_vivant(c) ? GRAS : GRIS, nom, couleur_ansi(c->perso.classe), classe);
    printf(" %s", combattant_vivant(c) ? VERT : GRIS);
    for (int i = 0; i < CASES_PV; i++)
        printf(i < pleines ? "■" : GRIS "□");
    printf(RESET " %3d/%-3d", c->pv, c->pv_max);
    if (!combattant_vivant(c))
        printf(ROUGE " K.O." RESET);
    else if (combattant_en_garde(c))
        printf(BLEU " ◆ garde" RESET);
    printf("\n");
}

static void afficher_bataille(const bataille_t *b)
{
    static const char *titres[NB_EQUIPES] = {"Ton équipe", "Équipe adverse"};

    for (int e = NB_EQUIPES - 1; e >= 0; e--) {
        printf("  " GRIS "── %s ──" RESET "\n", titres[e]);
        for (int i = 0; i < b->equipes[e].taille; i++)
            afficher_combattant(b, (ref_t){e, i});
    }
    printf("\n");
}

/** Affiche, dans l'ordre, les lignes de journal écrites depuis `avant`. */
static void afficher_nouveautes(const bataille_t *b, int avant)
{
    const char *ligne = NULL;

    for (int i = b->nb_journal - avant - 1; i >= 0; i--) {
        ligne = bataille_journal(b, i);
        if (ligne)
            printf("  " CYAN "»" RESET " %s\n", ligne);
    }
    printf("\n");
}

/* ------------------------------------------------------------------ */
/*  Tour du joueur                                                     */
/* ------------------------------------------------------------------ */

/** Choix de l'action ; renvoie -1 sur Ctrl+D. */
static int choisir_action(const bataille_t *b, action_t *a)
{
    int choix = 0;

    for (int i = 0; i < NB_ACTIONS; i++)
        printf("    " GRAS "%d" RESET "  %s%s" RESET "\n", i + 1,
            bataille_action_possible(b, (action_t)i) ? "" : GRIS,
            NOMS_ACTIONS[i]);
    while (1) {
        if (lire_entier("Action", 1, NB_ACTIONS, &choix) != 0)
            return -1;
        *a = (action_t)(choix - 1);
        if (bataille_action_possible(b, *a))
            return 0;
        afficher_erreur(*a == ACTION_SOIGNER ? "Aucun allié ne peut être soigné"
            : "Action impossible");
    }
}

/** Choix de la cible parmi les cibles valides (0 = retour) ; -1 sur Ctrl+D. */
static int choisir_cible(const bataille_t *b, action_t a, ref_t *cible)
{
    ref_t cibles[NB_EQUIPES * TAILLE_EQUIPE_MAX];
    const combattant_t *acteur = bataille_lire(b, bataille_actif(b));
    const combattant_t *c = NULL;
    int n = bataille_cibles(b, a, cibles);
    int choix = 0;

    if (a == ACTION_DEFENDRE) {
        *cible = cibles[0];
        return 1;
    }
    for (int i = 0; i < n; i++) {
        c = bataille_lire(b, cibles[i]);
        printf("    " GRAS "%d" RESET "  %s%-14s" RESET " %s%+d PV" RESET "\n", i + 1,
            couleur_ansi(c->perso.classe), c->perso.nom,
            a == ACTION_ATTAQUER ? ROUGE : VERT, a == ACTION_ATTAQUER
            ? -bataille_degats(acteur, c) : bataille_soin(acteur, c));
    }
    printf("    " GRAS "0" RESET "  " GRIS "Retour" RESET "\n");
    if (lire_entier("Cible", 0, n, &choix) != 0)
        return -1;
    if (choix == 0)
        return 0;
    *cible = cibles[choix - 1];
    return 1;
}

/** Tour d'un allié ; renvoie -1 si le joueur abandonne (Ctrl+D). */
static int tour_joueur(const bataille_t *b, action_t *a, ref_t *cible)
{
    int ok = 0;

    while (ok == 0) {
        printf("  " GRAS "Tour de %s%s" RESET "\n",
            couleur_ansi(bataille_lire(b, bataille_actif(b))->perso.classe),
            bataille_lire(b, bataille_actif(b))->perso.nom);
        if (choisir_action(b, a) == -1)
            return -1;
        ok = choisir_cible(b, *a, cible);
        if (ok == -1)
            return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Boucle de combat                                                   */
/* ------------------------------------------------------------------ */

/*
** ╭─ jouer ──────────────────────────────────────────────────────────────
** │ La boucle du combat, tant qu'il n'y a pas de gagnant.
** │
** │ 1. affiche l'état des deux équipes
** │ 2. équipe 0 : le joueur choisit (tour_joueur) ; équipe 1 :
** │    bataille_ia()
** │ 3. bataille_agir() applique l'action
** │ 4. afficher_nouveautes() montre les lignes de journal écrites depuis
** │    `avant`
** │ À la fin : Victoire ou Défaite.
** ╰──────────────────────────────────────────────────────────────────────
*/
static void jouer(bataille_t *b)
{
    evenement_t ev[MAX_EVENEMENTS];
    action_t a = ACTION_ATTAQUER;
    ref_t cible = {0, 0};
    int avant = 0;

    while (b->gagnant == -1) {
        afficher_bataille(b);
        if (bataille_actif(b).equipe == 0 && tour_joueur(b, &a, &cible) == -1) {
            printf("  " GRIS "↩ Combat abandonné (« " MOT_ANNULER " » ou Ctrl+D)." RESET "\n\n");
            return;
        }
        if (bataille_actif(b).equipe == 1)
            bataille_ia(b, &a, &cible);
        avant = b->nb_journal;
        if (bataille_agir(b, a, cible, ev) == -1)
            afficher_erreur("Action refusée");
        afficher_nouveautes(b, avant);
    }
    afficher_bataille(b);
    printf("  %s" RESET "\n\n", b->gagnant == 0 ? VERT GRAS "✔ Victoire !"
        : ROUGE GRAS "✗ Défaite…");
}

static int nb_vivants(const roster_t *r)
{
    int n = 0;

    for (int i = 0; i < r->len; i++)
        n += !est_ko(&r->data[i]);
    return n;
}

/*
** ╭─ menu_bataille ──────────────────────────────────────────────────────
** │ Prépare puis lance un combat.
** │
** │ 1. il faut au moins 2 personnages en forme
** │ 2. le joueur compose son équipe, en laissant au moins 1 personnage à
** │    l'adversaire
** │ 3. l'adversaire (l'ordinateur) est composé parmi les personnages
** │    restants
** │ 4. bataille_init() puis jouer()
** │ `q` annule à tout moment (composition ou tour de jeu) ; le roster n'est
** │ jamais modifié, le combat travaille sur des copies.
** ╰──────────────────────────────────────────────────────────────────────
*/
void menu_bataille(const roster_t *r)
{
    personnage_t joueurs[TAILLE_EQUIPE_MAX];
    personnage_t adversaires[TAILLE_EQUIPE_MAX];
    bataille_t b;
    int nj = 0;
    int na = 0;

    if (nb_vivants(r) < 2) {
        afficher_erreur("Il faut au moins 2 personnages en forme");
        return;
    }
    nj = choisir_equipe(r, "Ton équipe", joueurs, NULL, 0);
    if (nj < 0 || nj == nb_vivants(r)) {
        if (nj >= 0)
            afficher_erreur("Garde au moins un personnage pour l'adversaire");
        else if (nj == SAISIE_ANNULEE)
            printf("  " GRIS "↩ Combat annulé." RESET "\n\n");
        return;
    }
    na = choisir_equipe(r, "Équipe adverse (jouée par l'ordinateur)", adversaires,
        joueurs, nj);
    if (na == SAISIE_ANNULEE)
        printf("  " GRIS "↩ Combat annulé." RESET "\n\n");
    if (na < 0 || bataille_init(&b, joueurs, nj, adversaires, na) == -1)
        return;
    jouer(&b);
}
