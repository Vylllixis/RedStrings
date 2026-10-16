/*
**
*/

#include <stddef.h>
#include "my.h"

char *my_strstr(char *str, char const *to_find)
{
    if (*to_find == '\0')
        return str;

    for (int i = 0; str[i] != '\0'; i++) {
        int j = 0;
        while (str[i + j] == to_find[j] && to_find[j] != '\0') {
            j++;
        }
        if (to_find[j] == '\0')
            return &str[i];
    }
    return NULL;
}
