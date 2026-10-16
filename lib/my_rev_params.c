/*
**
*/

#include "my.h"

int my_rev_params(int ac, char **av)
{
    for (int i = ac - 1; i >= 0; i--) {
        my_putstr(av[i]);
        my_putchar('\n');
    }
    return 0;
}
