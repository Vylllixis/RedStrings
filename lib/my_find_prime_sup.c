/*
**
*/

/*
** ╭─ my_find_prime_sup.c ────────────────────────────────────────────────
** │ But         Trouver le plus petit nombre premier supérieur ou égal à
** │             nb.
** │ Utilise     my_is_prime
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <limits.h>
#include "my.h"

int my_find_prime_sup(int nb)
{
    if (nb <= 2)
        return 2;
    while (nb < INT_MAX && !my_is_prime(nb))
        nb++;
    return my_is_prime(nb) ? nb : 0;
}
