/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "personnage.h"
#include "saisie.h"

int creerPersonnage(personnage_t *p, personnage_t *tab, int len)
{
    int choix = 1;

    //en-tête
    printf("\n");
    boiteHaut(CYAN);
    boiteLigne(CYAN, "  " GRAS "✦ Création d'un personnage" RESET);
    boiteBas(CYAN);
    printf("\n");

    //choix du nom
    printf("  " GRAS "Nom" RESET CYAN " › " RESET);
    if (scanf("%29s", p->name) != 1)
        return -1;
    viderBuffer();
    if (findPersonnage(tab, len, p->name) != -1) {
        afficherErreur("Ce nom est déjà pris");
        return -1;
    }
    printf("\n");

    //choix des statistiques
    printf("  " GRIS "── Statistiques ──" RESET "\n");
    lireEntier("Points de vie", 0, 99, &p->pv);
    lireEntier("Attaque      ", 0, 99, &p->atk);
    lireEntier("Défense      ", 0, 99, &p->def);
    printf("\n");

    //choix de la classe (1-3 saisi, l'enum commence à 0)
    printf("  " GRIS "── Classe ──" RESET "\n");
    printf("    " GRAS "1" RESET "  " ROUGE "Guerrier" RESET "\n");
    printf("    " GRAS "2" RESET "  " MAGENTA "Mage" RESET "\n");
    printf("    " GRAS "3" RESET "  " VERT "Archer" RESET "\n");
    lireEntier("Choix", 1, 3, &choix);
    p->classe = (Classe_t)(choix - 1);

    printf("\n  " VERT "✔ %s a été créé !" RESET "\n", p->name);
    return 0;
}
