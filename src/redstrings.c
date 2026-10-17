/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "my.h"
#include "personnage.h"
#include "redstrings.h"
#include "saisie.h"

#define MAX_PERSONNAGES 10

static int findPersonage(personnage_t *tab, int size, char *name)
{
    for (int i = 0; i < size; i++) {
        if (my_strcmp(tab[i].name, name) == 0)
            return i;
    }
    return -1;
}

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
    boiteLigne(CYAN, "  " GRAS "2" RESET "  Afficher un personnage");
    boiteLigne(CYAN, "  " GRAS "3" RESET "  Combat");
    boiteLigne(CYAN, "  " GRAS "0" RESET "  Quitter");
    boiteBas(CYAN);
}

static int lireChoix(void)
{
    int choix = 0;
    int ret = scanf("%d", &choix);

    printf("  " GRAS "Choix" RESET CYAN " › " RESET);
    while (ret != 1) {
        if (ret == EOF)
            return 0;
        viderBuffer();
        afficherErreur("Tape un nombre entre 0 et 3");
        printf("  " GRAS "Choix" RESET CYAN " › " RESET);
        ret = scanf("%d", &choix);
    }
    viderBuffer();
    return choix;
}

static int demanderPersonnage(personnage_t *tab, int len, const char *question)
{
    char nom[30];
    int idx = 0;

    printf("  " GRAS "%s" RESET CYAN " › " RESET, question);
    scanf("%29s", nom);
    viderBuffer();
    idx = findPersonage(tab, len, nom);
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
    creerPersonnage(&tab[*len]);
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
        choix = lireChoix();
        printf("\n");
        if (choix == 1)
            menuAjouter(tabPersonnage, &len);
        else if (choix == 2)
            menuAfficher(tabPersonnage, len);
        else if (choix == 3)
            menuCombat(tabPersonnage, len);
        else if (choix != 0)
            afficherErreur("Choix inconnu");
    }
    printf("  " GRIS "À bientôt !" RESET "\n");
    return 0;
}
