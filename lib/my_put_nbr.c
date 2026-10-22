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
    if (nb < 0) {
        my_putchar('-');
        nb = -nb;
    }
    if (nb >= 10) {
        my_put_nbr(nb / 10);
    }
    my_putchar(nb % 10 + '0');
    return 0;
}
