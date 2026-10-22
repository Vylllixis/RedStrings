/*
**
*/

/*
** ╭─ my_swap_params.c ───────────────────────────────────────────────────
** │ But         Échanger deux pointeurs de chaîne (et non le texte
** │             lui-même).
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"

void my_swap_params(char **a, char **b)
{
    char *temp = *a;
    *a = *b;
    *b = temp;
}
