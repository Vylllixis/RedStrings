/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "bataille_console.h"
#include "boite.h"
#include "couleurs.h"
#include "personnage.h"
#include "redstrings.h"
#include "roster.h"
#include "sauvegarde.h"
#include "saisie.h"

static void afficher_menu(void)
{
    boite_haut(CYAN);
    boite_ligne(CYAN, "  " GRAS "✦ Menu principal" RESET);
    boite_separateur(CYAN);
    boite_ligne(CYAN, "  " GRAS "1" RESET "  Ajouter un personnage");
    boite_ligne(CYAN, "  " GRAS "2" RESET "  Afficher un personnage");
    boite_ligne(CYAN, "  " GRAS "3" RESET "  Combat");
    boite_ligne(CYAN, "  " GRAS "4" RESET "  Sauvegarder");
    boite_ligne(CYAN, "  " GRAS "5" RESET "  Charger");
    boite_ligne(CYAN, "  " GRAS "0" RESET "  Quitter");
    boite_bas(CYAN);
}

static int demander_personnage(const roster_t *r, const char *question)
{
    char nom[30] = "";
    int idx = 0;

    printf("  " GRAS "%s" RESET CYAN " › " RESET, question);
    if (scanf("%29s", nom) != 1) {
        afficher_erreur("Saisie interrompue");
        return -1;
    }
    vider_buffer();
    idx = trouver_personnage(r->data, r->len, nom);
    if (idx == -1)
        afficher_erreur("Personnage introuvable");
    return idx;
}

static void menu_ajouter(roster_t *r)
{
    personnage_t nouveau = {0};

    if (creer_personnage(&nouveau, r->data, r->len) == 0
        && roster_ajouter(r, &nouveau) == -1)
        afficher_erreur("Mémoire insuffisante");
    printf("\n");
}

static void menu_afficher(const roster_t *r)
{
    int idx = demander_personnage(r, "Nom du personnage");

    if (idx != -1)
        afficher_personnage(&r->data[idx]);
}

static void menu_sauvegarder(const roster_t *r)
{
    sauvegarde_t code = sauvegarder(r, FICHIER_SAUVEGARDE);

    if (code != SAUVEGARDE_OK)
        afficher_erreur(sauvegarde_message(code));
    else
        printf("  " VERT "✔ %d personnage(s) sauvegardé(s) dans %s" RESET "\n\n",
            r->len, FICHIER_SAUVEGARDE);
}

static void menu_charger(roster_t *r)
{
    sauvegarde_t code = charger(r, FICHIER_SAUVEGARDE);

    if (code != SAUVEGARDE_OK)
        afficher_erreur(sauvegarde_message(code));
    else
        printf("  " VERT "✔ %d personnage(s) chargé(s) depuis %s" RESET "\n\n",
            r->len, FICHIER_SAUVEGARDE);
}

int redstrings(void)
{
    roster_t roster;
    int choix = -1;

    roster_init(&roster);

    while (choix != 0) {
        afficher_menu();
        if (lire_entier("Choix", 0, 5, &choix) == -1)
            choix = 0;
        printf("\n");
        if (choix == 1)
            menu_ajouter(&roster);
        else if (choix == 2)
            menu_afficher(&roster);
        else if (choix == 3)
            menu_bataille(&roster);
        else if (choix == 4)
            menu_sauvegarder(&roster);
        else if (choix == 5)
            menu_charger(&roster);
    }
    roster_liberer(&roster);
    printf("  " GRIS "À bientôt !" RESET "\n");
    return 0;
}
