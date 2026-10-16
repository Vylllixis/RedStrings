/*
**
*/

#include <stdlib.h>
#include "my.h"

char *my_strdup(char const *src)
{
    int i = 0;
    int len = my_strlen(src) + 1;
    char *cpy = (char *)malloc(sizeof(char) * (len));

    if (cpy == NULL)
        return NULL;
    for (; i < len - 1; i++) {
        cpy[i] = src[i];
    }
    cpy[i] = '\0';
    return cpy;
}
