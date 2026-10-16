/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef MY_H
    #define MY_H

    #include <unistd.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include "personnage.h"
    #include "couleurs.h"

int redstrings(void);
int creerPersonnage(personnage_t *p);
int afficherPersonnage(const personnage_t *p);
void attaquer(personnage_t *attaquant, personnage_t *cible);
int my_strlen(char *str);
int my_strcmp(char *str1, char *str2);
void viderBuffer(void);
void boiteHaut(const char *couleur);
void boiteSeparateur(const char *couleur);
void boiteBas(const char *couleur);
void boiteLigne(const char *couleur, const char *format, ...);

#endif /* MY_H */
