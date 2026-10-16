/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef MY_H
    #define MY_H

    #include <unistd.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include "boite.h"
    #include "couleurs.h"
    #include "personnage.h"

int redstrings(void);
int my_strlen(char *str);
int my_strcmp(char *str1, char *str2);
void viderBuffer(void);

#endif /* MY_H */
