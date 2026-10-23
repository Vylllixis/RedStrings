/*
**
*/

/*
** ╭─ my_arraylen.c ──────────────────────────────────────────────────────
** │ But         Compter les éléments d'un tableau de chaînes terminé par
** │             NULL.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"

int my_arraylen(char * const *av)
{
    int i = 0;

    if (av == NULL)
        return 0;
    for (; av[i] != NULL; i++);
    return i;
}
