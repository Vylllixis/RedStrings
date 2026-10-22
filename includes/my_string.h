/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/**
 * @file my_string.h
 * @brief Chaînes de caractères de la bibliothèque maison `libmy.a`.
 *
 * Réécriture des fonctions classiques de `<string.h>`. Le jeu les utilise à
 * la place de celles de la libc. Les tests de classe (`my_str_is...`)
 * renvoient 1 si vrai, 0 sinon, et 1 pour une chaîne vide.
 */

#ifndef MY_STRING_H
    #define MY_STRING_H

/** @name Longueur, comparaison et recherche */
/**@{*/
/** @brief Nombre de caractères avant le caractère nul final. */
int my_strlen(const char *str);
/** @brief Compare deux chaînes ; renvoie 0 si elles sont égales, sinon la différence des premiers octets distincts. */
int my_strcmp(char const *s1, char const *s2);
/** @brief Comme my_strcmp(), sur les `n` premiers caractères seulement. */
int my_strncmp(char const *s1, char const *s2, int n);
/** @brief Première occurrence de `to_find` dans `str`, ou NULL (renvoie `str` si `to_find` est vide). */
char *my_strstr(char *str, char const *to_find);
/**@}*/

/** @name Copie et concaténation */
/**@{*/
/** @brief Copie `src` dans `dest`, caractère nul final compris. `dest` doit être assez grand. */
char *my_strcpy(char *dest, char const *src);
/** @brief Copie au plus `n` caractères de `src` et complète par des caractères nuls jusqu'à `n`. */
char *my_strncpy(char *dest, char const *src, int n);
/** @brief Ajoute `src` à la fin de `dest`. `dest` doit être assez grand. */
char *my_strcat(char *dest, char const *src);
/** @brief Ajoute au plus `n` caractères de `src` à la fin de `dest`, puis un caractère nul. */
char *my_strncat(char *dest, char const *src, int n);
/** @brief Copie `src` dans une zone allouée avec `malloc` (à libérer avec `free`), ou NULL si l'allocation échoue. */
char *my_strdup(char const *src);
/**@}*/

/** @name Transformation (modifient la chaîne en place) */
/**@{*/
/** @brief Inverse l'ordre des caractères. */
char *my_revstr(char *str);
/** @brief Passe les lettres en majuscules (ASCII). */
char *my_strupcase(char *str);
/** @brief Passe les lettres en minuscules (ASCII). */
char *my_strlowcase(char *str);
/** @brief Première lettre de chaque mot en majuscule, les autres en minuscules. */
char *my_strcapitalize(char *str);
/**@}*/

/** @name Tests (1 si vrai, 0 sinon) */
/**@{*/
/** @brief Uniquement des lettres ASCII. */
int my_str_isalpha(char const *str);
/** @brief Uniquement des minuscules ASCII. */
int my_str_islower(char const *str);
/** @brief Uniquement des majuscules ASCII. */
int my_str_isupper(char const *str);
/** @brief Uniquement des chiffres. */
int my_str_isnum(char const *str);
/** @brief Uniquement des caractères imprimables (codes 32 à 126). */
int my_str_isprintable(char const *str);
/**@}*/

#endif /* MY_STRING_H */
