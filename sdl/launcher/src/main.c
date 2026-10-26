#include <stdio.h>
#include "engine/engine.h"

int main(void)
{
    printf("engine %s\n", eng_version());
    eng_print_deps();
    return 0;
}
