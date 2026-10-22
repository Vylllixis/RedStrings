/*
**
*/

/*
** ╭─ my_strcpy.c ────────────────────────────────────────────────────────
** │ But         Copier une chaîne, '\0' compris, dans dest.
** │ Attention   dest doit être assez grand
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"
#include "my_string.h"

char *my_strcpy(char *dest, char const *src)
{
    int i = 0;

    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
    return dest;
}
