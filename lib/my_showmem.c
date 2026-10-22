/*
**
*/

/*
** ╭─ my_showmem.c ───────────────────────────────────────────────────────
** │ But         Écrire une zone mémoire de size caractères, 16 par ligne.
** │ Utilise     my_putchar
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"

int my_showmem(char const *str, int size)
{
    for (int i = 0; i < size; i++) {
        if (i % 16 == 0 && i != 0) {
            my_putchar('\n');
        }
        my_putchar(str[i]);
    }
    my_putchar('\n');
    return 0;
}
