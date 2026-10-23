/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ combat.c ───────────────────────────────────────────────────────────
** │ But         Les deux règles de base d'un duel : combien de dégâts, et
** │             qui est K.O.
** │ Contenu     calculer_degats, est_ko
** │ Utilisé par bataille.c, bataille_console.c, tests.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <limits.h>
#include <stddef.h>
#include "personnage.h"

/*
** ╭─ calculer_degats ────────────────────────────────────────────────────
** │ dégâts = ATQ de l'attaquant x (100 - DEF de la cible) / 100
** │
** │ La DEF est un pourcentage de réduction : DEF 20 retire 20 % des
** │ dégâts.
** │ Le résultat n'est jamais négatif (une DEF supérieure à 100 donne 0).
** ╰──────────────────────────────────────────────────────────────────────
*/
int calculer_degats(const personnage_t *attaquant, const personnage_t *cible)
{
    long long degats = 0;

    if (attaquant == NULL || cible == NULL)
        return 0;
    degats = (long long)attaquant->atq * (100 - (long long)cible->def) / 100;
    if (degats < 0)
        return 0;
    return degats > INT_MAX ? INT_MAX : (int)degats;
}

int est_ko(const personnage_t *p)
{
    return p == NULL || p->pv <= 0;
}
