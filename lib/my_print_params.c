/*
**
*/

/*
** ╭─ my_print_params.c ──────────────────────────────────────────────────
** │ But         Écrire chaque argument du programme sur sa ligne.
** │ Utilise     my_putstr, my_putchar
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"

int my_print_params(int ac, char **av)
{
    for (int i = 0; i < ac; i++) {
        my_putstr(av[i]);
        my_putchar('\n');
    }
    return 0;
}
