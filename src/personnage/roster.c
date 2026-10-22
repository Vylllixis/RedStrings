/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ roster.c ───────────────────────────────────────────────────────────
** │ But         La liste des personnages (roster) : un tableau qui
** │             grandit tout seul avec realloc.
** │ Contenu     roster_init, roster_ajouter, roster_liberer
** │ Utilisé par redstrings.c, sauvegarde.c, bataille_console.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdlib.h>
#include "roster.h"

void roster_init(roster_t *r)
{
    r->data = NULL;
    r->len = 0;
    r->capacite = 0;
}

/*
** ╭─ roster_ajouter ─────────────────────────────────────────────────────
** │ Ajoute une copie du personnage à la fin de la liste.
** │
** │ 1. si la liste est pleine, la capacité double (ou vaut
** │    ROSTER_CAPACITE_INITIALE au départ)
** │ 2. realloc() écrit d'abord dans tmp : s'il échoue, r->data reste
** │    valide, rien n'est perdu et la fonction renvoie -1
** │ 3. sinon r->data = tmp, puis on copie le personnage
** │ Renvoie 0 si OK, -1 si la mémoire manque.
** ╰──────────────────────────────────────────────────────────────────────
*/
int roster_ajouter(roster_t *r, const personnage_t *p)
{
    int nouvelle = 0;
    personnage_t *tmp = NULL;

    if (r->len == r->capacite) {
        nouvelle = r->capacite == 0 ? ROSTER_CAPACITE_INITIALE
            : r->capacite * 2;
        tmp = realloc(r->data, (size_t)nouvelle * sizeof(*tmp));
        if (tmp == NULL)
            return -1;      // r->data reste valide : pas de fuite
        r->data = tmp;
        r->capacite = nouvelle;
    }
    r->data[r->len++] = *p;
    return 0;
}

void roster_liberer(roster_t *r)
{
    free(r->data);
    roster_init(r);
}
