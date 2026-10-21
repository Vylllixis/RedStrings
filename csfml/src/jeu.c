/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <ctype.h>
#include "gui.h"

/** Recherche insensible à la casse de `recherche` dans `nom`. */
int contient_nom(const char *nom, const char *recherche)
{
    int j = 0;

    if (recherche[0] == '\0')
        return 1;
    for (int i = 0; nom[i] != '\0'; i++) {
        for (j = 0; recherche[j] != '\0' && nom[i + j] != '\0'; j++) {
            if (tolower((unsigned char)nom[i + j])
                != tolower((unsigned char)recherche[j]))
                break;
        }
        if (recherche[j] == '\0')
            return 1;
    }
    return 0;
}
