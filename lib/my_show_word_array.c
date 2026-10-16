/*
**
*/

#include <stddef.h>
#include "my.h"

int my_show_word_array(char * const *tab)
{
    for (int i = 0; tab[i] != NULL; i++) {
        my_putstr(tab[i]);
        if (tab[i + 1] == NULL)
            my_putchar('.');
        else
            my_putchar(' ');
    }
    return 0;
}
