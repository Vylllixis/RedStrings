/*
**
*/

/*
** ╭─ my_strncmp.c ───────────────────────────────────────────────────────
** │ But         Comme my_strcmp, mais en comparant au plus les n premiers
** │             caractères.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"
#include "my_string.h"

int my_strncmp(char const *s1, char const *s2, int n)
{
    int i = 0;

    if (n <= 0)
        return 0;
    if (s1 == NULL || s2 == NULL)
        return (s1 == s2) ? 0 : (s1 == NULL ? -1 : 1);
    while (s1[i] != '\0' && s2[i] != '\0' && i < n) {
        if (s1[i] != s2[i])
            return ((unsigned char)s1[i] - (unsigned char)s2[i]);
        i++;
    }
    if (i == n)
        return 0;
    return ((unsigned char)s1[i] - (unsigned char)s2[i]);
}
