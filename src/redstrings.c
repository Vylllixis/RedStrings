/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include "my.h"

#define MAX_PERSONNAGES 10

static void afficherErreur(const char *message)
{
    printf("  " ROUGE "✗ %s" RESET "\n\n", message);
}

static void afficherMenu(void)
{
    boiteHaut(CYAN);
    boiteLigne(CYAN, "  " GRAS "✦ Menu principal" RESET);
    boiteSeparateur(CYAN);
    boiteLigne(CYAN, "  " GRAS "1" RESET "  Ajouter un personnage");
    boiteLigne(CYAN, "  " GRAS "0" RESET "  Quitter");
    boiteBas(CYAN);
}

static int lireChoix(void)
{
    int choix = 0;

    printf("  " GRAS "Choix" RESET CYAN " › " RESET);
    while (scanf("%d", &choix) != 1) {
        viderBuffer();
        afficherErreur("Tape un nombre entre 0 et 3");
        printf("  " GRAS "Choix" RESET CYAN " › " RESET);
    }
    viderBuffer();
    return choix;
}

static void menuAjouter(personnage_t *tab, int *len)
{
    if (*len >= MAX_PERSONNAGES) {
        afficherErreur("Nombre maximum de personnages atteint");
        return;
    }
    creerPersonnage(&tab[*len]);
    (*len)++;
    printf("\n");
}

int redstrings(void)
{
    personnage_t tabPersonnage[MAX_PERSONNAGES];
    int len = 0;
    int choix = -1;

    while (choix != 0) {
        afficherMenu();
        choix = lireChoix();
        printf("\n");
        if (choix == 1)
            menuAjouter(tabPersonnage, &len);
        else if (choix != 0)
            afficherErreur("Choix inconnu");
    }
    return 0;
}
