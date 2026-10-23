/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests.c ────────────────────────────────────────────────────────────
** │ But         Tests automatiques des règles du jeu, avec assert() : si
** │             une règle casse, le programme s'arrête en indiquant la
** │             ligne.
** │ Contenu     combat en équipe (init, attaque, soin, garde, IA, partie
** │             complète), catalogue de classes, sauvegarde, liste de
** │             personnages, my_strcmp
** │ Lancer      make tests
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <stdlib.h>
#include "boite.h"
#include "my_string.h"
#include "personnage.h"
#include "roster.h"
#include "sauvegarde.h"
#include "bataille.h"
#include "lanceur.h"
#include "tests_bataille.h"
#include "tests_console.h"
#include "tests_donnees.h"
#include "tests_fuzz.h"
#include "tests_lib.h"
#include "tests_null.h"
#include "tests_ui_console.h"

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

    V_CMP_BLOQUANT(f, !=, NULL);
    fputs(contenu, f);
    fclose(f);
}

static void test_my_strcmp(void)
{
    V_TEXTE_EGAL("abc", "abc");
    V_TEXTE_DIFF("abc", "abd");
    V_TEXTE_DIFF("ab", "abc");
    V_TEXTE_EGAL("", "");
}

static void test_degats(void)
{
    personnage_t a = perso("A", 50, 40, 0, GUERRIER);
    personnage_t b = perso("B", 50, 40, 50, MAGE);
    personnage_t c = perso("C", 50, 40, 100, ARCHER);

    V_CMP(calculer_degats(&a, &b), ==, 20);  // 40 × (100 - 50) / 100
    V_CMP(calculer_degats(&b, &a), ==, 40);
    V_CMP(calculer_degats(&a, &c), ==, 0);
    b.pv = 0;
    V_VRAI(est_ko(&b));
}

static void test_trouver_personnage(void)
{
    personnage_t tab[2] = { perso("Bob", 1, 1, 1, MAGE),
        perso("Alice", 1, 1, 1, ARCHER) };

    V_CMP(trouver_personnage(tab, 2, "Alice"), ==, 1);
    V_CMP(trouver_personnage(tab, 2, "Bob"), ==, 0);
    V_CMP(trouver_personnage(tab, 2, "bob"), ==, -1);
    V_CMP(trouver_personnage(tab, 0, "Bob"), ==, -1);
}

static void test_roster(void)
{
    roster_t r;
    personnage_t p = perso("X", 1, 1, 1, GUERRIER);

    roster_init(&r);
    V_CMP(r.len, ==, 0);
    V_CMP(r.data, ==, NULL);
    for (int i = 0; i < 100; i++) {
        p.pv = i;
        V_CMP(roster_ajouter(&r, &p), ==, 0);
    }
    V_CMP(r.len, ==, 100);
    V_CMP(r.capacite, >=, 100);
    V_CMP(r.data[0].pv, ==, 0);
    V_CMP(r.data[99].pv, ==, 99);
    roster_liberer(&r);
    V_CMP(r.len, ==, 0);
    V_CMP(r.data, ==, NULL);
    V_CMP(r.capacite, ==, 0);
}

static void test_sauvegarde_aller_retour(void)
{
    roster_t r;
    roster_t relu;
    personnage_t bob = perso("Bob", 50, 30, 20, GUERRIER);
    personnage_t ali = perso("Alice", 40, 45, 10, MAGE);

    roster_init(&r);
    roster_init(&relu);
    V_CMP(roster_ajouter(&r, &bob), ==, 0);
    V_CMP(roster_ajouter(&r, &ali), ==, 0);
    V_CMP(sauvegarder(&r, FICHIER_TEST), ==, SAUVEGARDE_OK);
    V_CMP(charger(&relu, FICHIER_TEST), ==, SAUVEGARDE_OK);
    V_CMP(relu.len, ==, 2);
    V_TEXTE_EGAL(relu.data[1].nom, "Alice");
    V_CMP(relu.data[1].pv, ==, 40);
    V_CMP(relu.data[1].atq, ==, 45);
    V_CMP(relu.data[1].def, ==, 10);
    V_CMP(relu.data[1].classe, ==, MAGE);
    roster_liberer(&r);
    roster_liberer(&relu);
}

static void test_charger_refuse_fichier_invalide(void)
{
    roster_t r;
    personnage_t bob = perso("Bob", 50, 30, 20, GUERRIER);

    roster_init(&r);
    V_CMP(roster_ajouter(&r, &bob), ==, 0);
    V_CMP(charger(&r, "n_existe_pas.txt"), ==, SAUVEGARDE_ERR_OUVERTURE);
    ecrire_fichier("Al 10 10 10 99\n");         // classe inconnue
    V_CMP(charger(&r, FICHIER_TEST), ==, SAUVEGARDE_ERR_FORMAT);
    ecrire_fichier("Al 500 10 10 1\n");         // stat hors limites
    V_CMP(charger(&r, FICHIER_TEST), ==, SAUVEGARDE_ERR_FORMAT);
    ecrire_fichier("Al 10 10 10 1\nAl 1 1 1 1\n");   // nom en double
    V_CMP(charger(&r, FICHIER_TEST), ==, SAUVEGARDE_ERR_FORMAT);
    ecrire_fichier("Al 10 10\n");               // ligne tronquée
    V_CMP(charger(&r, FICHIER_TEST), ==, SAUVEGARDE_ERR_FORMAT);
    ecrire_fichier("Al dix 10 10 1\n");         // pas un nombre
    V_CMP(charger(&r, FICHIER_TEST), ==, SAUVEGARDE_ERR_FORMAT);
    V_CMP(r.len, ==, 1);
    V_TEXTE_EGAL(r.data[0].nom, "Bob");
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

    V_CMP(NB_CLASSES, >=, 20);
    for (int i = 0; i < NB_CLASSES; i++) {
        V_CMP_BLOQUANT(classes[i].nom, !=, NULL);
        V_CMP(classes[i].nom[0], !=, '\0');
        V_CMP_BLOQUANT(classes[i].description, !=, NULL);
        V_CMP(largeur_visible(classes[i].description), <=, DESCRIPTION_MAX);
        V_CMP(classes[i].categorie, <, NB_CATEGORIES);
        V_CMP(classes[i].embleme, <, NB_EMBLEMES);
        par_categorie[classes[i].categorie]++;
        somme = 0;
        for (int s = 0; s < NB_STATS; s++) {
            V_CMP(abs(classes[i].mods[s]), <=, MOD_MAX);
            somme += classes[i].mods[s];
        }
        V_CMP(somme, ==, 0);  // une classe n'est pas « plus forte »
        for (int j = 0; j < i; j++) {
            V_TEXTE_DIFF(classes[i].nom, classes[j].nom);
            ecart = abs(classes[i].couleur.r - classes[j].couleur.r)
                + abs(classes[i].couleur.g - classes[j].couleur.g)
                + abs(classes[i].couleur.b - classes[j].couleur.b);
            V_CMP(ecart, >=, 40);  // couleurs distinctes deux à deux
        }
    }
    for (int c = 0; c < NB_CATEGORIES; c++)
        do {
            V_CMP(par_categorie[c], >, 0);
            V_CMP_BLOQUANT(nom_categorie((categorie_t)c), !=, NULL);
        } while (0);
    ordre_classes(ordre);
    for (int i = 0; i < NB_CLASSES; i++)
        vues[ordre[i]]++;
    for (int i = 0; i < NB_CLASSES; i++)
        V_CMP(vues[i], ==, 1);  // ordre = permutation du catalogue
    V_CMP(GUERRIER, ==, 0);  // sauvegardes existantes
    V_CMP(MAGE, ==, 1);  // sauvegardes existantes
    V_CMP(ARCHER, ==, 2);  // sauvegardes existantes
    V_VRAI(classe_valide(0));
    V_VRAI(classe_valide(NB_CLASSES - 1));
    V_FAUX(classe_valide(-1));
    V_FAUX(classe_valide(NB_CLASSES));
}

static void test_modificateurs(void)
{
    personnage_t p = perso("P", 50, 50, 50, BERSERKER);
    personnage_t g = perso("G", 95, 5, 95, GARDIEN);
    personnage_t pr = perso("Pr", 50, 50, 50, PRETRE);

    p.soin = 20;
    appliquer_classe(&p);
    V_CMP(p.pv, ==, 55);
    V_CMP(p.atq, ==, 65);
    V_CMP(p.def, ==, 45);
    V_CMP(p.soin, ==, 5);
    appliquer_classe(&g);                   // bornes STAT_MIN / STAT_MAX
    V_CMP(g.pv, ==, 99);
    V_CMP(g.atq, ==, 0);
    V_CMP(g.def, ==, 99);
    pr.soin = 20;
    appliquer_classe(&pr);
    V_CMP(pr.soin, ==, 35);  // le Prêtre soigne mieux
    V_CMP(stat_finale(GUERRIER, STAT_PV, 42), ==, 42);  // modificateur nul
}

static void test_sauvegarde_classes(void)
{
    roster_t r;

    roster_init(&r);
    ecrire_fichier("Ancien 50 30 20 1\n");  // fichier écrit avant le soin
    V_CMP(charger(&r, FICHIER_TEST), ==, SAUVEGARDE_OK);
    V_CMP(r.data[0].classe, ==, MAGE);
    V_CMP(r.data[0].soin, ==, stat_finale(MAGE, STAT_SOIN, SOIN_MIGRATION));
    snprintf(r.data[0].nom, NOM_MAX, "Moine");
    r.data[0].classe = MOINE;
    V_CMP(sauvegarder(&r, FICHIER_TEST), ==, SAUVEGARDE_OK);
    roster_liberer(&r);
    V_CMP(charger(&r, FICHIER_TEST), ==, SAUVEGARDE_OK);
    V_CMP(r.data[0].classe, ==, MOINE);
    roster_liberer(&r);
    ecrire_fichier("Neo 50 30 20 22 40\n\nVieux 20 10 10 0\n");   // nouveau + ancien
    V_CMP(charger(&r, FICHIER_TEST), ==, SAUVEGARDE_OK);
    V_CMP(r.len, ==, 2);
    V_CMP(r.data[0].soin, ==, 40);
    V_CMP(r.data[0].classe, ==, PRETRE);
    V_CMP(r.data[1].soin, ==, stat_finale(GUERRIER, STAT_SOIN, SOIN_MIGRATION));
    V_CMP(sauvegarder(&r, FICHIER_TEST), ==, SAUVEGARDE_OK);
    roster_liberer(&r);
    V_CMP(charger(&r, FICHIER_TEST), ==, SAUVEGARDE_OK);
    V_CMP(r.data[0].soin, ==, 40);
    roster_liberer(&r);
    ecrire_fichier("Trop 50 30 20 1 40 7\n");               // champ en trop
    V_CMP(charger(&r, FICHIER_TEST), ==, SAUVEGARDE_ERR_FORMAT);
    ecrire_fichier("Fort 50 30 20 1 100\n");                // soin hors limites
    V_CMP(charger(&r, FICHIER_TEST), ==, SAUVEGARDE_ERR_FORMAT);
    ecrire_fichier("Futur 50 30 20 28\n");  // classe inconnue de ce catalogue
    V_CMP(charger(&r, FICHIER_TEST), ==, SAUVEGARDE_ERR_FORMAT);
    V_CMP(r.len, ==, 0);
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
    V_CMP(bataille_init(&b, j, 0, a, 2), ==, -1);
    V_CMP(bataille_init(&b, trop, TAILLE_EQUIPE_MAX + 1, a, 2), ==, -1);
    V_CMP(bataille_init(&b, &mort, 1, a, 2), ==, -1);
    V_CMP(bataille_init(&b, j, 3, a, 2), ==, 0);
    V_CMP(b.gagnant, ==, -1);
    V_CMP(b.nb_ordre, ==, 5);
    // ordre alterné : Kaelen, Varek, Elara, Nyx, Sylas
    V_CMP(b.ordre[0].equipe, ==, 0);
    V_CMP(b.ordre[1].equipe, ==, 1);
    V_CMP(b.ordre[2].index, ==, 1);
    V_CMP(b.ordre[4].equipe, ==, 0);
    V_CMP(b.ordre[4].index, ==, 2);
    V_CMP(bataille_actif(&b).equipe, ==, 0);
    V_CMP(bataille_actif(&b).index, ==, 0);
}

static void test_bataille_attaque_et_ko(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j[1] = {perso("K", 50, 40, 0, GUERRIER)};
    personnage_t a[2] = {perso("V", 30, 10, 50, BERSERKER), perso("N", 10, 10, 0, MAGE)};
    int n = 0;

    V_CMP(bataille_init(&b, j, 1, a, 2), ==, 0);
    V_FAUX(bataille_cible_valide(&b, ACTION_ATTAQUER, (ref_t){0, 0}));  // allié
    n = bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev);
    V_CMP(n, ==, 1);
    V_CMP(ev[0].type, ==, EV_ATTAQUE);
    V_CMP(ev[0].valeur, ==, 20);
    V_CMP(bataille_lire(&b, (ref_t){1, 0})->pv, ==, 10);
    V_CMP(ev[0].pv_apres, ==, 10);
    V_CMP(bataille_actif(&b).equipe, ==, 1);
    V_CMP(bataille_actif(&b).index, ==, 0);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 0}, ev), ==, 1);
    V_CMP(bataille_lire(&b, (ref_t){0, 0})->pv, ==, 40);  // 10, défense 0
    n = bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 0}, ev);   // N attaque
    V_CMP(n, ==, 1);
    V_CMP(bataille_lire(&b, (ref_t){0, 0})->pv, ==, 30);
    n = bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 1}, ev);   // K tue N
    V_CMP(n, ==, 2);
    V_CMP(ev[1].type, ==, EV_KO);
    V_CMP(ev[0].pv_apres, ==, 0);
    V_CMP(bataille_lire(&b, (ref_t){1, 1})->pv, ==, 0);  // jamais négatif
    V_FAUX(bataille_cible_valide(&b, ACTION_ATTAQUER, (ref_t){1, 1}));  // vaincu
    V_CMP(bataille_actif(&b).equipe, ==, 1);
    V_CMP(bataille_actif(&b).index, ==, 0);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 0}, ev), ==, 1);
    // N est vaincu : il ne joue plus, le tour revient à K
    V_CMP(bataille_actif(&b).equipe, ==, 0);
    V_CMP(bataille_actif(&b).index, ==, 0);
    n = bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev);
    V_CMP(n, ==, 3);
    V_CMP(ev[1].type, ==, EV_KO);
    V_CMP(ev[2].type, ==, EV_VICTOIRE);
    V_CMP(b.gagnant, ==, 0);
    V_CMP(ev[2].valeur, ==, 0);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev), ==, -1);  // bloqué
    V_CMP_BLOQUANT(bataille_journal(&b, 0), !=, NULL);
}

static void test_bataille_degats_minimum(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j[1] = {perso("Faible", 10, 1, 0, GUERRIER)};
    personnage_t a[1] = {perso("Mur", 10, 1, 99, GARDIEN)};

    V_CMP(bataille_init(&b, j, 1, a, 1), ==, 0);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev), ==, 1);
    V_CMP(ev[0].valeur, ==, 1);  // au moins 1 dégât
}

static void test_bataille_soin(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j[2] = {soigneur("Elara", 100, 35), perso("K", 100, 10, 0, GUERRIER)};
    personnage_t a[1] = {perso("V", 99, 5, 0, BERSERKER)};
    personnage_t sans_soin[2] = {soigneur("Brute", 50, 0), perso("K", 50, 10, 0, GUERRIER)};

    V_CMP(bataille_init(&b, j, 2, a, 1), ==, 0);
    V_FAUX(bataille_action_possible(&b, ACTION_SOIGNER));  // personne n'est blessé
    bataille_combattant(&b, (ref_t){0, 1})->pv = 40;        // 40 PV sur 100
    V_CMP(bataille_agir(&b, ACTION_SOIGNER, (ref_t){0, 1}, ev), ==, 1);
    V_CMP(ev[0].type, ==, EV_SOIN);
    V_CMP(ev[0].valeur, ==, 35);
    V_CMP(ev[0].pv_apres, ==, 75);
    V_CMP(bataille_agir(&b, ACTION_DEFENDRE, (ref_t){1, 0}, ev), >=, 1);  // V
    V_CMP(bataille_agir(&b, ACTION_DEFENDRE, (ref_t){0, 1}, ev), >=, 1);  // K
    bataille_combattant(&b, (ref_t){0, 1})->pv = 90;        // 90 PV sur 100
    V_CMP(bataille_actif(&b).equipe, ==, 0);
    V_CMP(bataille_actif(&b).index, ==, 0);
    V_CMP(bataille_agir(&b, ACTION_SOIGNER, (ref_t){0, 1}, ev), >=, 1);
    V_CMP(ev[0].valeur, ==, 10);  // plafonné aux PV max
    V_CMP(ev[0].pv_apres, ==, 100);  // plafonné aux PV max
    bataille_combattant(&b, (ref_t){0, 1})->pv = 0;
    V_FAUX(bataille_cible_valide(&b, ACTION_SOIGNER, (ref_t){0, 1}));  // vaincu
    V_CMP(bataille_init(&b, sans_soin, 2, a, 1), ==, 0);
    bataille_combattant(&b, (ref_t){0, 1})->pv = 10;
    V_FAUX(bataille_action_possible(&b, ACTION_SOIGNER));  // soin nul
}

static void test_bataille_garde(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j[1] = {perso("K", 99, 10, 0, GUERRIER)};
    personnage_t a[1] = {perso("V", 99, 40, 0, BERSERKER)};
    int n = 0;

    V_CMP(bataille_init(&b, j, 1, a, 1), ==, 0);
    V_CMP(bataille_agir(&b, ACTION_DEFENDRE, (ref_t){0, 0}, ev), ==, 1);
    V_CMP(ev[0].type, ==, EV_GARDE);
    V_VRAI(combattant_en_garde(bataille_lire(&b, (ref_t){0, 0})));
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 0}, ev), >=, 1);
    V_CMP(ev[0].valeur, ==, 20);  // 40 réduit de moitié
    V_CMP(ev[0].garde, ==, 1);  // 40 réduit de moitié
    // la garde a servi ; elle expire au début du tour suivant de K
    V_CMP(ev[1].type, ==, EV_GARDE_FIN);
    V_CMP(ev[1].acteur.equipe, ==, 0);
    V_FAUX(combattant_en_garde(bataille_lire(&b, (ref_t){0, 0})));
    n = bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev);
    V_CMP(n, ==, 1);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 0}, ev), >=, 1);
    V_CMP(ev[0].valeur, ==, 40);  // plus de garde
    V_CMP(ev[0].garde, ==, 0);  // plus de garde
}

static void test_bataille_ia(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j[2] = {perso("K", 50, 10, 0, GUERRIER), perso("L", 50, 10, 0, GUERRIER)};
    personnage_t a[2] = {soigneur("Soi", 50, 20), perso("V", 80, 10, 0, BERSERKER)};
    action_t act = ACTION_ATTAQUER;
    ref_t cible = {0, 0};

    V_CMP(bataille_init(&b, j, 2, a, 2), ==, 0);
    bataille_combattant(&b, (ref_t){0, 1})->pv = 5;
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 1}, ev), >=, 1);  // K → V

    bataille_ia(NULL, NULL, NULL);

    bataille_ia(&b, &act, &cible);          // Soi : personne sous 50 % chez lui
    V_CMP(act, ==, ACTION_ATTAQUER);
    V_CMP(cible.equipe, ==, 0);
    V_CMP(cible.index, ==, 1);
    bataille_combattant(&b, (ref_t){1, 1})->pv = 30;
    bataille_combattant(&b, (ref_t){0, 1})->pv = 50;       // plus achevable : il soigne
    bataille_ia(&b, &act, &cible);          // V à 30/80 : Soi le soigne
    V_CMP(act, ==, ACTION_SOIGNER);
    V_CMP(cible.index, ==, 1);
    V_CMP(bataille_agir(&b, act, cible, ev), >=, 1);
    V_CMP(ev[0].valeur, ==, 20);
}

/** L'ordinateur ne se soigne jamais indéfiniment, mais achève toujours un ennemi. */
static void test_bataille_ia_lassitude(void)
{
    bataille_t b;
    personnage_t j = perso("K", 100, 10, 0, GUERRIER);
    personnage_t a = soigneur("Soi", 100, 60);
    action_t act = ACTION_ATTAQUER;
    ref_t cible = {0, 0};
    int defensifs = 0;

    V_CMP(bataille_init(&b, &j, 1, &a, 1), ==, 0);
    b.tour = 1;                                 // c'est à l'ordinateur de jouer
    bataille_combattant(&b, (ref_t){1, 0})->pv = 10;
    for (int i = 0; i < 200; i++) {             // l'adversaire ne l'attaque jamais
        bataille_ia(&b, &act, &cible);
        V_BOOL(act == ACTION_ATTAQUER || defensifs < DEFENSIFS_MAX);
        defensifs = act == ACTION_ATTAQUER ? 0 : defensifs + 1;
        bataille_combattant(&b, (ref_t){1, 0})->defensifs = defensifs;
        bataille_combattant(&b, (ref_t){1, 0})->pv = 10;
    }
    bataille_combattant(&b, (ref_t){0, 0})->pv = 5;     // achevable : toujours attaquer
    bataille_combattant(&b, (ref_t){1, 0})->defensifs = 0;
    for (int i = 0; i < 200; i++) {
        bataille_ia(&b, &act, &cible);
        V_CMP(act, ==, ACTION_ATTAQUER);
        V_CMP(cible.equipe, ==, 0);
    }
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
    V_CMP(bataille_init(&b, j, 4, a, 3), ==, 0);
    while (b.gagnant == -1 && tours < 1000) {
        bataille_ia(&b, &act, &cible);
        V_CMP(bataille_agir(&b, act, cible, ev), >, 0);
        tours++;
    }
    V_CMP(b.gagnant, !=, -1);
    for (int e = 0; e < NB_EQUIPES; e++)
        for (int i = 0; i < b.equipes[e].taille; i++)
            do {
                V_CMP(b.equipes[e].membres[i].pv, >=, 0);
                V_CMP(b.equipes[e].membres[i].pv, <=, b.equipes[e].membres[i].pv_max);
            } while (0);
    V_CMP(j[0].pv, ==, 40);  // les originaux ne sont pas touchés
}

int main(void)
{
    LANCER(test_bataille_init);
    LANCER(test_bataille_attaque_et_ko);
    LANCER(test_bataille_degats_minimum);
    LANCER(test_bataille_soin);
    LANCER(test_bataille_garde);
    LANCER(test_bataille_ia);
    LANCER(test_bataille_ia_lassitude);
    LANCER(test_bataille_complete);
    LANCER(test_catalogue);
    LANCER(test_modificateurs);
    LANCER(test_sauvegarde_classes);
    LANCER(test_my_strcmp);
    LANCER(test_degats);
    LANCER(test_trouver_personnage);
    LANCER(test_roster);
    LANCER(test_sauvegarde_aller_retour);
    LANCER(test_charger_refuse_fichier_invalide);
    tests_lib_chaines();
    tests_lib_nombres();
    tests_lib_affichage();
    tests_personnage();
    tests_roster();
    tests_sauvegarde();
    tests_bataille();
    tests_ui_console();
    tests_console();
    tests_null();
    tests_fuzz();
    remove(FICHIER_TEST);
    remove("tests_clavier_tmp.txt");
    if (system("rm -rf /tmp/redstrings_tests") != 0)       /* dossiers jetables des tests de la console */
        fprintf(stderr, "nettoyage de /tmp/redstrings_tests impossible\n");
    return lanceur_bilan();
}
