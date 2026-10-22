/*
**
*/

/*
** ╭─ my_isneg.c ─────────────────────────────────────────────────────────
** │ But         Dire si un nombre est négatif : 1 si oui, 0 sinon.
** │ Attention   la fonction s'appelle isneg
** ╰──────────────────────────────────────────────────────────────────────
*/

#include "my.h"

int isneg(int n)
{
    if (n < 0)
        return 1;
    else
        return 0;
}
