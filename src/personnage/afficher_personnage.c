/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "personnage.h"

#define LARGEUR_BARRE 20

static void repeter(char *dest, size_t taille, const char *motif, int fois)
{
    size_t n = 0;

    dest[0] = '\0';
    for (int i = 0; i < fois && n < taille; i++)
        n += (size_t)snprintf(dest + n, taille - n, "%s", motif);
}

static void afficher_stat(const char *nom, int valeur, const char *couleur)
{
    int remplies = (valeur + 4) / 5;
    char pleine[64];
    char vide[64];

    if (remplies > LARGEUR_BARRE)
        remplies = LARGEUR_BARRE;
    repeter(pleine, sizeof(pleine), "█", remplies);
    repeter(vide, sizeof(vide), "░", LARGEUR_BARRE - remplies);
    boite_ligne(GRIS, "  %-4s %s%s" GRIS "%s" RESET "  " GRAS "%2d" RESET,
        nom, couleur, pleine, vide, valeur);
}

void afficher_personnage(const personnage_t *p)
{
    const char *nom_classe = "Guerrier";
    const char *couleur_classe = ROUGE;

    if (p->classe == MAGE) {
        nom_classe = "Mage";
        couleur_classe = MAGENTA;
    } else if (p->classe == ARCHER) {
        nom_classe = "Archer";
        couleur_classe = VERT;
    }
    printf("\n");
    boite_haut(GRIS);
    boite_ligne(GRIS, "  " GRAS "%s" RESET GRIS "  ·  " RESET "%s%s" RESET,
        p->nom, couleur_classe, nom_classe);
    boite_separateur(GRIS);
    afficher_stat("PV", p->pv, VERT);
    afficher_stat("ATK", p->atq, ROUGE);
    afficher_stat("DEF", p->def, BLEU);
    boite_bas(GRIS);
    printf("\n");
}
