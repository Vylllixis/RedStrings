/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ redstrings.c ───────────────────────────────────────────────────────
** │ But         Menu principal du jeu en terminal : affiche les choix, lit
** │             le choix du joueur, puis appelle l'action demandée.
** │ Contenu     ligne_menu, afficher_menu, redstrings
** │ Utilise     menu_personnages.c (les actions), roster.c (la liste des
** │             personnages), bataille_console.c (le combat), saisie.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include "bataille_console.h"
#include "boite.h"
#include "couleurs.h"
#include "menu_personnages.h"
#include "redstrings.h"
#include "roster.h"
#include "saisie.h"

/* Une ligne du menu ; grisée quand l'entrée n'a pas de sens pour l'instant. */
static void ligne_menu(int numero, const char *libelle, int disponible)
{
    boite_ligne(CYAN, "  %s%d" RESET "  %s%s" RESET, disponible ? GRAS : GRIS, numero,
        disponible ? "" : GRIS, libelle);
}

static void afficher_menu(const roster_t *r)
{
    if (!roster_valide(r))
        return;
    boite_haut(CYAN);
    boite_ligne(CYAN, "  " GRAS "✦ Menu principal" RESET GRIS "  %d personnage%s" RESET,
        r->len, r->len > 1 ? "s" : "");
    boite_separateur(CYAN);
    ligne_menu(CHOIX_AJOUTER, "Ajouter un personnage", 1);
    ligne_menu(CHOIX_AFFICHER, "Afficher un personnage",
        menu_indisponible(r, CHOIX_AFFICHER) == NULL);
    ligne_menu(CHOIX_COMBAT, "Combat", menu_indisponible(r, CHOIX_COMBAT) == NULL);
    ligne_menu(CHOIX_SAUVEGARDER, "Sauvegarder",
        menu_indisponible(r, CHOIX_SAUVEGARDER) == NULL);
    ligne_menu(CHOIX_CHARGER, "Charger (remplace tout)", 1);
    ligne_menu(CHOIX_FUSIONNER, "Ajouter depuis le fichier", 1);
    ligne_menu(CHOIX_QUITTER, "Quitter", 1);
    boite_bas(CYAN);
}

/*
** ╭─ redstrings ─────────────────────────────────────────────────────────
** │ Boucle du menu principal, jusqu'à ce que le joueur choisisse 0.
** │
** │ 1. roster_init() prépare la liste des personnages (vide)
** │ 2. affiche le menu (les entrées sans effet sont grisées) et lit un choix
** │    entre 0 et 6 (Ctrl+D = quitter)
** │ 3. si l'entrée est indisponible, menu_indisponible() donne la raison et
** │    rien ne se lance ; sinon appelle l'action qui correspond
** │ 4. en sortant, roster_liberer() rend la mémoire : aucune fuite
** │ Renvoie 0, le code de sortie du programme.
** ╰──────────────────────────────────────────────────────────────────────
*/
int redstrings(void)
{
    roster_t roster;
    int choix = -1;
    const char *raison = NULL;

    roster_init(&roster);
    while (choix != CHOIX_QUITTER) {
        afficher_menu(&roster);
        if (lire_entier_menu("Choix", 0, NB_CHOIX - 1, &choix) != 0)
            choix = CHOIX_QUITTER;
        printf("\n");
        raison = menu_indisponible(&roster, choix);
        if (raison != NULL)
            afficher_erreur(raison);
        else if (choix == CHOIX_AJOUTER)
            menu_ajouter(&roster);
        else if (choix == CHOIX_AFFICHER)
            menu_afficher(&roster);
        else if (choix == CHOIX_COMBAT)
            menu_bataille(&roster);
        else if (choix == CHOIX_SAUVEGARDER)
            menu_sauvegarder(&roster);
        else if (choix == CHOIX_CHARGER)
            menu_charger(&roster);
        else if (choix == CHOIX_FUSIONNER)
            menu_fusionner(&roster);
    }
    roster_liberer(&roster);
    printf("  " GRIS "À bientôt !" RESET "\n");
    return 0;
}
