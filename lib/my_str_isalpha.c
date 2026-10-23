/*
**
*/

/*
** ╭─ my_str_isalpha.c ───────────────────────────────────────────────────
** │ But         Vérifier que la chaîne ne contient que des lettres.
** │             Renvoie 1 si c'est vrai (une chaîne vide aussi), 0 sinon.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"
#include "my_string.h"

int my_str_isalpha(char const *str)
{
    if (str == NULL)
        return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (!((str[i] >= 'a' && str[i] <= 'z') || (str[i] >= 'A' && str[i] <= 'Z')))
            return 0;
    }
    return 1;
}
