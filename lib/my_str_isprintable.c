/*
**
*/

/*
** ╭─ my_str_isprintable.c ───────────────────────────────────────────────
** │ But         Vérifier que tous les caractères sont imprimables (ASCII
** │             32 à 126). Renvoie 1 si c'est vrai, 0 sinon.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"
#include "my_string.h"

int my_str_isprintable(char const *str)
{
    if (str == NULL)
        return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (!(str[i] >= 32 && str[i] <= 126))
            return 0;
    }
    return 1;
}
