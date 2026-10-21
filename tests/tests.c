/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <assert.h>
#include <stdio.h>
#include "my_string.h"
#include "personnage.h"
#include "roster.h"
#include "sauvegarde.h"

#define FICHIER_TEST "tests_tmp.txt"

static personnage_t perso(const char *nom, int pv, int atq, int def,
    classe_t classe)
{
    personnage_t p = {0};

    my_strcpy(p.nom, nom);
    p.pv = pv;
    p.atq = atq;
    p.def = def;
    p.classe = classe;
    return p;
}

static void ecrire_fichier(const char *contenu)
{
    FILE *f = fopen(FICHIER_TEST, "w");

    assert(f != NULL);
    fputs(contenu, f);
    fclose(f);
}

static void test_my_strcmp(void)
{
    assert(my_strcmp("abc", "abc") == 0);
    assert(my_strcmp("abc", "abd") != 0);
    assert(my_strcmp("ab", "abc") != 0);
    assert(my_strcmp("", "") == 0);
}

static void test_degats(void)
{
    personnage_t a = perso("A", 50, 40, 0, GUERRIER);
    personnage_t b = perso("B", 50, 40, 50, MAGE);
    personnage_t c = perso("C", 50, 40, 100, ARCHER);

    assert(calculer_degats(&a, &b) == 20);      // 40 × (100 - 50) / 100
    assert(calculer_degats(&b, &a) == 40);
    assert(calculer_degats(&a, &c) == 0);
    attaquer(&a, &b);
    assert(b.pv == 30);
}

static void test_trouver_personnage(void)
{
    personnage_t tab[2] = { perso("Bob", 1, 1, 1, MAGE),
        perso("Alice", 1, 1, 1, ARCHER) };

    assert(trouver_personnage(tab, 2, "Alice") == 1);
    assert(trouver_personnage(tab, 2, "Bob") == 0);
    assert(trouver_personnage(tab, 2, "bob") == -1);
    assert(trouver_personnage(tab, 0, "Bob") == -1);
}

static void test_roster(void)
{
    roster_t r;
    personnage_t p = perso("X", 1, 1, 1, GUERRIER);

    roster_init(&r);
    assert(r.len == 0 && r.data == NULL);
    for (int i = 0; i < 100; i++) {
        p.pv = i;
        assert(roster_ajouter(&r, &p) == 0);
    }
    assert(r.len == 100 && r.capacite >= 100);
    assert(r.data[0].pv == 0 && r.data[99].pv == 99);
    roster_liberer(&r);
    assert(r.len == 0 && r.data == NULL && r.capacite == 0);
}

static void test_sauvegarde_aller_retour(void)
{
    roster_t r;
    roster_t relu;
    personnage_t bob = perso("Bob", 50, 30, 20, GUERRIER);
    personnage_t ali = perso("Alice", 40, 45, 10, MAGE);

    roster_init(&r);
    roster_init(&relu);
    assert(roster_ajouter(&r, &bob) == 0 && roster_ajouter(&r, &ali) == 0);
    assert(sauvegarder(&r, FICHIER_TEST) == SAUVEGARDE_OK);
    assert(charger(&relu, FICHIER_TEST) == SAUVEGARDE_OK);
    assert(relu.len == 2);
    assert(my_strcmp(relu.data[1].nom, "Alice") == 0);
    assert(relu.data[1].pv == 40 && relu.data[1].atq == 45);
    assert(relu.data[1].def == 10 && relu.data[1].classe == MAGE);
    roster_liberer(&r);
    roster_liberer(&relu);
}

static void test_charger_refuse_fichier_invalide(void)
{
    roster_t r;
    personnage_t bob = perso("Bob", 50, 30, 20, GUERRIER);

    roster_init(&r);
    assert(roster_ajouter(&r, &bob) == 0);
    assert(charger(&r, "n_existe_pas.txt") == SAUVEGARDE_ERR_OUVERTURE);
    ecrire_fichier("Al 10 10 10 9\n");          // classe inconnue
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_ERR_FORMAT);
    ecrire_fichier("Al 500 10 10 1\n");         // stat hors limites
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_ERR_FORMAT);
    ecrire_fichier("Al 10 10 10 1\nAl 1 1 1 1\n");   // nom en double
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_ERR_FORMAT);
    ecrire_fichier("Al 10 10\n");               // ligne tronquée
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_ERR_FORMAT);
    ecrire_fichier("Al dix 10 10 1\n");         // pas un nombre
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_ERR_FORMAT);
    assert(r.len == 1 && my_strcmp(r.data[0].nom, "Bob") == 0);
    roster_liberer(&r);
}

int main(void)
{
    test_my_strcmp();
    test_degats();
    test_trouver_personnage();
    test_roster();
    test_sauvegarde_aller_retour();
    test_charger_refuse_fichier_invalide();
    remove(FICHIER_TEST);
    printf("  Tous les tests passent\n");
    return 0;
}
