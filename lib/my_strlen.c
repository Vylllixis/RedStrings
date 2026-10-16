/*
**
*/

#include "my.h"

int my_strlen(const char *str)
{
    int length = 0;

    for (; str[length] != '\0'; length++);
    return length;
}
