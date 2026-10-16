/*
**
*/

#include "my.h"

int my_sort_params(int ac, char **av)
{
    char **sort_av = av;

    for (int i = 0; i < ac - 1; i++) {
        for (int j = 0; j < ac - i - 1; j++) {
            if (my_strcmp(av[j], av[j + 1]) > 0) {
                my_swap_params(&av[j], &av[j + 1]);
            }
        }
    }
    my_print_params(ac, sort_av);
    return 0;
}
