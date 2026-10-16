/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "struct.h"
#include "couleurs.h"

#ifndef MY_H
    #define MY_H

int redstrings(void);
int creerPersonnage(personnage_t *p);
int afficherPersonnage(const personnage_t *p);
void attaquer(personnage_t *attaquant, personnage_t *cible);
int my_strlen(char *str);
int my_strcmp(char *str1, char *str2);

#endif /* MY_H */
