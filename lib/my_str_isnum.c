/*
**
*/

/*
** ╭─ my_str_isnum.c ─────────────────────────────────────────────────────
** │ But         Vérifier que la chaîne ne contient que des chiffres.
** │             Renvoie 1 si c'est vrai, 0 sinon.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"
#include "my_string.h"

int my_str_isnum(char const *str)
{
    if (str == NULL)
        return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (!(str[i] >= '0' && str[i] <= '9'))
            return 0;
    }
    return 1;
}
