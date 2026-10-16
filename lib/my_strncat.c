/*
**
*/

#include "my.h"

char *my_strncat(char *dest, char const *src, int n)
{
    int dest_len = my_strlen(dest);
    int i = 0;

    while (src[i] != '\0' && i < n) {
        dest[dest_len + i] = src[i];
        i++;
    }
    dest[dest_len + i] = '\0';
    return dest;
}
