/*
**
*/

/*
** ╭─ my_revstr.c ────────────────────────────────────────────────────────
** │ But         Inverser une chaîne sur place : le premier caractère est
** │             échangé avec le dernier, le deuxième avec
** │             l'avant-dernier, etc.
** │ Utilise     my_strlen
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"
#include "my_string.h"

char *my_revstr(char *str)
{
    int length = my_strlen(str);

    if (str == NULL)
        return NULL;
    for (int i = 0; i < length / 2; i++) {
        char temp = str[i];
        str[i] = str[length - i - 1];
        str[length - i - 1] = temp;
    }
    return str;
}
