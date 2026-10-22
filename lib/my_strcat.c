/*
**
*/

/*
** ╭─ my_strcat.c ────────────────────────────────────────────────────────
** │ But         Coller src à la fin de dest.
** │ Utilise     my_strlen
** │ Attention   dest doit avoir la place pour les deux chaînes
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"
#include "my_string.h"

char *my_strcat(char *dest, char const *src)
{
    int dest_len = my_strlen(dest);
    int i = 0;

    while (src[i] != '\0') {
        dest[dest_len + i] = src[i];
        i++;
    }
    dest[dest_len + i] = '\0';
    return dest;
}
