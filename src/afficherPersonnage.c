/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include "my.h"

#define LARGEUR_BARRE 20

//affiche une stat sous forme de barre : PV   ██████████░░░░░░░░░░  50
static void afficherStat(const char *nom, int valeur, const char *couleur)
{
    int remplies = (valeur + 4) / 5;

    printf(GRIS "│" RESET "  %-4s ", nom);
    printf("%s", couleur);
    for (int i = 0; i < remplies; i++)
        printf("█");
    printf(GRIS);
    for (int i = remplies; i < LARGEUR_BARRE; i++)
        printf("░");
    printf(RESET "  " GRAS "%2d" RESET "\n", valeur);
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
