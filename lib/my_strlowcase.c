/*
**
*/

/*
** ╭─ my_strlowcase.c ────────────────────────────────────────────────────
** │ But         Passer une chaîne en minuscules ; les autres caractères
** │             ne changent pas.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"
#include "my_string.h"

char *my_strlowcase(char *str)
{
    if (str == NULL)
        return NULL;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] += 32;
        }
    }
    return str;
}
