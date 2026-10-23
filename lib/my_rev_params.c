/*
**
*/

/*
** ╭─ my_rev_params.c ────────────────────────────────────────────────────
** │ But         Comme my_print_params, mais du dernier argument au
** │             premier.
** │ Utilise     my_putstr, my_putchar
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"

int my_rev_params(int ac, char **av)
{
    if (av == NULL || ac <= 0)
        return 0;
    for (int i = ac - 1; i >= 0; i--) {
        my_putstr(av[i]);
        my_putchar('\n');
    }
    return 0;
}
