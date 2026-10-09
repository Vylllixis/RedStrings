/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include "my.h"

int redstrings(void)
{
    personnage_t *a = malloc(sizeof(personnage_t));
    personnage_t *b = malloc(sizeof(personnage_t));

    creerPersonnage(a);
    creerPersonnage(b);
    afficherPersonnage(a);
    afficherPersonnage(b);
    attaquer(a, b);
    afficherPersonnage(a);
    afficherPersonnage(b);
    return 0;
}
