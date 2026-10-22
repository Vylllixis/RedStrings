/*
**
*/

/*
** ╭─ my_compute_power_rec.c ─────────────────────────────────────────────
** │ But         Calculer nb puissance power par récursivité : nb x
** │             nb^(power-1). Renvoie 0 si power est négatif.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"

int my_compute_power_rec(int nb, int power)
{
    if (power < 0)
        return 0;
    if (power == 0)
        return 1;
    return nb * my_compute_power_rec(nb, power - 1);
}
