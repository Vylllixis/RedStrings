/*
**
*/

/*
** ╭─ my_put_nbr.c ───────────────────────────────────────────────────────
** │ But         Écrire un entier avec son signe : récursif, il écrit
** │             d'abord les chiffres de gauche (nb / 10), puis le dernier
** │             (nb % 10).
** │ Utilise     my_putchar
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"

int my_put_nbr(int nb)
{
    long long n = nb;

    if (n < 0) {
        my_putchar('-');
        n = -n;
    }
    if (n >= 10) {
        my_put_nbr((int)(n / 10));
    }
    my_putchar((char)(n % 10 + '0'));
    return 0;
}
