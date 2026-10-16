/*
**
*/

#include "my.h"

int my_getnbr(char const *str)
{
    int i = 0;
    int sign = 1;
    int result = 0;

    if (str[i] == '-') {
        sign = -1;
        i++;
    }
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }
    return result * sign;
}
