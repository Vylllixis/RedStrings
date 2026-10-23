/*
**
*/

/*
** ╭─ my_print_params.c ──────────────────────────────────────────────────
** │ But         Écrire chaque argument du programme sur sa ligne.
** │ Utilise     my_putstr, my_putchar
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"

int my_print_params(int ac, char **av)
{
    if (av == NULL || ac <= 0)
        return 0;
    for (int i = 0; i < ac; i++) {
        my_putstr(av[i]);
        my_putchar('\n');
    }
    return 0;
}
