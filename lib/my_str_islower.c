/*
**
*/

/*
** ╭─ my_str_islower.c ───────────────────────────────────────────────────
** │ But         Vérifier que la chaîne ne contient que des minuscules.
** │             Renvoie 1 si c'est vrai, 0 sinon.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"
#include "my_string.h"

int my_str_islower(char const *str)
{
    if (str == NULL)
        return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (!(str[i] >= 'a' && str[i] <= 'z'))
            return 0;
    }
    return 1;
}
