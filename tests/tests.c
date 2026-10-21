/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "boite.h"
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
    assert(subir_attaque(&a, &b) == 20 && b.pv == 30 && !est_ko(&b));
    b.pv = 50;
    attaquer(&a, &b);
    assert(b.pv == 30);
    b.pv = 5;
    attaquer(&a, &b);
    assert(b.pv == 0);      // jamais négatif, sinon le mort n'est pas détecté
    assert(est_ko(&b));
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
    ecrire_fichier("Al 10 10 10 99\n");         // classe inconnue
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

/** Chaque classe déclarée a des métadonnées complètes et cohérentes. */
static void test_catalogue(void)
{
    classe_t ordre[NB_CLASSES];
    int vues[NB_CLASSES] = {0};
    int par_categorie[NB_CATEGORIES] = {0};
    int somme = 0;
    int ecart = 0;

    assert(NB_CLASSES >= 20);
    for (int i = 0; i < NB_CLASSES; i++) {
        assert(classes[i].nom != NULL && classes[i].nom[0] != '\0');
        assert(classes[i].description != NULL);
        assert(largeur_visible(classes[i].description) <= DESCRIPTION_MAX);
        assert(classes[i].categorie < NB_CATEGORIES);
        assert(classes[i].embleme < NB_EMBLEMES);
        par_categorie[classes[i].categorie]++;
        somme = 0;
        for (int s = 0; s < NB_STATS; s++) {
            assert(abs(classes[i].mods[s]) <= MOD_MAX);
            somme += classes[i].mods[s];
        }
        assert(somme == 0);                 // une classe n'est pas « plus forte »
        for (int j = 0; j < i; j++) {
            assert(my_strcmp(classes[i].nom, classes[j].nom) != 0);
            ecart = abs(classes[i].couleur.r - classes[j].couleur.r)
                + abs(classes[i].couleur.g - classes[j].couleur.g)
                + abs(classes[i].couleur.b - classes[j].couleur.b);
            assert(ecart >= 40);            // couleurs distinctes deux à deux
        }
    }
    for (int c = 0; c < NB_CATEGORIES; c++)
        assert(par_categorie[c] > 0 && nom_categorie((categorie_t)c) != NULL);
    ordre_classes(ordre);
    for (int i = 0; i < NB_CLASSES; i++)
        vues[ordre[i]]++;
    for (int i = 0; i < NB_CLASSES; i++)
        assert(vues[i] == 1);               // ordre = permutation du catalogue
    assert(GUERRIER == 0 && MAGE == 1 && ARCHER == 2);  // sauvegardes existantes
    assert(classe_valide(0) && classe_valide(NB_CLASSES - 1));
    assert(!classe_valide(-1) && !classe_valide(NB_CLASSES));
}

static void test_modificateurs(void)
{
    personnage_t p = perso("P", 50, 50, 50, BERSERKER);
    personnage_t g = perso("G", 95, 5, 95, GARDIEN);

    appliquer_classe(&p);
    assert(p.pv == 55 && p.atq == 65 && p.def == 30);
    appliquer_classe(&g);                   // bornes STAT_MIN / STAT_MAX
    assert(g.pv == 99 && g.atq == 0 && g.def == 99);
    assert(stat_finale(GUERRIER, STAT_ATQ, 42) == 42);
}

static void test_sauvegarde_classes(void)
{
    roster_t r;

    roster_init(&r);
    ecrire_fichier("Ancien 50 30 20 1\n");  // fichier écrit avec 3 classes
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_OK && r.data[0].classe == MAGE);
    snprintf(r.data[0].nom, NOM_MAX, "Moine");
    r.data[0].classe = MOINE;
    assert(sauvegarder(&r, FICHIER_TEST) == SAUVEGARDE_OK);
    roster_liberer(&r);
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_OK && r.data[0].classe == MOINE);
    roster_liberer(&r);
    ecrire_fichier("Futur 50 30 20 28\n");  // classe inconnue de ce catalogue
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_ERR_FORMAT && r.len == 0);
}

int main(void)
{
    test_catalogue();
    test_modificateurs();
    test_sauvegarde_classes();
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
