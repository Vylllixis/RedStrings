/*
**
*/

#include "my.h"

static int is_alnum(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z');
}

char **my_str_to_word_array(char const *str)
{
    char **array = calloc(my_strlen(str) / 2 + 2, sizeof(char *));
    int n = 0;
    int len = 0;

    for (; *str; str += len) {
        for (; *str && !is_alnum(*str); str++);
        for (len = 0; is_alnum(str[len]); len++);
        if (len)
            array[n++] = my_strncpy(calloc(len + 1, 1), str, len);
    }
    return array;
}
