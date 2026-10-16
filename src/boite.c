/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include "my.h"

void boiteHaut(const char *couleur)
{
    printf("%s╭" LIGNE "╮" RESET "\n", couleur);
}

void boiteSeparateur(const char *couleur)
{
    printf("%s├" LIGNE "┤" RESET "\n", couleur);
}

void boiteBas(const char *couleur)
{
    printf("%s╰" LIGNE "╯" RESET "\n", couleur);
}
