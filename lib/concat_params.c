/*
**
*/

#include <stdlib.h>
#include "my.h"
#include "my_string.h"

char *concat_params(int ac, char **av)
{
    int len = my_arraystrlen(ac, av) + ac + 1;
    char *catparams = (char *)malloc(sizeof(char) * len);
    int x = 0;

    for (int i = 0; i < ac; i++) {
        for (int j = 0; j < my_strlen(av[i]); j++) {
            catparams[x] = av[i][j];
            x++;
        }
        catparams[x] = '\n';
        x++;
    }
    return catparams;
}
