/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include "personnage.h"

int calculer_degats(const personnage_t *attaquant, const personnage_t *cible)
{
    int degats = attaquant->atq * (100 - cible->def) / 100;

    return degats < 0 ? 0 : degats;
}

int est_ko(const personnage_t *p)
{
    return p->pv == 0;
}
