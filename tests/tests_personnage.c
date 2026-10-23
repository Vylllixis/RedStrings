/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_personnage.c ─────────────────────────────────────────────────
** │ But         Un ou plusieurs tests par fonction de personnage.c,
** │             combat.c, afficher_personnage.c et creer_personnage.c.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <string.h>
#include "lanceur.h"
#include "outils_tests.h"
#include "personnage.h"
#include "saisie.h"
#include "tests_donnees.h"

static personnage_t un(const char *nom, int pv, int atq, int def, classe_t c)
{
    personnage_t p = {0};

    snprintf(p.nom, sizeof(p.nom), "%s", nom);
    p.pv = pv;
    p.atq = atq;
    p.def = def;
    p.classe = c;
    return p;
}

static void test_nom_categorie(void)
{
    V_TEXTE_EGAL(nom_categorie(CAT_CORPS_A_CORPS), "Corps à corps");
    V_TEXTE_EGAL(nom_categorie(CAT_MAGIE), "Magie");
    V_TEXTE_EGAL(nom_categorie(CAT_DISTANCE), "Distance");
    V_TEXTE_EGAL(nom_categorie(CAT_SOUTIEN), "Soutien");
    V_TEXTE_EGAL(nom_categorie(NB_CATEGORIES), "?");
}

static void test_couleur_ansi(void)
{
    char attendu[32];

    for (int c = 0; c < NB_CLASSES; c++) {
        snprintf(attendu, sizeof(attendu), "\033[38;2;%d;%d;%dm",
            classes[c].couleur.r, classes[c].couleur.g, classes[c].couleur.b);
        V_TEXTE_EGAL(couleur_ansi((classe_t)c), attendu);
        V_CMP(couleur_ansi((classe_t)c), ==, couleur_ansi((classe_t)c));  // même tampon réutilisé
    }
    V_TEXTE_EGAL(couleur_ansi((classe_t)NB_CLASSES), couleur_ansi(GUERRIER));
}

static void test_classe_valide(void)
{
    V_VRAI(classe_valide(0));
    V_VRAI(classe_valide(GUERRIER));
    V_VRAI(classe_valide(NB_CLASSES - 1));
    V_FAUX(classe_valide(-1));
    V_FAUX(classe_valide(NB_CLASSES));
    V_FAUX(classe_valide(100000));
    V_FAUX(classe_valide(-2147483647 - 1));
    V_FAUX(classe_valide(2147483647));
}

static void test_ordre_classes(void)
{
    classe_t ordre[NB_CLASSES];
    int vue[NB_CLASSES] = {0};

    ordre_classes(ordre);
    for (int i = 0; i < NB_CLASSES; i++) {
        V_VRAI(classe_valide((int)ordre[i]));
        vue[ordre[i]]++;
        if (i > 0)                          // rangées par catégorie croissante
            V_CMP(classes[ordre[i]].categorie, >=, classes[ordre[i - 1]].categorie);
    }
    for (int i = 0; i < NB_CLASSES; i++)
        V_CMP(vue[i], ==, 1);  // chaque classe exactement une fois
    V_CMP(classes[ordre[0]].categorie, ==, CAT_CORPS_A_CORPS);
    V_CMP(classes[ordre[NB_CLASSES - 1]].categorie, ==, CAT_SOUTIEN);
}

static void test_valeur_stat(void)
{
    personnage_t p = un("P", 11, 22, 33, GUERRIER);

    p.soin = 44;
    V_CMP(valeur_stat(&p, STAT_PV), ==, 11);
    V_CMP(valeur_stat(&p, STAT_ATQ), ==, 22);
    V_CMP(valeur_stat(&p, STAT_DEF), ==, 33);
    V_CMP(valeur_stat(&p, STAT_SOIN), ==, 44);
    V_CMP(valeur_stat(&p, NB_STATS), ==, 0);
    V_CMP(valeur_stat(&p, (stat_t)-1), ==, 0);
}

static void test_stat_finale(void)
{
    V_CMP(stat_finale(GUERRIER, STAT_PV, 50), ==, 50);  // modificateur nul
    V_CMP(stat_finale(BERSERKER, STAT_ATQ, 50), ==, 65);  // +15
    V_CMP(stat_finale(BERSERKER, STAT_DEF, 50), ==, 45);  // -5
    V_CMP(stat_finale(BERSERKER, STAT_ATQ, 90), ==, STAT_MAX);  // borne haute
    V_CMP(stat_finale(BERSERKER, STAT_DEF, 2), ==, STAT_MIN);  // borne basse
    V_CMP(stat_finale(GUERRIER, STAT_PV, -50), ==, STAT_MIN);
    V_CMP(stat_finale(GUERRIER, STAT_PV, 1000), ==, STAT_MAX);
    for (int c = 0; c < NB_CLASSES; c++)
        for (int s = 0; s < NB_STATS; s++)
            V_CMP(stat_finale((classe_t)c, (stat_t)s, 50), ==, 50 + classes[c].mods[s]);
}

static void test_appliquer_classe(void)
{
    personnage_t p = un("P", 50, 50, 50, PRETRE);

    p.soin = 20;
    appliquer_classe(&p);
    V_CMP(p.pv, ==, 55);
    V_CMP(p.atq, ==, 35);
    V_CMP(p.def, ==, 45);
    V_CMP(p.soin, ==, 35);
    V_TEXTE_EGAL(p.nom, "P");  // seules les stats changent
    V_CMP(p.classe, ==, PRETRE);  // seules les stats changent
    p.classe = (classe_t)NB_CLASSES;
    p.pv = 10;
    appliquer_classe(&p);
    V_CMP(p.pv, ==, 10);  // classe invalide : rien
}

static void test_trouver_personnage_cas(void)
{
    personnage_t t[3] = {un("Ann", 1, 1, 1, MAGE), un("Bob", 1, 1, 1, MAGE), un("Cat", 1, 1, 1, MAGE)};

    V_CMP(trouver_personnage(t, 3, "Ann"), ==, 0);
    V_CMP(trouver_personnage(t, 3, "Cat"), ==, 2);
    V_CMP(trouver_personnage(t, 3, "ann"), ==, -1);  // sensible à la casse
    V_CMP(trouver_personnage(t, 3, "An"), ==, -1);  // nom exact seulement
    V_CMP(trouver_personnage(t, 2, "Cat"), ==, -1);  // size borne la recherche
    V_CMP(trouver_personnage(t, 0, "Ann"), ==, -1);
}

static void test_nom_est_un_id_cas(void)
{
    V_VRAI(nom_est_un_id("0"));
    V_VRAI(nom_est_un_id("007"));
    V_VRAI(nom_est_un_id("1234567890123"));
    V_FAUX(nom_est_un_id(""));
    V_FAUX(nom_est_un_id("a1"));
    V_FAUX(nom_est_un_id("1a"));
    V_FAUX(nom_est_un_id("-1"));
    V_FAUX(nom_est_un_id("1 2"));
    V_FAUX(nom_est_un_id("1.5"));
}

static void test_calculer_degats(void)
{
    personnage_t a = un("A", 10, 40, 0, GUERRIER);
    personnage_t c = un("C", 10, 0, 50, GUERRIER);

    V_CMP(calculer_degats(&a, &c), ==, 20);  // 40 x (100 - 50) / 100
    c.def = 0;
    V_CMP(calculer_degats(&a, &c), ==, 40);
    c.def = 100;
    V_CMP(calculer_degats(&a, &c), ==, 0);
    c.def = 150;
    V_CMP(calculer_degats(&a, &c), ==, 0);  // jamais négatif
    c.def = 33;
    V_CMP(calculer_degats(&a, &c), ==, 26);  // arrondi vers le bas
    a.atq = 0;
    V_CMP(calculer_degats(&a, &c), ==, 0);
    a.atq = 2147483647;
    c.def = -100;                                                    // pas de dépassement d'int
    V_CMP(calculer_degats(&a, &c), ==, 2147483647);
}

static void test_est_ko(void)
{
    personnage_t p = un("P", 1, 1, 1, GUERRIER);

    V_FAUX(est_ko(&p));
    p.pv = 0;
    V_VRAI(est_ko(&p));
    p.pv = -5;
    V_VRAI(est_ko(&p));  // PV négatifs : K.O. aussi
}

static void test_afficher_personnage(void)
{
    personnage_t p = un("Merlin", 99, 10, 50, MAGE);
    char s[4096];

    p.soin = 5;
    capture_debut();
    afficher_personnage(&p);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "Merlin"));
    V_VRAI(contient(s, "Mage"));
    V_VRAI(contient(s, "Arcanes offensives"));
    V_VRAI(contient(s, "Magie"));
    V_VRAI(contient(s, "PV"));
    V_VRAI(contient(s, "ATK"));
    V_VRAI(contient(s, "DEF"));
    V_VRAI(contient(s, "SOI"));
    V_VRAI(contient(s, "╭"));
    V_VRAI(contient(s, "╰"));
    V_VRAI(contient(s, "█"));
    V_VRAI(contient(s, "99"));
    V_VRAI(contient(s, "50"));
    p.pv = 0;
    p.atq = 0;
    capture_debut();
    afficher_personnage(&p);                                         // stats à zéro : barres vides
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "░"));
}

static void test_creer_personnage_complet(void)
{
    personnage_t p = {0};
    personnage_t tab[1] = {un("Pris", 1, 1, 1, MAGE)};
    classe_t ordre[NB_CLASSES];
    int m = muet_commencer();

    ordre_classes(ordre);
    entree_simulee("Pris\nNeo\n50\n40\n30\n20\n1\n");               // « Pris » est refusé, puis Neo
    V_CMP(creer_personnage(&p, tab, 1), ==, 0);
    muet_finir(m);
    V_TEXTE_EGAL(p.nom, "Neo");
    V_CMP(p.classe, ==, ordre[0]);
    V_CMP(p.pv, ==, stat_finale(ordre[0], STAT_PV, 50));
    V_CMP(p.atq, ==, stat_finale(ordre[0], STAT_ATQ, 40));
    V_CMP(p.def, ==, stat_finale(ordre[0], STAT_DEF, 30));
    V_CMP(p.soin, ==, stat_finale(ordre[0], STAT_SOIN, 20));
}

static void test_creer_personnage_abandon(void)
{
    personnage_t p = {0};
    int m = muet_commencer();

    entree_simulee("Neo\n50\nq\n");                                  // q à la 2e statistique
    V_CMP(creer_personnage(&p, NULL, 0), ==, SAISIE_ANNULEE);
    entree_simulee("Neo\n50\n");                                     // Ctrl+D
    V_CMP(creer_personnage(&p, NULL, 0), ==, SAISIE_FIN);
    entree_simulee("");
    V_CMP(creer_personnage(&p, NULL, 0), ==, SAISIE_FIN);
    muet_finir(m);
}

void tests_personnage(void)
{
    LANCER(test_nom_categorie);
    LANCER(test_couleur_ansi);
    LANCER(test_classe_valide);
    LANCER(test_ordre_classes);
    LANCER(test_valeur_stat);
    LANCER(test_stat_finale);
    LANCER(test_appliquer_classe);
    LANCER(test_trouver_personnage_cas);
    LANCER(test_nom_est_un_id_cas);
    LANCER(test_calculer_degats);
    LANCER(test_est_ko);
    LANCER(test_afficher_personnage);
    LANCER(test_creer_personnage_complet);
    LANCER(test_creer_personnage_abandon);
}
