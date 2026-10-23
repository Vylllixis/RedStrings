/*
**
*/

/*
** ╭─ my_putstr.c ────────────────────────────────────────────────────────
** │ But         Écrire une chaîne, caractère par caractère.
** │ Utilise     my_putchar
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"

int my_putstr(char const *str)
{
    if (str == NULL)
        return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        my_putchar(str[i]);
    }
    return 0;
}
