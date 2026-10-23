/*
**
*/

/*
** ╭─ my_getnbr.c ────────────────────────────────────────────────────────
** │ But         Convertir le début d'une chaîne en entier : le signe
** │             « - » est accepté, la lecture s'arrête au premier
** │             caractère qui n'est pas un chiffre.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <limits.h>
#include <stddef.h>
#include "my.h"

int my_getnbr(char const *str)
{
    int i = 0;
    int negatif = 0;
    long long result = 0;

    if (str == NULL)
        return 0;
    if (str[i] == '-') {
        negatif = 1;
        i++;
    }
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        if (result > (long long)INT_MAX + negatif)
            return 0;
        i++;
    }
    return (int)(negatif ? -result : result);
}
