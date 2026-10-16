/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "personnage.h"
#include "saisie.h"

static void afficherQuestion(const char *question, int min, int max)
{
    printf("  " GRAS "%s" RESET GRIS " (%d-%d)" RESET CYAN " › " RESET,
        question, min, max);
}

static int lireEntier(const char *question, int min, int max)
{
    int valeur = 0;
    int ok = 0;

    afficherQuestion(question, min, max);
    ok = scanf("%d", &valeur);
    viderBuffer();
    while (ok != 1 || valeur < min || valeur > max) {
        printf("  " ROUGE "✗ Valeur indisponible, tape un nombre entre %d et %d"
            RESET "\n", min, max);
        afficherQuestion(question, min, max);
        ok = scanf("%d", &valeur);
        viderBuffer();
    }
    return valeur;
}

int creerPersonnage(personnage_t *p)
{
    //en-tête
    printf("\n");
    boiteHaut(CYAN);
    boiteLigne(CYAN, "  " GRAS "✦ Création d'un personnage" RESET);
    boiteBas(CYAN);
    printf("\n");

    //choix du nom
    printf("  " GRAS "Nom" RESET CYAN " › " RESET);
    scanf("%29s", p->name);
    viderBuffer();
    printf("\n");

    //choix des statistiques
    printf("  " GRIS "── Statistiques ──" RESET "\n");
    p->pv = lireEntier("Points de vie", 0, 99);
    p->atk = lireEntier("Attaque      ", 0, 99);
    p->def = lireEntier("Défense      ", 0, 99);
    printf("\n");

    //choix de la classe (1-3 saisi, l'enum commence à 0)
    printf("  " GRIS "── Classe ──" RESET "\n");
    printf("    " GRAS "1" RESET "  " ROUGE "Guerrier" RESET "\n");
    printf("    " GRAS "2" RESET "  " MAGENTA "Mage" RESET "\n");
    printf("    " GRAS "3" RESET "  " VERT "Archer" RESET "\n");
    p->classe = lireEntier("Choix", 1, 3) - 1;

    printf("\n  " VERT "✔ %s a été créé !" RESET "\n", p->name);
    return 0;
}
