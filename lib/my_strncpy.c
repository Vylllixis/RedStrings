/*
**
*/

/*
** ╭─ my_strncpy.c ───────────────────────────────────────────────────────
** │ But         Copier au plus n caractères de src dans dest ; si src est
** │             plus courte, la fin est remplie de '\0'.
** │ Attention   si src fait n caractères ou plus, dest n'a pas de '\0'
** │             final
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"
#include "my_string.h"

char *my_strncpy(char *dest, char const *src, int n)
{
    int i = 0;

    while (src[i] != '\0' && i < n) {
        dest[i] = src[i];
        i++;
    }
    while (i < n) {
        dest[i] = '\0';
        i++;
    }
    return dest;
}
