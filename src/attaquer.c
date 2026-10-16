/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include "my.h"

void attaquer(personnage_t *attaquant, personnage_t *cible)
{
    cible->pv -= attaquant->atk * (100 - cible->def) / 100;
}
