/*
**
*/

/*
** ╭─ concat_params.c ────────────────────────────────────────────────────
** │ But         Coller tous les arguments du programme dans une seule
** │             chaîne allouée, un argument par ligne.
** │ Utilise     my_arraystrlen, my_strlen
** │ Attention   le résultat est à libérer avec free()
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdlib.h>
#include "my.h"
#include "my_string.h"

char *concat_params(int ac, char **av)
{
    int len = 0;
    char *catparams = NULL;
    int x = 0;

    if (av == NULL || ac < 0)
        return NULL;
    len = my_arraystrlen(ac, av) + ac + 1;
    catparams = (char *)malloc((size_t)len);
    if (catparams == NULL)
        return NULL;
    for (int i = 0; i < ac; i++) {
        for (int j = 0; j < my_strlen(av[i]); j++) {
            catparams[x] = av[i][j];
            x++;
        }
        catparams[x] = '\n';
        x++;
    }
    catparams[x] = '\0';
    return catparams;
}
