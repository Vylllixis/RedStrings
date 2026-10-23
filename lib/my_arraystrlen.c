/*
**
*/

/*
** ╭─ my_arraystrlen.c ───────────────────────────────────────────────────
** │ But         Additionner les longueurs des ac chaînes de av.
** │ Utilise     my_strlen
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"
#include "my_string.h"

int my_arraystrlen(int ac, char **av)
{
    int length = 0;

    if (av == NULL || ac <= 0)
        return 0;
    for (int i = 0; i < ac; i++)
        length += my_strlen(av[i]);
    return length;
}
