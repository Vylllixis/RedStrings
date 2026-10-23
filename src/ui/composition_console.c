/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ composition_console.c ──────────────────────────────────────────────
** │ But         Composer les deux équipes d'un combat dans le terminal :
** │             demander les personnages un par un, en refusant ceux qui
** │             sont K.O. ou déjà engagés.
** │ Contenu     deja_choisi, nb_disponibles, choisir_equipe
** │ Utilisé par bataille_console.c (menu_bataille)
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include "composition_console.h"
#include "couleurs.h"
#include "my_string.h"
#include "saisie.h"

/*  Constitution des équipes                                           */
/* ------------------------------------------------------------------ */

static int deja_choisi(const personnage_t *choisis, int n, const char *nom)
{
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

/*
** ╭─ choisir_equipe ─────────────────────────────────────────────────────
** │ Demande jusqu'à TAILLE_EQUIPE_MAX noms (une ligne vide termine, q
** │ annule tout).
** │
** │ Refuse un nom introuvable, un personnage K.O., ou un personnage déjà
** │ engagé dans l'une des deux équipes.
** │ Renvoie la taille de l'équipe, SAISIE_ANNULEE (q) ou SAISIE_FIN
** │ (Ctrl+D).
** ╰──────────────────────────────────────────────────────────────────────
*/
int choisir_equipe(const roster_t *r, const char *titre,
    personnage_t equipe[TAILLE_EQUIPE_MAX], const personnage_t *autre, int nb_autre)
{
    char nom[NOM_MAX];
    int n = 0;
    int idx = 0;

    printf("  " GRAS "%s" RESET GRIS " (1 à %d · ligne vide = terminer · " MOT_ANNULER
        " = annuler)" RESET "\n", titre, TAILLE_EQUIPE_MAX);
    while (n < TAILLE_EQUIPE_MAX && nb_disponibles(r, equipe, n, autre, nb_autre) > 0) {
        printf("  " GRIS "%d." RESET CYAN " › " RESET, n + 1);
        if (lire_ligne(nom, sizeof(nom)) == -1)
            return SAISIE_FIN;
        if (my_strcmp(nom, MOT_ANNULER) == 0)
            return SAISIE_ANNULEE;
        if (nom[0] == '\0' && n > 0)
            break;
        idx = trouver_personnage(r->data, r->len, nom);
        if (idx == -1)
            afficher_erreur("Personnage introuvable");
        else if (est_ko(&r->data[idx]))
            afficher_erreur("Ce personnage est K.O.");
        else if (deja_choisi(equipe, n, nom) || deja_choisi(autre, nb_autre, nom))
            afficher_erreur("Ce personnage est déjà engagé");
        else
            equipe[n++] = r->data[idx];
    }
    printf("\n");
    return n;
}
