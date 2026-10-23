/*
**
*/

/*
** ╭─ my_swap.c ──────────────────────────────────────────────────────────
** │ But         Échanger deux entiers, grâce à leurs adresses.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"

void my_swap(int *a, int *b)
{
    int temp = 0;

    if (a == NULL || b == NULL)
        return;
    temp = *a;
    *a = *b;
    *b = temp;
}
