/*
**
*/

/*
** ╭─ my_strstr.c ────────────────────────────────────────────────────────
** │ But         Chercher la première occurrence d'une sous-chaîne.
** │             Renvoie un pointeur à cet endroit, ou NULL si elle n'est
** │             pas trouvée (une sous-chaîne vide est trouvée au début).
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"
#include "my_string.h"

char *my_strstr(char *str, char const *to_find)
{
    if (*to_find == '\0')
        return str;

    for (int i = 0; str[i] != '\0'; i++) {
        int j = 0;
        while (str[i + j] == to_find[j] && to_find[j] != '\0') {
            j++;
        }
        if (to_find[j] == '\0')
            return &str[i];
    }
    return NULL;
}
