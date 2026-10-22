/*
**
*/

/*
** ╭─ my_strlen.c ────────────────────────────────────────────────────────
** │ But         Compter les caractères d'une chaîne, sans le '\0' final.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"
#include "my_string.h"

int my_strlen(const char *str)
{
    int length = 0;

    for (; str[length] != '\0'; length++);
    return length;
}
