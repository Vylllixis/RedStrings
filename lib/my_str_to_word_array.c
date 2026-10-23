/*
**
*/

/*
** ╭─ my_str_to_word_array.c ─────────────────────────────────────────────
** │ But         Découper une chaîne en mots (suites de lettres et de
** │             chiffres). Le tableau renvoyé est terminé par NULL.
** │ Utilise     my_strlen, my_strncpy
** │ Attention   le tableau et chaque mot sont alloués : à libérer
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdlib.h>
#include "my.h"
#include "my_string.h"

static int is_alnum(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z');
}

char **my_str_to_word_array(char const *str)
{
    char **array = NULL;
    int n = 0;
    int len = 0;

    if (str == NULL)
        return NULL;
    array = calloc((size_t)my_strlen(str) / 2 + 2, sizeof(char *));
    if (array == NULL)
        return NULL;
    for (; *str; str += len) {
        for (; *str && !is_alnum(*str); str++);
        for (len = 0; is_alnum(str[len]); len++);
        if (len == 0)
            continue;
        array[n] = calloc((size_t)len + 1, 1);
        if (array[n] == NULL) {
            for (int i = 0; i < n; i++)
                free(array[i]);
            free(array);
            return NULL;
        }
        my_strncpy(array[n++], str, len);
    }
    return array;
}
