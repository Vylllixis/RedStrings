/*
**
*/

/*
** ╭─ my_strdup.c ────────────────────────────────────────────────────────
** │ But         Copier une chaîne dans une zone allouée avec malloc.
** │             Renvoie NULL si l'allocation échoue.
** │ Utilise     my_strlen
** │ Attention   le résultat est à libérer avec free()
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include <stdlib.h>
#include "my.h"
#include "my_string.h"

char *my_strdup(char const *src)
{
    int i = 0;
    int len = 0;
    char *cpy = NULL;

    if (src == NULL)
        return NULL;
    len = my_strlen(src) + 1;
    cpy = (char *)malloc((size_t)len);
    if (cpy == NULL)
        return NULL;
    for (; i < len - 1; i++) {
        cpy[i] = src[i];
    }
    cpy[i] = '\0';
    return cpy;
}
