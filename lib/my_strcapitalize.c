/*
**
*/

/*
** ╭─ my_strcapitalize.c ─────────────────────────────────────────────────
** │ But         Mettre une majuscule à la première lettre de chaque mot,
** │             et des minuscules ailleurs. Un mot commence après tout
** │             caractère qui n'est ni une lettre ni un chiffre.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdbool.h>
#include <stddef.h>
#include "my.h"
#include "my_string.h"

char *my_strcapitalize(char *str)
{
    bool new_word = true;

    if (str == NULL)
        return NULL;
    for (int i = 0; str[i] != '\0'; i++) {
        if ((str[i] >= 'a' && str[i] <= 'z') || (str[i] >= 'A' && str[i] <= 'Z')) {
            if (new_word) {
                if (str[i] >= 'a' && str[i] <= 'z') {
                    str[i] -= 32;
                }
                new_word = false;
            } else {
                if (str[i] >= 'A' && str[i] <= 'Z') {
                    str[i] += 32;
                }
            }
        } else if (str[i] >= '0' && str[i] <= '9') {
            new_word = false;
        } else {
            new_word = true;
        }
    }
    return str;
}
