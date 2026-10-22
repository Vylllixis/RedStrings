/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/**
 * @file my.h
 * @brief Affichage, nombres et tableaux de chaînes de la bibliothèque `libmy.a`.
 *
 * Les fonctions d'affichage écrivent sur la sortie standard avec `write`.
 */

#ifndef MY_H
    #define MY_H

/** @name Affichage */
/**@{*/
/** @brief Écrit un caractère. */
void my_putchar(char c);
/** @brief Écrit une chaîne. Renvoie 0. */
int my_putstr(char const *str);
/** @brief Écrit un entier, avec son signe. Renvoie 0. */
int my_put_nbr(int nb);
/** @brief Écrit une chaîne ; un caractère non imprimable est écrit `\` suivi de son code décimal. Renvoie 0. */
int my_showstr(char const *str);
/** @brief Écrit `size` caractères, 16 par ligne. Renvoie 0. */
int my_showmem(char const *str, int size);
/**@}*/

/** @name Nombres */
/**@{*/
/** @brief 1 si `n` est négatif, sinon 0. */
int isneg(int n);
/** @brief Lit un entier au début de `str` (signe `-` accepté) ; s'arrête au premier caractère qui n'est pas un chiffre. */
int my_getnbr(char const *str);
/** @brief `nb` puissance `power`, par récursion ; 0 si `power` est négatif. */
int my_compute_power_rec(int nb, int power);
/** @brief Racine carrée entière exacte de `nb` ; 0 si `nb` n'est pas un carré parfait. */
int my_compute_square_root(int nb);
/** @brief 1 si `nb` est premier, sinon 0. */
int my_is_prime(int nb);
/** @brief Plus petit nombre premier supérieur ou égal à `nb`. */
int my_find_prime_sup(int nb);
/** @brief Échange les valeurs pointées par `a` et `b`. */
void my_swap(int *a, int *b);
/** @brief Trie `tab` (`size` éléments) par ordre croissant. */
void my_sort_int_array(int *tab, int size);
/**@}*/

/** @name Tableaux de chaînes et paramètres du programme */
/**@{*/
/** @brief Nombre d'éléments d'un tableau de chaînes terminé par NULL. */
int my_arraylen(char * const *av);
/** @brief Somme des longueurs des `ac` chaînes de `av`. */
int my_arraystrlen(int ac, char **av);
/** @brief Découpe `str` en mots (suites de lettres et de chiffres) ; tableau terminé par NULL, à libérer. */
char **my_str_to_word_array(char const *str);
/** @brief Écrit les mots d'un tableau terminé par NULL, séparés par un espace, avec un point à la fin. Renvoie 0. */
int my_show_word_array(char * const *tab);
/** @brief Échange deux pointeurs de chaîne. */
void my_swap_params(char **a, char **b);
/** @brief Écrit chaque paramètre sur sa ligne. Renvoie 0. */
int my_print_params(int ac, char **av);
/** @brief Écrit les paramètres sur une ligne chacun, du dernier au premier. Renvoie 0. */
int my_rev_params(int ac, char **av);
/** @brief Trie `av` par ordre alphabétique (en place) puis l'écrit. Renvoie 0. */
int my_sort_params(int ac, char **av);
/** @brief Concatène les paramètres, un par ligne, dans une zone allouée (à libérer). */
char *concat_params(int ac, char **av);
/**@}*/

#endif /* MY_H */
