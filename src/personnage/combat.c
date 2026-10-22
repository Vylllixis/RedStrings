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
    int degats = attaquant->atq * (100 - cible->def) / 100;

    return degats < 0 ? 0 : degats;
}

int est_ko(const personnage_t *p)
{
    return p->pv == 0;
}
