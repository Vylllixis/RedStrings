/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include "my.h"

void attaquer(personnage_t *attaquant, personnage_t *cible)
{
    // [calcul_degats]
    int degats = attaquant->atk * (100 - cible->def) / 100;

    if (degats < 0)
        degats = 0;
    cible->pv -= degats;
    if (cible->pv < 0)
        cible->pv = 0;
    // [calcul_degats]
}
