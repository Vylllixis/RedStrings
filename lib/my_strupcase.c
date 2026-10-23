/*
**
*/

/*
** ╭─ my_strupcase.c ─────────────────────────────────────────────────────
** │ But         Passer une chaîne en majuscules ; les autres caractères
** │             ne changent pas (en ASCII, une minuscule vaut sa
** │             majuscule + 32).
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"
#include "my_string.h"

char *my_strupcase(char *str)
{
    if (str == NULL)
        return NULL;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] -= 32;
        }
    }
    return str;
}
