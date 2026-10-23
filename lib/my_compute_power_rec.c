/*
**
*/

/*
** ╭─ my_compute_power_rec.c ─────────────────────────────────────────────
** │ But         Calculer nb puissance power par récursivité : nb x
** │             nb^(power-1). Renvoie 0 si power est négatif.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <limits.h>
#include "my.h"

int my_compute_power_rec(int nb, int power)
{
    long long reste = 0;

    if (power < 0)
        return 0;
    if (power == 0)
        return 1;
    reste = my_compute_power_rec(nb, power - 1);
    if (reste == 0 || reste * nb > INT_MAX || reste * nb < INT_MIN)
        return 0;
    return (int)(reste * nb);
}
