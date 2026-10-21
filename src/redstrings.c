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

int findPersonnage(const personnage_t *tab, int size, const char *name)
{
    for (int i = 0; i < size; i++) {
        if (my_strcmp(tab[i].name, name) == 0)
            return i;
    }
    return -1;
}

static void afficherMenu(void)
{
    boiteHaut(CYAN);
    boiteLigne(CYAN, "  " GRAS "✦ Menu principal" RESET);
    boiteSeparateur(CYAN);
    boiteLigne(CYAN, "  " GRAS "1" RESET "  Ajouter un personnage");
    boiteLigne(CYAN, "  " GRAS "2" RESET "  Afficher un personnage");
    boiteLigne(CYAN, "  " GRAS "3" RESET "  Combat");
    boiteLigne(CYAN, "  " GRAS "0" RESET "  Quitter");
    boiteBas(CYAN);
}

static int demanderPersonnage(personnage_t *tab, int len, const char *question)
{
    char nom[30];
    int idx = 0;

    printf("  " GRAS "%s" RESET CYAN " › " RESET, question);
    scanf("%29s", nom);
    viderBuffer();
    idx = findPersonnage(tab, len, nom);
    if (idx == -1)
        afficherErreur("Personnage introuvable");
    return idx;
}

static void menuAjouter(personnage_t *tab, int *len)
{
    if (*len >= MAX_PERSONNAGES) {
        afficherErreur("Nombre maximum de personnages atteint");
        return;
    }
    if (creerPersonnage(&tab[*len], tab, *len) == 0)
        (*len)++;
    printf("\n");
}

static void menuAfficher(personnage_t *tab, int len)
{
    int idx = demanderPersonnage(tab, len, "Nom du personnage");

    if (idx != -1)
        afficherPersonnage(&tab[idx]);
}

static void menuCombat(personnage_t *tab, int len)
{
    int idAtk = demanderPersonnage(tab, len, "Attaquant");
    int idDef = 0;

    if (idAtk == -1)
        return;
    idDef = demanderPersonnage(tab, len, "Défenseur");
    if (idDef == -1)
        return;
    attaquer(&tab[idAtk], &tab[idDef]);
}

int redstrings(void)
{
    personnage_t tabPersonnage[MAX_PERSONNAGES];
    int len = 0;
    int choix = -1;

    while (choix != 0) {
        afficherMenu();
        if (lireEntier("Choix", 0, 3, &choix) == -1)
            choix = 0;
        printf("\n");
        if (choix == 1)
            menuAjouter(tabPersonnage, &len);
        else if (choix == 2)
            menuAfficher(tabPersonnage, len);
        else if (choix == 3)
            menuCombat(tabPersonnage, len);
    }
    printf("  " GRIS "À bientôt !" RESET "\n");
    return 0;
}
