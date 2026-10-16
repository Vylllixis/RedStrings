/*
**
*/

#include "my.h"

int my_find_prime_sup(int nb)
{
    if (nb <= 1)
        return 2;
    while (!my_is_prime(nb)) {
        nb++;
    }
    return nb;
}
