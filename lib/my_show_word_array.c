/*
**
*/

/*
** ╭─ my_show_word_array.c ───────────────────────────────────────────────
** │ But         Écrire les mots d'un tableau terminé par NULL, séparés
** │             par un espace, avec un point à la fin.
** │ Utilise     my_putstr, my_putchar
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stddef.h>
#include "my.h"

int my_show_word_array(char * const *tab)
{
    if (tab == NULL)
        return 0;
    for (int i = 0; tab[i] != NULL; i++) {
        my_putstr(tab[i]);
        if (tab[i + 1] == NULL)
            my_putchar('.');
        else
            my_putchar(' ');
    }
    return 0;
}
