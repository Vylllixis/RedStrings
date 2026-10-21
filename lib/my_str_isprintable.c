/*
**
*/

#include "my.h"
#include "my_string.h"

int my_str_isprintable(char const *str)
{
    for (int i = 0; str[i] != '\0'; i++) {
        if (!(str[i] >= 32 && str[i] <= 126))
            return 0;
    }
    return 1;
}
