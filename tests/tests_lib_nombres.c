/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_lib_nombres.c ────────────────────────────────────────────────
** │ But         Un ou plusieurs tests par fonction de nombres et de
** │             tableaux de libmy : valeurs usuelles, limites des int,
** │             tableaux vides.
** │ Contenu     isneg, my_getnbr, puissance, racine, nombres premiers,
** │             échanges, tri, my_arraylen, my_arraystrlen,
** │             my_str_to_word_array, concat_params
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "lanceur.h"
#include "my.h"
#include "my_string.h"
#include "tests_lib.h"

static void test_isneg(void)
{
    V_CMP(isneg(-1), ==, 1);
    V_CMP(isneg(-2147483647 - 1), ==, 1);
    V_CMP(isneg(0), ==, 0);
    V_CMP(isneg(1), ==, 0);
    V_CMP(isneg(INT_MAX), ==, 0);
}

static void test_my_getnbr(void)
{
    V_CMP(my_getnbr("42"), ==, 42);
    V_CMP(my_getnbr("-42"), ==, -42);
    V_CMP(my_getnbr("0"), ==, 0);
    V_CMP(my_getnbr("-0"), ==, 0);
    V_CMP(my_getnbr("007"), ==, 7);
    V_CMP(my_getnbr("12abc"), ==, 12);  // s'arrête au premier non-chiffre
    V_CMP(my_getnbr("abc"), ==, 0);
    V_CMP(my_getnbr(""), ==, 0);
    V_CMP(my_getnbr("-"), ==, 0);
    V_CMP(my_getnbr("2147483647"), ==, INT_MAX);
    V_CMP(my_getnbr("-2147483648"), ==, INT_MIN);
    V_CMP(my_getnbr("2147483648"), ==, 0);  // dépassement : 0
    V_CMP(my_getnbr("-2147483649"), ==, 0);
    V_CMP(my_getnbr("99999999999999999999"), ==, 0);
}

static void test_my_compute_power_rec(void)
{
    V_CMP(my_compute_power_rec(2, 10), ==, 1024);
    V_CMP(my_compute_power_rec(5, 0), ==, 1);
    V_CMP(my_compute_power_rec(0, 0), ==, 1);  // convention
    V_CMP(my_compute_power_rec(0, 5), ==, 0);
    V_CMP(my_compute_power_rec(1, 1000000 / 1000), ==, 1);
    V_CMP(my_compute_power_rec(-2, 3), ==, -8);
    V_CMP(my_compute_power_rec(-2, 4), ==, 16);
    V_CMP(my_compute_power_rec(3, -1), ==, 0);  // exposant négatif : 0
    V_CMP(my_compute_power_rec(2, 31), ==, 0);  // dépasse int : 0
    V_CMP(my_compute_power_rec(2, 30), ==, 1073741824);
}

static void test_my_compute_square_root(void)
{
    V_CMP(my_compute_square_root(0), ==, 0);
    V_CMP(my_compute_square_root(1), ==, 1);
    V_CMP(my_compute_square_root(16), ==, 4);
    V_CMP(my_compute_square_root(144), ==, 12);
    V_CMP(my_compute_square_root(15), ==, 0);  // pas un carré parfait
    V_CMP(my_compute_square_root(-4), ==, 0);
    V_CMP(my_compute_square_root(2147395600), ==, 46340);  // le plus grand carré d'un int
    V_CMP(my_compute_square_root(INT_MAX), ==, 0);
}

static void test_my_is_prime(void)
{
    V_VRAI(my_is_prime(2));
    V_VRAI(my_is_prime(3));
    V_VRAI(my_is_prime(5));
    V_VRAI(my_is_prime(97));
    V_FAUX(my_is_prime(0));
    V_FAUX(my_is_prime(1));
    V_FAUX(my_is_prime(-7));
    V_FAUX(my_is_prime(4));
    V_FAUX(my_is_prime(9));
    V_FAUX(my_is_prime(100));
    V_VRAI(my_is_prime(7919));
    V_VRAI(my_is_prime(INT_MAX));  // 2^31 - 1 est premier, sans débordement
    V_FAUX(my_is_prime(INT_MAX - 1));
}

static void test_my_find_prime_sup(void)
{
    V_CMP(my_find_prime_sup(0), ==, 2);
    V_CMP(my_find_prime_sup(1), ==, 2);
    V_CMP(my_find_prime_sup(2), ==, 2);  // lui-même s'il est premier
    V_CMP(my_find_prime_sup(14), ==, 17);
    V_CMP(my_find_prime_sup(90), ==, 97);
    V_CMP(my_find_prime_sup(-10), ==, 2);
    V_CMP(my_find_prime_sup(INT_MAX), ==, INT_MAX);
    V_CMP(my_find_prime_sup(INT_MAX - 1), ==, INT_MAX);
}

static void test_my_swap(void)
{
    int a = 1;
    int b = 2;

    my_swap(&a, &b);
    V_CMP(a, ==, 2);
    V_CMP(b, ==, 1);
    my_swap(&a, &a);
    V_CMP(a, ==, 2);  // même adresse : sans effet
}

static void test_my_swap_params(void)
{
    char *a = "un";
    char *b = "deux";

    my_swap_params(&a, &b);
    V_TEXTE_EGAL(a, "deux");
    V_TEXTE_EGAL(b, "un");
}

static void test_my_sort_int_array(void)
{
    int t[6] = {5, -1, 3, 3, 0, 9};
    int u[1] = {7};
    int deja[3] = {1, 2, 3};

    my_sort_int_array(t, 6);
    V_CMP(t[0], ==, -1);
    V_CMP(t[1], ==, 0);
    V_CMP(t[2], ==, 3);
    V_CMP(t[3], ==, 3);
    V_CMP(t[4], ==, 5);
    V_CMP(t[5], ==, 9);
    my_sort_int_array(u, 1);
    V_CMP(u[0], ==, 7);
    my_sort_int_array(deja, 3);
    V_CMP(deja[0], ==, 1);
    V_CMP(deja[2], ==, 3);
    my_sort_int_array(t, 0);                            // taille 0 : rien
    my_sort_int_array(t, 3);                            // ne trie que les 3 premiers
    V_CMP(t[3], ==, 3);
    V_CMP(t[5], ==, 9);
}

static void test_my_arraylen(void)
{
    char *deux[3] = {"a", "b", NULL};
    char *aucun[1] = {NULL};

    V_CMP(my_arraylen(deux), ==, 2);
    V_CMP(my_arraylen(aucun), ==, 0);
}

static void test_my_arraystrlen(void)
{
    char *av[3] = {"ab", "", "cde"};

    V_CMP(my_arraystrlen(3, av), ==, 5);
    V_CMP(my_arraystrlen(1, av), ==, 2);
    V_CMP(my_arraystrlen(0, av), ==, 0);
}

static void test_my_str_to_word_array(void)
{
    char **m = my_str_to_word_array("  Hello, world 42!! ok ");

    V_CMP_BLOQUANT(m, !=, NULL);
    V_TEXTE_EGAL(m[0], "Hello");
    V_TEXTE_EGAL(m[1], "world");
    V_TEXTE_EGAL(m[2], "42");
    V_TEXTE_EGAL(m[3], "ok");
    V_CMP(m[4], ==, NULL);
    for (int i = 0; m[i] != NULL; i++)
        free(m[i]);
    free(m);
    m = my_str_to_word_array("!!! ???");                // aucun mot
    V_CMP_BLOQUANT(m, !=, NULL);
    V_CMP(m[0], ==, NULL);
    free(m);
    m = my_str_to_word_array("seul");
    V_CMP_BLOQUANT(m, !=, NULL);
    V_TEXTE_EGAL(m[0], "seul");
    V_CMP(m[1], ==, NULL);
    free(m[0]);
    free(m);
}

static void test_concat_params(void)
{
    char *av[3] = {"un", "deux", "trois"};
    char *c = concat_params(3, av);

    V_CMP_BLOQUANT(c, !=, NULL);
    V_TEXTE_EGAL(c, "un\ndeux\ntrois\n");
    free(c);
    c = concat_params(1, av);
    V_CMP_BLOQUANT(c, !=, NULL);
    V_TEXTE_EGAL(c, "un\n");
    free(c);
    c = concat_params(0, av);
    V_CMP_BLOQUANT(c, !=, NULL);
    V_CMP(c[0], ==, '\0');
    free(c);
}

void tests_lib_nombres(void)
{
    LANCER(test_isneg);
    LANCER(test_my_getnbr);
    LANCER(test_my_compute_power_rec);
    LANCER(test_my_compute_square_root);
    LANCER(test_my_is_prime);
    LANCER(test_my_find_prime_sup);
    LANCER(test_my_swap);
    LANCER(test_my_swap_params);
    LANCER(test_my_sort_int_array);
    LANCER(test_my_arraylen);
    LANCER(test_my_arraystrlen);
    LANCER(test_my_str_to_word_array);
    LANCER(test_concat_params);
}
