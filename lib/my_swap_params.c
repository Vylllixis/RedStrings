/*
**
*/

/*
** ╭─ my_swap_params.c ───────────────────────────────────────────────────
** │ But         Échanger deux pointeurs de chaîne (et non le texte
** │             lui-même).
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"

void my_swap_params(char **a, char **b)
{
    char *temp = NULL;

    if (a == NULL || b == NULL)
        return;
    temp = *a;
    *a = *b;
    *b = temp;
}
