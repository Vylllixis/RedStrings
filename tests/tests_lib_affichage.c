/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_lib_affichage.c ──────────────────────────────────────────────
** │ But         Les fonctions de libmy qui écrivent sur la sortie
** │             standard : on capture ce qu'elles écrivent et on le
** │             compare au texte attendu.
** │ Contenu     my_putchar, my_putstr, my_put_nbr, my_showstr,
** │             my_showmem, my_show_word_array, my_print_params,
** │             my_rev_params, my_sort_params
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <limits.h>
#include <string.h>
#include "lanceur.h"
#include "my.h"
#include "outils_tests.h"
#include "tests_lib.h"

static void test_my_putchar(void)
{
    char s[8];

    capture_debut();
    my_putchar('A');
    my_putchar('\n');
    my_putchar('z');
    capture_fin(s, sizeof(s));
    V_TEXTE_EGAL(s, "A\nz");
}

static void test_my_putstr(void)
{
    char s[64];

    capture_debut();
    V_CMP(my_putstr("Bonjour"), ==, 0);
    V_CMP(my_putstr(""), ==, 0);
    V_CMP(my_putstr(" é!"), ==, 0);
    capture_fin(s, sizeof(s));
    V_TEXTE_EGAL(s, "Bonjour é!");
}

static void test_my_put_nbr(void)
{
    char s[64];

    capture_debut();
    V_CMP(my_put_nbr(0), ==, 0);
    my_putchar(' ');
    my_put_nbr(42);
    my_putchar(' ');
    my_put_nbr(-7);
    my_putchar(' ');
    my_put_nbr(INT_MAX);
    my_putchar(' ');
    my_put_nbr(INT_MIN);
    capture_fin(s, sizeof(s));
    V_TEXTE_EGAL(s, "0 42 -7 2147483647 -2147483648");
}

static void test_my_showstr(void)
{
    char s[64];

    capture_debut();
    V_CMP(my_showstr("Hello\nWorld\t!"), ==, 0);
    capture_fin(s, sizeof(s));
    V_TEXTE_EGAL(s, "Hello\\10World\\9!");  // '\n' = 10, '\t' = 9
    capture_debut();
    my_showstr("a\x7F" "b");
    capture_fin(s, sizeof(s));
    V_TEXTE_EGAL(s, "a\\127b");
}

static void test_my_showmem(void)
{
    char s[128];

    capture_debut();
    V_CMP(my_showmem("0123456789abcdefXYZ", 19), ==, 0);
    capture_fin(s, sizeof(s));
    V_TEXTE_EGAL(s, "0123456789abcdef\nXYZ\n");  // 16 par ligne
    capture_debut();
    my_showmem("abc", 3);
    capture_fin(s, sizeof(s));
    V_TEXTE_EGAL(s, "abc\n");
}

static void test_my_show_word_array(void)
{
    char *t[4] = {"un", "deux", "trois", NULL};
    char *seul[2] = {"seul", NULL};
    char s[64];

    capture_debut();
    V_CMP(my_show_word_array(t), ==, 0);
    capture_fin(s, sizeof(s));
    V_TEXTE_EGAL(s, "un deux trois.");
    capture_debut();
    my_show_word_array(seul);
    capture_fin(s, sizeof(s));
    V_TEXTE_EGAL(s, "seul.");
}

static void test_my_print_params(void)
{
    char *av[3] = {"a", "bb", "ccc"};
    char s[64];

    capture_debut();
    V_CMP(my_print_params(3, av), ==, 0);
    capture_fin(s, sizeof(s));
    V_TEXTE_EGAL(s, "a\nbb\nccc\n");
    capture_debut();
    my_print_params(0, av);
    capture_fin(s, sizeof(s));
    V_CMP(s[0], ==, '\0');
}

static void test_my_rev_params(void)
{
    char *av[3] = {"a", "bb", "ccc"};
    char s[64];

    capture_debut();
    V_CMP(my_rev_params(3, av), ==, 0);
    capture_fin(s, sizeof(s));
    V_TEXTE_EGAL(s, "ccc\nbb\na\n");
}

static void test_my_sort_params(void)
{
    char *av[4] = {"pomme", "Banane", "cerise", "abricot"};
    char s[64];

    capture_debut();
    V_CMP(my_sort_params(4, av), ==, 0);
    capture_fin(s, sizeof(s));
    V_TEXTE_EGAL(s, "Banane\nabricot\ncerise\npomme\n");  // ordre ASCII
    V_TEXTE_EGAL(av[0], "Banane");  // tri sur place
    V_TEXTE_EGAL(av[3], "pomme");  // tri sur place
}

void tests_lib_affichage(void)
{
    LANCER(test_my_putchar);
    LANCER(test_my_putstr);
    LANCER(test_my_put_nbr);
    LANCER(test_my_showstr);
    LANCER(test_my_showmem);
    LANCER(test_my_show_word_array);
    LANCER(test_my_print_params);
    LANCER(test_my_rev_params);
    LANCER(test_my_sort_params);
}
