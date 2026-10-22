/*
**
*/

/*
** ╭─ my_strlowcase.c ────────────────────────────────────────────────────
** │ But         Passer une chaîne en minuscules ; les autres caractères
** │             ne changent pas.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"
#include "my_string.h"

char *my_strlowcase(char *str)
{
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] += 32;
        }
    }
    return str;
}
