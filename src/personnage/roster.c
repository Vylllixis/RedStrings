/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdlib.h>
#include "roster.h"

void roster_init(roster_t *r)
{
    r->data = NULL;
    r->len = 0;
    r->capacite = 0;
}

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
