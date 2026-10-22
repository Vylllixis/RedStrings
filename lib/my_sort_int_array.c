/*
**
*/

/*
** ╭─ my_sort_int_array.c ────────────────────────────────────────────────
** │ But         Trier un tableau d'entiers par ordre croissant (tri à
** │             bulles).
** │ Utilise     my_swap
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"

void my_sort_int_array(int *tab, int size)
{
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - i - 1; j++) {
            if (tab[j] > tab[j + 1]) {
                my_swap(&tab[j], &tab[j + 1]);
            }
        }
    }
}
