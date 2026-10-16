/*
**
*/

#include "my.h"

void my_swap_params(char **a, char **b)
{
    char *temp = *a;
    *a = *b;
    *b = temp;
}
