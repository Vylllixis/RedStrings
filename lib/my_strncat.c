/*
**
*/

/*
** ╭─ my_strncat.c ───────────────────────────────────────────────────────
** │ But         Comme my_strcat, mais en collant au plus n caractères de
** │             src. Le '\0' final est toujours ajouté.
** │ Utilise     my_strlen
** │ Attention   dest doit avoir la place pour les caractères ajoutés
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"
#include "my_string.h"

char *my_strncat(char *dest, char const *src, int n)
{
    int dest_len = my_strlen(dest);
    int i = 0;

    while (src[i] != '\0' && i < n) {
        dest[dest_len + i] = src[i];
        i++;
    }
    dest[dest_len + i] = '\0';
    return dest;
}
