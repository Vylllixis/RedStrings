/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <ctype.h>
#include "gui.h"

/** Même recherche que celle de la version terminal (déclarée dans personnage.h). */
int trouver_personnage(const personnage_t *tab, int nb, const char *nom)
{
    for (int i = 0; i < nb; i++) {
        if (my_strcmp(tab[i].nom, nom) == 0)
            return i;
    }
    return -1;
}

/** Même calcul que attaquer() de la version terminal, sans affichage. */
int appliquer_attaque(personnage_t *attaquant, personnage_t *cible)
{
    int degats = attaquant->atq * (100 - cible->def) / 100;

    if (degats < 0)
        degats = 0;
    cible->pv -= degats;
    if (cible->pv < 0)
        cible->pv = 0;
    return degats;
}

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
