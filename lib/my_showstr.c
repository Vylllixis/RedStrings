/*
**
*/

/*
** ╭─ my_showstr.c ───────────────────────────────────────────────────────
** │ But         Écrire une chaîne ; un caractère non imprimable (hors
** │             ASCII 32 à 126) est écrit « \ » suivi de son code.
** │ Utilise     my_putchar, my_put_nbr
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"

int my_showstr(char const *str)
{
    int i = 0;

    if (str == NULL)
        return 0;
    while (str[i] != '\0') {
        if (str[i] < 32 || str[i] > 126) {
            my_putchar('\\');
            my_put_nbr((unsigned char)str[i]);
        } else {
            my_putchar(str[i]);
        }
        i++;
    }
    return 0;
}
