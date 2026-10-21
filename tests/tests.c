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
#include "bataille.h"

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

static personnage_t soigneur(const char *nom, int pv, int soin)
{
    personnage_t p = perso(nom, pv, 10, 0, PRETRE);

    p.soin = soin;
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
    b.pv = 0;
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
    personnage_t pr = perso("Pr", 50, 50, 50, PRETRE);

    p.soin = 20;
    appliquer_classe(&p);
    assert(p.pv == 55 && p.atq == 65 && p.def == 45 && p.soin == 5);
    appliquer_classe(&g);                   // bornes STAT_MIN / STAT_MAX
    assert(g.pv == 99 && g.atq == 0 && g.def == 99);
    pr.soin = 20;
    appliquer_classe(&pr);
    assert(pr.soin == 35);                  // le Prêtre soigne mieux
    assert(stat_finale(GUERRIER, STAT_PV, 42) == 42);     // modificateur nul
}

static void test_sauvegarde_classes(void)
{
    roster_t r;

    roster_init(&r);
    ecrire_fichier("Ancien 50 30 20 1\n");  // fichier écrit avant le soin
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_OK && r.data[0].classe == MAGE);
    assert(r.data[0].soin == stat_finale(MAGE, STAT_SOIN, SOIN_MIGRATION));
    snprintf(r.data[0].nom, NOM_MAX, "Moine");
    r.data[0].classe = MOINE;
    assert(sauvegarder(&r, FICHIER_TEST) == SAUVEGARDE_OK);
    roster_liberer(&r);
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_OK && r.data[0].classe == MOINE);
    roster_liberer(&r);
    ecrire_fichier("Neo 50 30 20 22 40\n\nVieux 20 10 10 0\n");   // nouveau + ancien
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_OK && r.len == 2);
    assert(r.data[0].soin == 40 && r.data[0].classe == PRETRE);
    assert(r.data[1].soin == stat_finale(GUERRIER, STAT_SOIN, SOIN_MIGRATION));
    assert(sauvegarder(&r, FICHIER_TEST) == SAUVEGARDE_OK);
    roster_liberer(&r);
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_OK && r.data[0].soin == 40);
    roster_liberer(&r);
    ecrire_fichier("Trop 50 30 20 1 40 7\n");               // champ en trop
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_ERR_FORMAT);
    ecrire_fichier("Fort 50 30 20 1 100\n");                // soin hors limites
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_ERR_FORMAT);
    ecrire_fichier("Futur 50 30 20 28\n");  // classe inconnue de ce catalogue
    assert(charger(&r, FICHIER_TEST) == SAUVEGARDE_ERR_FORMAT && r.len == 0);
}

/* ------------------------------------------------------------------ */
/*  Combat en équipe                                                   */
/* ------------------------------------------------------------------ */

static void test_bataille_init(void)
{
    bataille_t b;
    personnage_t j[3] = {perso("Kaelen", 60, 40, 20, GUERRIER),
        soigneur("Elara", 40, 30), perso("Sylas", 45, 35, 10, ARCHER)};
    personnage_t a[2] = {perso("Varek", 70, 45, 5, BERSERKER),
        perso("Nyx", 30, 50, 10, ASSASSIN)};
    personnage_t trop[TAILLE_EQUIPE_MAX + 1];
    personnage_t mort = perso("Mort", 0, 10, 10, GUERRIER);

    for (int i = 0; i <= TAILLE_EQUIPE_MAX; i++)
        trop[i] = perso("T", 10, 10, 10, GUERRIER);
    assert(bataille_init(&b, j, 0, a, 2) == -1);
    assert(bataille_init(&b, trop, TAILLE_EQUIPE_MAX + 1, a, 2) == -1);
    assert(bataille_init(&b, &mort, 1, a, 2) == -1);
    assert(bataille_init(&b, j, 3, a, 2) == 0);
    assert(b.gagnant == -1 && b.nb_ordre == 5);
    // ordre alterné : Kaelen, Varek, Elara, Nyx, Sylas
    assert(b.ordre[0].equipe == 0 && b.ordre[1].equipe == 1 && b.ordre[2].index == 1);
    assert(b.ordre[4].equipe == 0 && b.ordre[4].index == 2);
    assert(bataille_actif(&b).equipe == 0 && bataille_actif(&b).index == 0);
}

static void test_bataille_attaque_et_ko(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j[1] = {perso("K", 50, 40, 0, GUERRIER)};
    personnage_t a[2] = {perso("V", 30, 10, 50, BERSERKER), perso("N", 10, 10, 0, MAGE)};
    int n = 0;

    assert(bataille_init(&b, j, 1, a, 2) == 0);
    assert(!bataille_cible_valide(&b, ACTION_ATTAQUER, (ref_t){0, 0}));  // allié
    n = bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev);
    assert(n == 1 && ev[0].type == EV_ATTAQUE && ev[0].valeur == 20);
    assert(bataille_lire(&b, (ref_t){1, 0})->pv == 10 && ev[0].pv_apres == 10);
    assert(bataille_actif(&b).equipe == 1 && bataille_actif(&b).index == 0);
    assert(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 0}, ev) == 1);
    assert(bataille_lire(&b, (ref_t){0, 0})->pv == 40);         // 10, défense 0
    n = bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 0}, ev);   // N attaque
    assert(n == 1 && bataille_lire(&b, (ref_t){0, 0})->pv == 30);
    n = bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 1}, ev);   // K tue N
    assert(n == 2 && ev[1].type == EV_KO && ev[0].pv_apres == 0);
    assert(bataille_lire(&b, (ref_t){1, 1})->pv == 0);           // jamais négatif
    assert(!bataille_cible_valide(&b, ACTION_ATTAQUER, (ref_t){1, 1}));  // vaincu
    assert(bataille_actif(&b).equipe == 1 && bataille_actif(&b).index == 0);
    assert(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 0}, ev) == 1);
    // N est vaincu : il ne joue plus, le tour revient à K
    assert(bataille_actif(&b).equipe == 0 && bataille_actif(&b).index == 0);
    n = bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev);
    assert(n == 3 && ev[1].type == EV_KO && ev[2].type == EV_VICTOIRE);
    assert(b.gagnant == 0 && ev[2].valeur == 0);
    assert(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev) == -1);  // bloqué
    assert(bataille_journal(&b, 0) != NULL);
}

static void test_bataille_degats_minimum(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j[1] = {perso("Faible", 10, 1, 0, GUERRIER)};
    personnage_t a[1] = {perso("Mur", 10, 1, 99, GARDIEN)};

    assert(bataille_init(&b, j, 1, a, 1) == 0);
    assert(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev) == 1);
    assert(ev[0].valeur == 1);                // au moins 1 dégât
}

static void test_bataille_soin(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j[2] = {soigneur("Elara", 100, 35), perso("K", 100, 10, 0, GUERRIER)};
    personnage_t a[1] = {perso("V", 99, 5, 0, BERSERKER)};
    personnage_t sans_soin[2] = {soigneur("Brute", 50, 0), perso("K", 50, 10, 0, GUERRIER)};

    assert(bataille_init(&b, j, 2, a, 1) == 0);
    assert(!bataille_action_possible(&b, ACTION_SOIGNER));  // personne n'est blessé
    bataille_combattant(&b, (ref_t){0, 1})->pv = 40;        // 40 PV sur 100
    assert(bataille_agir(&b, ACTION_SOIGNER, (ref_t){0, 1}, ev) == 1);
    assert(ev[0].type == EV_SOIN && ev[0].valeur == 35 && ev[0].pv_apres == 75);
    assert(bataille_agir(&b, ACTION_DEFENDRE, (ref_t){1, 0}, ev) >= 1);  // V
    assert(bataille_agir(&b, ACTION_DEFENDRE, (ref_t){0, 1}, ev) >= 1);  // K
    bataille_combattant(&b, (ref_t){0, 1})->pv = 90;        // 90 PV sur 100
    assert(bataille_actif(&b).equipe == 0 && bataille_actif(&b).index == 0);
    assert(bataille_agir(&b, ACTION_SOIGNER, (ref_t){0, 1}, ev) >= 1);
    assert(ev[0].valeur == 10 && ev[0].pv_apres == 100);     // plafonné aux PV max
    bataille_combattant(&b, (ref_t){0, 1})->pv = 0;
    assert(!bataille_cible_valide(&b, ACTION_SOIGNER, (ref_t){0, 1}));  // vaincu
    assert(bataille_init(&b, sans_soin, 2, a, 1) == 0);
    bataille_combattant(&b, (ref_t){0, 1})->pv = 10;
    assert(!bataille_action_possible(&b, ACTION_SOIGNER));  // soin nul
}

static void test_bataille_garde(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j[1] = {perso("K", 99, 10, 0, GUERRIER)};
    personnage_t a[1] = {perso("V", 99, 40, 0, BERSERKER)};
    int n = 0;

    assert(bataille_init(&b, j, 1, a, 1) == 0);
    assert(bataille_agir(&b, ACTION_DEFENDRE, (ref_t){0, 0}, ev) == 1);
    assert(ev[0].type == EV_GARDE && combattant_en_garde(bataille_lire(&b, (ref_t){0, 0})));
    assert(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 0}, ev) >= 1);
    assert(ev[0].valeur == 20 && ev[0].garde == 1);   // 40 réduit de moitié
    // la garde a servi ; elle expire au début du tour suivant de K
    assert(ev[1].type == EV_GARDE_FIN && ev[1].acteur.equipe == 0);
    assert(!combattant_en_garde(bataille_lire(&b, (ref_t){0, 0})));
    n = bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev);
    assert(n == 1);
    assert(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 0}, ev) >= 1);
    assert(ev[0].valeur == 40 && ev[0].garde == 0);   // plus de garde
}

static void test_bataille_ia(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j[2] = {perso("K", 50, 10, 0, GUERRIER), perso("L", 50, 10, 0, GUERRIER)};
    personnage_t a[2] = {soigneur("Soi", 50, 20), perso("V", 80, 10, 0, BERSERKER)};
    action_t act = ACTION_ATTAQUER;
    ref_t cible = {0, 0};

    assert(bataille_init(&b, j, 2, a, 2) == 0);
    bataille_combattant(&b, (ref_t){0, 1})->pv = 5;
    assert(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 1}, ev) >= 1);  // K → V
    bataille_ia(&b, &act, &cible);          // Soi : personne sous 50 % chez lui
    assert(act == ACTION_ATTAQUER && cible.equipe == 0 && cible.index == 1);
    bataille_combattant(&b, (ref_t){1, 1})->pv = 30;
    bataille_ia(&b, &act, &cible);          // V à 30/80 : Soi le soigne
    assert(act == ACTION_SOIGNER && cible.index == 1);
    assert(bataille_agir(&b, act, cible, ev) >= 1 && ev[0].valeur == 20);
}

/** Un combat entier joué par l'ordinateur des deux côtés se termine. */
static void test_bataille_complete(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j[4];
    personnage_t a[3];
    action_t act = ACTION_ATTAQUER;
    ref_t cible = {0, 0};
    int tours = 0;

    for (int i = 0; i < 4; i++)
        j[i] = perso("J", 40 + i * 10, 20 + i * 5, i * 10, (classe_t)i);
    for (int i = 0; i < 3; i++)
        a[i] = soigneur("A", 50 + i * 10, 15);
    assert(bataille_init(&b, j, 4, a, 3) == 0);
    while (b.gagnant == -1 && tours < 1000) {
        bataille_ia(&b, &act, &cible);
        assert(bataille_agir(&b, act, cible, ev) > 0);
        tours++;
    }
    assert(b.gagnant != -1);
    for (int e = 0; e < NB_EQUIPES; e++)
        for (int i = 0; i < b.equipes[e].taille; i++)
            assert(b.equipes[e].membres[i].pv >= 0
                && b.equipes[e].membres[i].pv <= b.equipes[e].membres[i].pv_max);
    assert(j[0].pv == 40);                  // les originaux ne sont pas touchés
}

int main(void)
{
    test_bataille_init();
    test_bataille_attaque_et_ko();
    test_bataille_degats_minimum();
    test_bataille_soin();
    test_bataille_garde();
    test_bataille_ia();
    test_bataille_complete();
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
