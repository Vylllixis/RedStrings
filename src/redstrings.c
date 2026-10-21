/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "my_string.h"
#include "personnage.h"
#include "redstrings.h"
#include "saisie.h"

#define MAX_PERSONNAGES 10

int trouver_personnage(const personnage_t *tab, int size, const char *nom)
{
    for (int i = 0; i < size; i++) {
        if (my_strcmp(tab[i].nom, nom) == 0)
            return i;
    }
    return -1;
}

static void afficher_menu(void)
{
    boite_haut(CYAN);
    boite_ligne(CYAN, "  " GRAS "✦ Menu principal" RESET);
    boite_separateur(CYAN);
    boite_ligne(CYAN, "  " GRAS "1" RESET "  Ajouter un personnage");
    boite_ligne(CYAN, "  " GRAS "2" RESET "  Afficher un personnage");
    boite_ligne(CYAN, "  " GRAS "3" RESET "  Combat");
    boite_ligne(CYAN, "  " GRAS "0" RESET "  Quitter");
    boite_bas(CYAN);
}

static int demander_personnage(personnage_t *tab, int len, const char *question)
{
    char nom[30] = "";
    int idx = 0;

    printf("  " GRAS "%s" RESET CYAN " › " RESET, question);
    if (scanf("%29s", nom) != 1) {
        afficher_erreur("Saisie interrompue");
        return -1;
    }
    vider_buffer();
    idx = trouver_personnage(tab, len, nom);
    if (idx == -1)
        afficher_erreur("Personnage introuvable");
    return idx;
}

static void menu_ajouter(personnage_t *tab, int *len)
{
    if (*len >= MAX_PERSONNAGES) {
        afficher_erreur("Nombre maximum de personnages atteint");
        return;
    }
    if (creer_personnage(&tab[*len], tab, *len) == 0)
        (*len)++;
    printf("\n");
}

static void menu_afficher(personnage_t *tab, int len)
{
    int idx = demander_personnage(tab, len, "Nom du personnage");

    if (idx != -1)
        afficher_personnage(&tab[idx]);
}

static void menu_combat(personnage_t *tab, int len)
{
    int id_atq = demander_personnage(tab, len, "Attaquant");
    int id_def = 0;

    if (id_atq == -1)
        return;
    if (tab[id_atq].pv == 0) {
        afficher_erreur("Ce personnage est mort");
        return;
    }
    id_def = demander_personnage(tab, len, "Défenseur");
    if (id_def == -1)
        return;
    if (tab[id_def].pv == 0) {
        afficher_erreur("Ce personnage est mort");
        return;
    }
    if (id_atq == id_def) {
    afficher_erreur("Un personnage ne peut pas s'attaquer lui-même");
        return;
    }
    attaquer(&tab[id_atq], &tab[id_def]);
}

int redstrings(void)
{
    personnage_t tab_personnage[MAX_PERSONNAGES];
    int len = 0;
    int choix = -1;

    while (choix != 0) {
        afficher_menu();
        if (lire_entier("Choix", 0, 3, &choix) == -1)
            choix = 0;
        printf("\n");
        if (choix == 1)
            menu_ajouter(tab_personnage, &len);
        else if (choix == 2)
            menu_afficher(tab_personnage, len);
        else if (choix == 3)
            menu_combat(tab_personnage, len);
    }
    printf("  " GRIS "À bientôt !" RESET "\n");
    return 0;
}
