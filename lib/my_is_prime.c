/*
**
*/

/*
** ╭─ my_is_prime.c ──────────────────────────────────────────────────────
** │ But         Dire si nb est premier (1) ou non (0) : on teste les
** │             diviseurs seulement jusqu'à la racine carrée (i x i <=
** │             nb).
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"

int my_is_prime(int nb)
{
    if (nb <= 1)
        return 0;
    for (int i = 2; i * i <= nb; i++) {
        if (nb % i == 0)
            return 0;
    }
    return 1;
}
