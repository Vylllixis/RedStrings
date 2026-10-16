/*
**
*/

#include <stddef.h>
#include "my.h"

int my_arraylen(char * const *av)
{
    int i = 0;

    for (; av[i] != NULL; i++);
    return i;
}
