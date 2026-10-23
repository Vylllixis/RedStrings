/*
**
*/

/*
** ╭─ my_strcmp.c ────────────────────────────────────────────────────────
** │ But         Comparer deux chaînes. Renvoie 0 si elles sont égales,
** │             sinon la différence entre les deux premiers caractères
** │             qui diffèrent (négatif si s1 est avant s2).
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"
#include "my_string.h"

int my_strcmp(char const *s1, char const *s2)
{
    int i = 0;

    if (s1 == NULL || s2 == NULL)
        return (s1 == s2) ? 0 : (s1 == NULL ? -1 : 1);
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i])
            return ((unsigned char)s1[i] - (unsigned char)s2[i]);
        i++;
    }
    return ((unsigned char)s1[i] - (unsigned char)s2[i]);
}
