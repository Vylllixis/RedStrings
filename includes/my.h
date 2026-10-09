/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "struct.h"

#ifndef MY_H
    #define MY_H

int redstrings(void);
int creerPersonnage(personnage_t *p);
int afficherPersonnage(const personnage_t *p);
void attaquer(personnage_t *attaquant, personnage_t *cible);

#endif /* MY_H */
