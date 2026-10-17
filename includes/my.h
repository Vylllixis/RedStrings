/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "boite.h"
#include "couleurs.h"
#include "personnage.h"
#include "redstrings.h"
#include "saisie.h"

#ifndef MY_H
    #define MY_H

/* Affichage */
void my_putchar(char c);
int my_putstr(char const *str);
int my_put_nbr(int nb);
int my_showstr(char const *str);
int my_showmem(char const *str, int size);

/* Nombres */
int isneg(int n);
int my_getnbr(char const *str);
int my_compute_power_rec(int nb, int power);
int my_compute_square_root(int nb);
int my_is_prime(int nb);
int my_find_prime_sup(int nb);
void my_swap(int *a, int *b);
void my_sort_int_array(int *tab, int size);

/* Chaînes : longueur, comparaison, copie */
int my_strlen(const char *str);
int my_strcmp(char const *s1, char const *s2);
int my_strncmp(char const *s1, char const *s2, int n);
char *my_strcpy(char *dest, char const *src);
char *my_strncpy(char *dest, char const *src, int n);
char *my_strcat(char *dest, char const *src);
char *my_strncat(char *dest, char const *src, int n);
char *my_strdup(char const *src);
char *my_strstr(char *str, char const *to_find);

/* Chaînes : transformation */
char *my_revstr(char *str);
char *my_strupcase(char *str);
char *my_strlowcase(char *str);
char *my_strcapitalize(char *str);

/* Chaînes : tests (renvoient 1 si vrai, 0 sinon) */
int my_str_isalpha(char const *str);
int my_str_islower(char const *str);
int my_str_isupper(char const *str);
int my_str_isnum(char const *str);
int my_str_isprintable(char const *str);

/* Tableaux de chaînes et paramètres du programme */
int my_arraylen(char * const *av);
int my_arraystrlen(int ac, char **av);
char **my_str_to_word_array(char const *str);
int my_show_word_array(char * const *tab);
void my_swap_params(char **a, char **b);
int my_print_params(int ac, char **av);
int my_rev_params(int ac, char **av);
int my_sort_params(int ac, char **av);
char *concat_params(int ac, char **av);

#endif /* MY_H */
