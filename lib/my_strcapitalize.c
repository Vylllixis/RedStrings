/*
**
*/

#include <stdbool.h>
#include "my.h"
#include "my_string.h"

char *my_strcapitalize(char *str)
{
    bool new_word = true;

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
