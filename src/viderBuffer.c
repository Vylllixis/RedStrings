/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "saisie.h"

void viderBuffer(void)
{
    int c = getchar();

    while (c != '\n' && c != EOF)
        c = getchar();
}

