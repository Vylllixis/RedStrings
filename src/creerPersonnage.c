/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include "my.h"

//vide le reste de la ligne tapée (lettres, espaces, retour à la ligne)
static void viderBuffer(void)
{
    int c = getchar();

    while (c != '\n' && c != EOF)
        c = getchar();
}

//redemande tant que la saisie n'est pas un nombre entre min et max
static int lireEntier(const char *question, int min, int max)
{
    int valeur = 0;
    int ok = 0;

    printf("%s\n- ", question);
    ok = scanf("%d", &valeur);
    viderBuffer();
    printf("\n");
    while (ok != 1 || valeur < min || valeur > max) {
        printf("Valeur indisponible,\n%s\n- ", question);
        ok = scanf("%d", &valeur);
        viderBuffer();
        printf("\n");
    }
    return valeur;
}

int creerPersonnage(personnage_t *p)
{
    //choix du nom
    printf("Quel est le nom de ce personnage ?\n- ");
    scanf("%29s", p->nom);
    viderBuffer();
    printf("\n");

    //choix des points de vie
    p->pv = lireEntier("Combien a-t-il de points de vie ? (0-99)", 0, 99);

    //choix de la puissance
    p->atk = lireEntier("Quelle est sa puissance d'attaque ? (0-99)", 0, 99);

    //choix de la défense
    p->def = lireEntier("Combien de points de défense a-t-il ? (0-99)", 0, 99);

    //choix de la classe (1-3 saisi, l'enum commence à 0)
    p->classe = lireEntier("Quelle classe a-t-il ? (1 - GUERRIER, 2 - MAGE, 3 - ARCHER)", 1, 3) - 1;

    return 0;
}
