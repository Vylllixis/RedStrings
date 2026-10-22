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

#include "my.h"
#include "my_string.h"

int my_strcmp(char const *s1, char const *s2)
{
    int i = 0;

    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i])
            return (s1[i] - s2[i]);
        i++;
    }
    return (s1[i] - s2[i]);
}
