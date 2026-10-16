/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "personnage.h"

#define LARGEUR_BARRE 20

static void repeter(char *dest, const char *motif, int fois)
{
    int n = 0;

    dest[0] = '\0';
    for (int i = 0; i < fois; i++)
        n += sprintf(dest + n, "%s", motif);
}

static void afficherStat(const char *nom, int valeur, const char *couleur)
{
    int remplies = (valeur + 4) / 5;
    char pleine[64];
    char vide[64];

    repeter(pleine, "█", remplies);
    repeter(vide, "░", LARGEUR_BARRE - remplies);
    boiteLigne(GRIS, "  %-4s %s%s" GRIS "%s" RESET "  " GRAS "%2d" RESET,
        nom, couleur, pleine, vide, valeur);
}

int afficherPersonnage(const personnage_t *p)
{
    const char *nameClasse = "Guerrier";
    const char *couleurClasse = ROUGE;

    if (p->classe == MAGE) {
        nameClasse = "Mage";
        couleurClasse = MAGENTA;
    } else if (p->classe == ARCHER) {
        nameClasse = "Archer";
        couleurClasse = VERT;
    }
    printf("\n");
    boiteHaut(GRIS);
    boiteLigne(GRIS, "  " GRAS "%s" RESET GRIS "  ·  " RESET "%s%s" RESET,
        p->name, couleurClasse, nameClasse);
    boiteSeparateur(GRIS);
    afficherStat("PV", p->pv, VERT);
    afficherStat("ATK", p->atk, ROUGE);
    afficherStat("DEF", p->def, BLEU);
    boiteBas(GRIS);
    printf("\n");
    return 0;
}
