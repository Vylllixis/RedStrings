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

#include "my.h"

int my_getnbr(char const *str)
{
    int i = 0;
    int sign = 1;
    int result = 0;

    if (str[i] == '-') {
        sign = -1;
        i++;
    }
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }
    return result * sign;
}
