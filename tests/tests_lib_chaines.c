/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_lib_chaines.c ────────────────────────────────────────────────
** │ But         Un ou plusieurs tests par fonction de chaînes de libmy :
** │             cas normaux, chaîne vide, limites, valeurs de retour.
** │ Contenu     longueur, comparaison, recherche, copie, concaténation,
** │             duplication, transformation, tests de contenu
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdlib.h>
#include <string.h>
#include "lanceur.h"
#include "my_string.h"
#include "tests_lib.h"

static void test_my_strlen(void)
{
    V_CMP(my_strlen(""), ==, 0);
    V_CMP(my_strlen("a"), ==, 1);
    V_CMP(my_strlen("bonjour"), ==, 7);
    V_CMP(my_strlen("ab\0cd"), ==, 2);  // s'arrête au premier '\0'
    V_CMP(my_strlen("é"), ==, 2);  // compte des octets, pas des lettres
}

static void test_my_strcmp_cas(void)
{
    V_TEXTE_EGAL("abc", "abc");
    V_TEXTE_EGAL("", "");
    V_CMP(my_strcmp("abc", "abd"), <, 0);
    V_CMP(my_strcmp("abd", "abc"), >, 0);
    V_CMP(my_strcmp("ab", "abc"), <, 0);  // préfixe : le plus court est avant
    V_CMP(my_strcmp("abc", "ab"), >, 0);
    V_CMP(my_strcmp("", "a"), <, 0);
    V_CMP(my_strcmp("a", ""), >, 0);
    V_CMP(my_strcmp("A", "a"), <, 0);  // majuscules avant minuscules
    V_CMP(my_strcmp("\xC3\xA9", "z"), >, 0);  // octets > 127 : non signés, comme strcmp
    V_CMP(my_strcmp("abc", "abd"), ==, -1);  // différence des premiers octets distincts
}

static void test_my_strncmp(void)
{
    V_CMP(my_strncmp("abcdef", "abcxyz", 3), ==, 0);
    V_CMP(my_strncmp("abcdef", "abcxyz", 4), <, 0);
    V_CMP(my_strncmp("abc", "abc", 100), ==, 0);
    V_CMP(my_strncmp("abc", "abd", 0), ==, 0);
    V_CMP(my_strncmp("ab", "abc", 3), <, 0);
    V_CMP(my_strncmp("", "", 5), ==, 0);
    V_CMP(my_strncmp("b", "a", 1), >, 0);
    V_CMP(my_strncmp("a", "b", -2), ==, 0);  // n négatif : rien à comparer
}

static void test_my_strstr(void)
{
    char s[] = "hello world";

    V_CMP(my_strstr(s, "world"), ==, s + 6);
    V_CMP(my_strstr(s, "hello"), ==, s);
    V_CMP(my_strstr(s, "o"), ==, s + 4);  // première occurrence
    V_CMP(my_strstr(s, "xyz"), ==, NULL);
    V_CMP(my_strstr(s, ""), ==, s);  // motif vide : renvoie str
    V_CMP(my_strstr(s, "hello world!"), ==, NULL);  // motif plus long que la chaîne
    V_CMP(my_strstr(s, "ld"), ==, s + 9);  // à la toute fin
    V_CMP(my_strstr((char[]){""}, "a"), ==, NULL);
}

static void test_my_strcpy(void)
{
    char d[16];

    memset(d, 'x', sizeof(d));
    V_CMP(my_strcpy(d, "abc"), ==, d);
    V_TEXTE_EGAL(d, "abc");  // le '\0' est copié, rien au-delà
    V_CMP(d[4], ==, 'x');  // le '\0' est copié, rien au-delà
    V_CMP(my_strcpy(d, ""), ==, d);
    V_CMP(d[0], ==, '\0');
    my_strcpy(d, "0123456789abcde");
    V_TEXTE_EGAL(d, "0123456789abcde");
}

static void test_my_strncpy(void)
{
    char d[8];

    memset(d, 'x', sizeof(d));
    my_strncpy(d, "abcdef", 3);
    V_CMP(strncmp(d, "abc", 3), ==, 0);  // pas de '\0' si n <= longueur
    V_CMP(d[3], ==, 'x');  // pas de '\0' si n <= longueur
    memset(d, 'x', sizeof(d));
    my_strncpy(d, "ab", 5);
    V_CMP(d[0], ==, 'a');
    V_CMP(d[1], ==, 'b');
    V_CMP(d[2], ==, '\0');
    V_CMP(d[4], ==, '\0');
    V_CMP(d[5], ==, 'x');
    memset(d, 'x', sizeof(d));
    my_strncpy(d, "abc", 0);
    V_CMP(d[0], ==, 'x');
    V_CMP(my_strncpy(d, "z", 1), ==, d);
    V_CMP(d[0], ==, 'z');
}

static void test_my_strcat(void)
{
    char d[16] = "foo";

    V_CMP(my_strcat(d, "bar"), ==, d);
    V_TEXTE_EGAL(d, "foobar");
    my_strcat(d, "");
    V_TEXTE_EGAL(d, "foobar");
    d[0] = '\0';
    my_strcat(d, "x");
    V_TEXTE_EGAL(d, "x");
}

static void test_my_strncat(void)
{
    char d[16] = "foo";

    V_CMP(my_strncat(d, "barbaz", 3), ==, d);
    V_TEXTE_EGAL(d, "foobar");
    my_strncat(d, "!", 10);
    V_TEXTE_EGAL(d, "foobar!");
    my_strncat(d, "zzz", 0);
    V_TEXTE_EGAL(d, "foobar!");  // n = 0 : rien, chaîne toujours terminée
    d[0] = '\0';
    my_strncat(d, "ab", 1);
    V_TEXTE_EGAL(d, "a");
}

static void test_my_strdup(void)
{
    char *c = my_strdup("copie");

    V_CMP_BLOQUANT(c, !=, NULL);
    V_TEXTE_EGAL(c, "copie");
    c[0] = 'C';
    V_TEXTE_EGAL(c, "Copie");  // c'est bien une copie indépendante
    free(c);
    c = my_strdup("");
    V_CMP_BLOQUANT(c, !=, NULL);
    V_CMP(c[0], ==, '\0');
    free(c);
}

static void test_my_revstr(void)
{
    char a[] = "abcd";
    char b[] = "abc";
    char c[] = "x";
    char d[] = "";

    V_CMP(my_revstr(a), ==, a);
    V_TEXTE_EGAL(a, "dcba");
    my_revstr(b);
    V_TEXTE_EGAL(b, "cba");  // longueur impaire
    my_revstr(c);
    V_TEXTE_EGAL(c, "x");
    my_revstr(d);
    V_CMP(d[0], ==, '\0');
}

static void test_my_strupcase(void)
{
    char s[] = "abc XyZ 123 é!";

    V_CMP(my_strupcase(s), ==, s);
    V_TEXTE_EGAL(s, "ABC XYZ 123 é!");  // seuls a-z changent
}

static void test_my_strlowcase(void)
{
    char s[] = "ABC xYz 123 É!";

    V_CMP(my_strlowcase(s), ==, s);
    V_TEXTE_EGAL(s, "abc xyz 123 É!");
}

static void test_my_strcapitalize(void)
{
    char a[] = "hey, how are you? 42WORDS forty-two+fifty";
    char b[] = "";
    char c[] = "aBC";

    V_CMP(my_strcapitalize(a), ==, a);
    V_TEXTE_EGAL(a, "Hey, How Are You? 42words Forty-Two+Fifty");
    my_strcapitalize(b);
    V_CMP(b[0], ==, '\0');
    my_strcapitalize(c);
    V_TEXTE_EGAL(c, "Abc");
}

static void test_my_str_isalpha(void)
{
    V_CMP(my_str_isalpha("abcXYZ"), ==, 1);
    V_CMP(my_str_isalpha(""), ==, 1);  // vide : vrai par convention
    V_CMP(my_str_isalpha("ab1"), ==, 0);
    V_CMP(my_str_isalpha("ab c"), ==, 0);
    V_CMP(my_str_isalpha("é"), ==, 0);
}

static void test_my_str_islower(void)
{
    V_CMP(my_str_islower("abc"), ==, 1);
    V_CMP(my_str_islower(""), ==, 1);
    V_CMP(my_str_islower("abC"), ==, 0);
    V_CMP(my_str_islower("a b"), ==, 0);
}

static void test_my_str_isupper(void)
{
    V_CMP(my_str_isupper("ABC"), ==, 1);
    V_CMP(my_str_isupper(""), ==, 1);
    V_CMP(my_str_isupper("ABc"), ==, 0);
    V_CMP(my_str_isupper("A1"), ==, 0);
}

static void test_my_str_isnum(void)
{
    V_CMP(my_str_isnum("0123456789"), ==, 1);
    V_CMP(my_str_isnum(""), ==, 1);
    V_CMP(my_str_isnum("12a"), ==, 0);
    V_CMP(my_str_isnum("-1"), ==, 0);
    V_CMP(my_str_isnum("1 2"), ==, 0);
}

static void test_my_str_isprintable(void)
{
    V_CMP(my_str_isprintable("Hello, World ~!"), ==, 1);
    V_CMP(my_str_isprintable(""), ==, 1);
    V_CMP(my_str_isprintable("a\nb"), ==, 0);  // '\n' (10) n'est pas imprimable
    V_CMP(my_str_isprintable("tab\t"), ==, 0);
    V_CMP(my_str_isprintable("\x7F"), ==, 0);  // 127
    V_CMP(my_str_isprintable(" "), ==, 1);  // 32 l'est
}

void tests_lib_chaines(void)
{
    LANCER(test_my_strlen);
    LANCER(test_my_strcmp_cas);
    LANCER(test_my_strncmp);
    LANCER(test_my_strstr);
    LANCER(test_my_strcpy);
    LANCER(test_my_strncpy);
    LANCER(test_my_strcat);
    LANCER(test_my_strncat);
    LANCER(test_my_strdup);
    LANCER(test_my_revstr);
    LANCER(test_my_strupcase);
    LANCER(test_my_strlowcase);
    LANCER(test_my_strcapitalize);
    LANCER(test_my_str_isalpha);
    LANCER(test_my_str_islower);
    LANCER(test_my_str_isupper);
    LANCER(test_my_str_isnum);
    LANCER(test_my_str_isprintable);
}
