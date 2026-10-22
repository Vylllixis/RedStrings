/*
**
*/

/*
** ╭─ my_putchar.c ───────────────────────────────────────────────────────
** │ But         Écrire un caractère sur la sortie standard, avec write().
** │             Toutes les autres fonctions d'affichage de libmy passent
** │             par elle.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <unistd.h>
#include "my.h"

void my_putchar(char c)
{
    write(1, &c, 1);
}
