/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

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
