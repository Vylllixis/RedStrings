/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ composition_console.c ──────────────────────────────────────────────
** │ But         Composer les deux équipes d'un combat dans le terminal :
** │             demander les personnages un par un, en refusant ceux qui
** │             sont K.O. ou déjà engagés.
** │ Contenu     deja_choisi, nb_disponibles, montrer_liste, montrer_equipe,
** │             choisir_equipe
** │ Utilisé par bataille_console.c (menu_bataille)
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include "composition_console.h"
#include "couleurs.h"
#include "liste_console.h"
#include "my_string.h"
#include "saisie.h"

/*  Constitution des équipes                                           */
/* ------------------------------------------------------------------ */

static int deja_choisi(const personnage_t *choisis, int n, const char *nom)
{
    if (choisis == NULL || nom == NULL)
        return 0;
    for (int i = 0; i < n; i++) {
        if (my_strcmp(choisis[i].nom, nom) == 0)
            return 1;
    }
    return 0;
}

/** Personnages encore engageables : en forme et pas déjà choisis. */
static int nb_disponibles(const roster_t *r, const personnage_t *equipe, int n,
    const personnage_t *autre, int nb_autre)
{
    int dispo = 0;

    for (int i = 0; i < r->len; i++)
        dispo += !est_ko(&r->data[i]) && !deja_choisi(equipe, n, r->data[i].nom)
            && !deja_choisi(autre, nb_autre, r->data[i].nom);
    return dispo;
}

/* La liste complète, avec les personnages déjà engagés signalés dans chaque camp. */
static void montrer_liste(const roster_t *r, const personnage_t *equipe, int n,
    const personnage_t *autre, int nb_autre)
{
    groupe_liste_t groupes[2];
    int nb = 0;

    if (autre == NULL) {
        groupes[nb++] = (groupe_liste_t){equipe, n, "✔ ton équipe", CYAN};
    } else {
        groupes[nb++] = (groupe_liste_t){equipe, n, "✔ adversaire", ROUGE};
        groupes[nb++] = (groupe_liste_t){autre, nb_autre, "✔ ton équipe", CYAN};
    }
    afficher_liste(r, groupes, nb);
}

/* « ✔ Arthur · Merlin » : l'équipe en cours, d'un coup d'œil. */
static void montrer_equipe(const personnage_t *equipe, int n)
{
    if (equipe == NULL)
        return;
    printf("  " VERT "✔" RESET);
    for (int i = 0; i < n; i++)
        printf("%s %s", i > 0 ? " ·" : "", equipe[i].nom);
    printf("\n");
}

/*
** ╭─ choisir_equipe ─────────────────────────────────────────────────────
** │ Demande jusqu'à TAILLE_EQUIPE_MAX personnages, par nom ou par ID (une
** │ ligne vide termine, q annule tout).
** │
** │ 1. affiche la liste complète (avec les personnages déjà engagés), puis,
** │    après chaque choix, une ligne avec l'équipe en cours
** │ 2. roster_trouver() comprend un nom ou un ID
** │ 3. refuse un personnage introuvable, K.O., ou déjà engagé dans l'une
** │    des deux équipes (comparé par le nom retrouvé, pas par le texte tapé)
** │ Renvoie la taille de l'équipe, SAISIE_ANNULEE (q) ou SAISIE_FIN
** │ (Ctrl+D).
** ╰──────────────────────────────────────────────────────────────────────
*/
int choisir_equipe(const roster_t *r, const char *titre,
    personnage_t equipe[TAILLE_EQUIPE_MAX], const personnage_t *autre, int nb_autre)
{
    char saisie[64];
    int n = 0;
    int idx = 0;

    if (!roster_valide(r) || titre == NULL || equipe == NULL
        || nb_autre < 0 || nb_autre > TAILLE_EQUIPE_MAX
        || (autre == NULL && nb_autre > 0))
        return SAISIE_FIN;
    montrer_liste(r, equipe, n, autre, nb_autre);
    printf("  " GRAS "%s" RESET GRIS " (1 à %d · nom ou ID · ligne vide = terminer · "
        MOT_ANNULER " = annuler)" RESET "\n", titre, TAILLE_EQUIPE_MAX);
    while (n < TAILLE_EQUIPE_MAX && nb_disponibles(r, equipe, n, autre, nb_autre) > 0) {
        printf("  " GRIS "%d." RESET CYAN " › " RESET, n + 1);
        if (lire_ligne(saisie, sizeof(saisie)) == -1)
            return SAISIE_FIN;
        if (my_strcmp(saisie, MOT_ANNULER) == 0)
            return SAISIE_ANNULEE;
        if (saisie[0] == '\0' && n > 0)
            break;
        idx = roster_trouver(r, saisie);
        if (idx == -1)
            afficher_erreur("Personnage introuvable");
        else if (est_ko(&r->data[idx]))
            afficher_erreur("Ce personnage est K.O.");
        else if (deja_choisi(equipe, n, r->data[idx].nom)
            || deja_choisi(autre, nb_autre, r->data[idx].nom))
            afficher_erreur("Ce personnage est déjà engagé");
        else {
            equipe[n++] = r->data[idx];
            montrer_equipe(equipe, n);
        }
    }
    printf("\n");
    return n;
}
