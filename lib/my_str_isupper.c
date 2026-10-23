/*
**
*/

/*
** ╭─ my_str_isupper.c ───────────────────────────────────────────────────
** │ But         Vérifier que la chaîne ne contient que des majuscules.
** │             Renvoie 1 si c'est vrai, 0 sinon.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"
#include "my_string.h"

int my_str_isupper(char const *str)
{
    if (str == NULL)
        return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (!(str[i] >= 'A' && str[i] <= 'Z'))
            return 0;
    }
    return 1;
}
