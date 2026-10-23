/*
**
*/

/*
** ╭─ my_compute_square_root.c ───────────────────────────────────────────
** │ But         Racine carrée entière exacte. Renvoie 0 si nb n'est pas
** │             un carré parfait ou s'il est négatif.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"

int my_compute_square_root(int nb)
{
    if (nb <= 0)
        return 0;
    for (long long i = 1; i * i <= nb; i++) {
        if (i * i == nb)
            return (int)i;
    }
    return 0;
}
