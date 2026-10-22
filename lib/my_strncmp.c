/*
**
*/

/*
** ╭─ my_strncmp.c ───────────────────────────────────────────────────────
** │ But         Comme my_strcmp, mais en comparant au plus les n premiers
** │             caractères.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"
#include "my_string.h"

int my_strncmp(char const *s1, char const *s2, int n)
{
    int i = 0;

    while (s1[i] != '\0' && s2[i] != '\0' && i < n) {
        if (s1[i] != s2[i])
            return (s1[i] - s2[i]);
        i++;
    }
    if (i == n)
        return 0;
    return (s1[i] - s2[i]);
}
