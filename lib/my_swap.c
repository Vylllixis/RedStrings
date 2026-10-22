/*
**
*/

/*
** ╭─ my_swap.c ──────────────────────────────────────────────────────────
** │ But         Échanger deux entiers, grâce à leurs adresses.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"

void my_swap(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}
