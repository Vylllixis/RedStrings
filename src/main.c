/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ main.c ─────────────────────────────────────────────────────────────
** │ But         Point d'entrée du programme : lance le menu du jeu.
** │ Utilise     redstrings() de redstrings.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdlib.h>
#include <time.h>
#include "redstrings.h"

int main(void)
{
    srand((unsigned)time(NULL));        /* le hasard de l'ordinateur en combat */
    return redstrings();
}
