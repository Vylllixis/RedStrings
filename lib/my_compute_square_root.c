/*
**
*/

#include "my.h"

int my_compute_square_root(int nb)
{
    if (nb < 0)
        return 0;
    for (int i = 0; i <= nb / 2; i++) {
        if (i * i == nb)
            return i;
    }
    return 0;
}
