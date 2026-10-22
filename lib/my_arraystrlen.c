/*
**
*/

/*
** ╭─ my_arraystrlen.c ───────────────────────────────────────────────────
** │ But         Additionner les longueurs des ac chaînes de av.
** │ Utilise     my_strlen
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"
#include "my_string.h"

int my_arraystrlen(int ac, char **av)
{
    int length = 0;

    for (int i = 0; i < ac; i++) {
        for (int j = 0; j < my_strlen(av[i]); j++) {
            length++;
        }
    }
    return length;
}
