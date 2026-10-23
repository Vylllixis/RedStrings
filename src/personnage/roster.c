/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ roster.c ───────────────────────────────────────────────────────────
** │ But         La liste des personnages (roster) : un tableau qui
** │             grandit tout seul avec realloc.
** │ Contenu     roster_init, roster_ajouter, roster_liberer, roster_trouver,
** │             roster_nb_en_forme
** │ Utilisé par redstrings.c, sauvegarde.c, bataille_console.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdlib.h>
#include "my.h"
#include "my_string.h"
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

/*
** ╭─ roster_trouver ─────────────────────────────────────────────────────
** │ Retrouve un personnage à partir de ce que le joueur a tapé.
** │
** │ 1. le nom exact (même casse) : il a toujours priorité, même si le nom
** │    ressemble à un nombre
** │ 2. sinon un numéro de 1 à len : celui de la liste affichée à l'écran
** │ Le numéro n'est lu que s'il ne fait que des chiffres et au plus 9 :
** │ my_getnbr() ne protège pas contre un dépassement d'int.
** ╰──────────────────────────────────────────────────────────────────────
*/
int roster_trouver(const roster_t *r, const char *saisie)
{
    int numero = 0;

    for (int i = 0; i < r->len; i++) {
        if (my_strcmp(r->data[i].nom, saisie) == 0)
            return i;
    }
    if (saisie[0] == '\0' || my_strlen(saisie) > 9 || !my_str_isnum(saisie))
        return -1;
    numero = my_getnbr(saisie);
    return numero >= 1 && numero <= r->len ? numero - 1 : -1;
}

int roster_nb_en_forme(const roster_t *r)
{
    int n = 0;

    for (int i = 0; i < r->len; i++)
        n += !est_ko(&r->data[i]);
    return n;
}
