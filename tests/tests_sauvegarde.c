/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_sauvegarde.c ─────────────────────────────────────────────────
** │ But         Un ou plusieurs tests par fonction de sauvegarde.c :
** │             écriture, lecture « tout ou rien », fusion, messages.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <string.h>
#include "lanceur.h"
#include "outils_tests.h"
#include "roster.h"
#include "sauvegarde.h"
#include "tests_donnees.h"

#define F "tests_sauv_tmp.txt"

static personnage_t nomme(const char *nom, int pv, classe_t c)
{
    personnage_t p = {0};

    snprintf(p.nom, sizeof(p.nom), "%s", nom);
    p.pv = pv;
    p.atq = 10;
    p.def = 20;
    p.soin = 30;
    p.classe = c;
    return p;
}

static void lire_tout(const char *chemin, char *buf, size_t taille)
{
    FILE *f = fopen(chemin, "r");
    size_t n = 0;

    V_CMP_BLOQUANT(f, !=, NULL);
    n = fread(buf, 1, taille - 1, f);
    buf[n] = '\0';
    fclose(f);
}

static void test_sauvegarder_format(void)
{
    roster_t r;
    personnage_t a = nomme("Ann", 50, MAGE);
    char s[256];

    roster_init(&r);
    roster_ajouter(&r, &a);
    V_CMP(sauvegarder(&r, F), ==, SAUVEGARDE_OK);
    lire_tout(F, s, sizeof(s));
    V_TEXTE_EGAL(s, "Ann 50 10 20 1 30\n");  // nom pv atq def classe soin
    roster_liberer(&r);
    roster_init(&r);
    V_CMP(sauvegarder(&r, F), ==, SAUVEGARDE_OK);  // liste vide : fichier vide
    lire_tout(F, s, sizeof(s));
    V_CMP(s[0], ==, '\0');
    remove(F);
}

static void test_sauvegarder_sans_fichier_temporaire(void)
{
    roster_t r;
    personnage_t a = nomme("Ann", 50, MAGE);

    roster_init(&r);
    roster_ajouter(&r, &a);
    V_CMP(sauvegarder(&r, F), ==, SAUVEGARDE_OK);
    V_CMP(fopen(F ".tmp", "r"), ==, NULL);  // le fichier temporaire est renommé
    roster_liberer(&r);
    remove(F);
}

static void test_sauvegarder_garde_l_ancien_en_cas_d_echec(void)
{
    roster_t r;
    personnage_t a = nomme("Ann", 50, MAGE);
    char s[256];

    ecrire_fichier_test(F, "Ancien 1 1 1 0 1\n");
    roster_init(&r);
    roster_ajouter(&r, &a);
    V_CMP(sauvegarder(&r, "dossier_inexistant/x.txt"), ==, SAUVEGARDE_ERR_OUVERTURE);
    V_CMP(sauvegarder(NULL, F), ==, SAUVEGARDE_ERR_OUVERTURE);
    lire_tout(F, s, sizeof(s));
    V_TEXTE_EGAL(s, "Ancien 1 1 1 0 1\n");  // intact
    roster_liberer(&r);
    remove(F);
}

static void test_charger_cas_valides(void)
{
    roster_t r;

    roster_init(&r);
    ecrire_fichier_test(F, "Ann 50 10 20 1 30\n\n   \nBob 5 6 7 0 8\n");   // lignes vides ignorées
    V_CMP(charger(&r, F), ==, SAUVEGARDE_OK);
    V_CMP(r.len, ==, 2);
    V_TEXTE_EGAL(r.data[0].nom, "Ann");
    V_CMP(r.data[0].classe, ==, MAGE);
    V_CMP(r.data[0].pv, ==, 50);
    V_CMP(r.data[0].atq, ==, 10);
    V_CMP(r.data[0].def, ==, 20);
    V_CMP(r.data[0].soin, ==, 30);
    V_TEXTE_EGAL(r.data[1].nom, "Bob");
    V_CMP(r.data[1].soin, ==, 8);
    ecrire_fichier_test(F, "Fin 1 2 3 0 4");                                // sans retour à la ligne final
    V_CMP(charger(&r, F), ==, SAUVEGARDE_OK);  // remplace tout
    V_CMP(r.len, ==, 1);  // remplace tout
    ecrire_fichier_test(F, "");
    V_CMP(charger(&r, F), ==, SAUVEGARDE_OK);  // fichier vide : liste vide
    V_CMP(r.len, ==, 0);  // fichier vide : liste vide
    roster_liberer(&r);
    remove(F);
}

static void test_charger_refus(void)
{
    static const char *mauvais[] = {
        "Ann 50 10 20\n", "Ann 50 10 20 1 30 7\n", "Ann x 10 20 1 30\n", "Ann 100 10 20 1 30\n",
        "Ann -1 10 20 1 30\n", "Ann 50 10 20 99 30\n", "Ann 50 10 20 -1 30\n", "123 50 10 20 1 30\n",
        "Ann 50 10 20 1 30\nAnn 5 5 5 0 5\n", "Ann 99999999999 10 20 1 30\n", "Ann 50 10 20 1 100\n",
        "Ann 50 10 20 1 30 abc\n", "\x01\x02\x03\n", "Ann\n", "50 10 20 1 30\n", NULL
    };
    roster_t r;
    personnage_t b = nomme("Garde", 9, MAGE);

    roster_init(&r);
    roster_ajouter(&r, &b);
    for (int i = 0; mauvais[i] != NULL; i++) {
        ecrire_fichier_test(F, mauvais[i]);
        V_CMP(charger(&r, F), ==, SAUVEGARDE_ERR_FORMAT);
        V_CMP(r.len, ==, 1);  // « tout ou rien »
        V_TEXTE_EGAL(r.data[0].nom, "Garde");  // « tout ou rien »
    }
    roster_liberer(&r);
    remove(F);
}

static void test_charger_ligne_trop_longue(void)
{
    char grande[400];
    roster_t r;

    memset(grande, 'a', sizeof(grande) - 2);
    grande[sizeof(grande) - 2] = '\n';
    grande[sizeof(grande) - 1] = '\0';
    ecrire_fichier_test(F, grande);
    roster_init(&r);
    V_CMP(charger(&r, F), ==, SAUVEGARDE_ERR_FORMAT);
    V_CMP(r.len, ==, 0);
    remove(F);
}

static void test_charger_fichier_absent(void)
{
    roster_t r;

    roster_init(&r);
    V_CMP(charger(&r, "n_existe_pas.txt"), ==, SAUVEGARDE_ERR_OUVERTURE);
    V_CMP(charger(&r, "/"), !=, SAUVEGARDE_OK);  // un dossier n'est pas une sauvegarde
}

static void test_charger_migration_ancien_format(void)
{
    roster_t r;

    roster_init(&r);
    ecrire_fichier_test(F, "Vieux 20 10 10 22\n");      // sans soin : PRETRE
    V_CMP(charger(&r, F), ==, SAUVEGARDE_OK);
    V_CMP(r.len, ==, 1);
    V_CMP(r.data[0].soin, ==, stat_finale(PRETRE, STAT_SOIN, SOIN_MIGRATION));
    roster_liberer(&r);
    remove(F);
}

static void test_aller_retour_toutes_les_classes(void)
{
    roster_t r;
    roster_t relu;
    personnage_t p = nomme("X", 10, GUERRIER);
    char nom[16];

    roster_init(&r);
    for (int c = 0; c < NB_CLASSES; c++) {
        snprintf(nom, sizeof(nom), "N%d", c);
        p = nomme(nom, 1 + c, (classe_t)c);
        roster_ajouter(&r, &p);
    }
    V_CMP(sauvegarder(&r, F), ==, SAUVEGARDE_OK);
    roster_init(&relu);
    V_CMP(charger(&relu, F), ==, SAUVEGARDE_OK);
    V_CMP(relu.len, ==, NB_CLASSES);
    for (int c = 0; c < NB_CLASSES; c++)
        V_BOOL(memcmp(&r.data[c], &relu.data[c], sizeof(personnage_t)) == 0 || ( strcmp(r.data[c].nom, relu.data[c].nom) == 0 && r.data[c].classe == relu.data[c].classe && r.data[c].pv == relu.data[c].pv && r.data[c].soin == relu.data[c].soin));
    roster_liberer(&r);
    roster_liberer(&relu);
    remove(F);
}

static void test_fusionner_cas(void)
{
    roster_t r;
    personnage_t moi = nomme("Ann", 77, MAGE);
    int ajoutes = -1;
    int ignores = -1;

    roster_init(&r);
    roster_ajouter(&r, &moi);
    ecrire_fichier_test(F, "Ann 1 1 1 0 1\nBob 5 6 7 0 8\nCat 2 2 2 0 2\n");
    V_CMP(fusionner(&r, F, &ajoutes, &ignores), ==, SAUVEGARDE_OK);
    V_CMP(ajoutes, ==, 2);
    V_CMP(ignores, ==, 1);
    V_CMP(r.len, ==, 3);
    V_CMP(r.data[0].pv, ==, 77);  // celui du joueur est gardé
    V_TEXTE_EGAL(r.data[1].nom, "Bob");
    V_TEXTE_EGAL(r.data[2].nom, "Cat");
    V_CMP(fusionner(&r, F, &ajoutes, &ignores), ==, SAUVEGARDE_OK);  // deuxième fois : tout existe
    V_CMP(ajoutes, ==, 0);
    V_CMP(ignores, ==, 3);
    V_CMP(r.len, ==, 3);
    roster_liberer(&r);
    remove(F);
}

static void test_fusionner_invalide_ne_change_rien(void)
{
    roster_t r;
    personnage_t moi = nomme("Ann", 77, MAGE);
    int ajoutes = 5;
    int ignores = 5;

    roster_init(&r);
    roster_ajouter(&r, &moi);
    ecrire_fichier_test(F, "Bob 5 6 7 0 8\nMauvais ligne\n");
    V_CMP(fusionner(&r, F, &ajoutes, &ignores), ==, SAUVEGARDE_ERR_FORMAT);
    V_CMP(r.len, ==, 1);
    V_CMP(ajoutes, ==, 0);
    V_CMP(ignores, ==, 0);
    V_CMP(fusionner(&r, "absent.txt", &ajoutes, &ignores), ==, SAUVEGARDE_ERR_OUVERTURE);
    roster_liberer(&r);
    remove(F);
}

static void test_sauvegarde_message(void)
{
    V_TEXTE_EGAL(sauvegarde_message(SAUVEGARDE_OK), "Terminé");
    V_VRAI(contient(sauvegarde_message(SAUVEGARDE_ERR_OUVERTURE), "ouvrir"));
    V_VRAI(contient(sauvegarde_message(SAUVEGARDE_ERR_ECRITURE), "écriture"));
    V_VRAI(contient(sauvegarde_message(SAUVEGARDE_ERR_FORMAT), "corrompu"));
    V_VRAI(contient(sauvegarde_message(SAUVEGARDE_ERR_MEMOIRE), "Mémoire"));
    for (int i = 0; i < 5; i++)
        for (int j = i + 1; j < 5; j++)
            V_TEXTE_DIFF(sauvegarde_message((sauvegarde_t)i), sauvegarde_message((sauvegarde_t)j));
    V_CMP_BLOQUANT(sauvegarde_message((sauvegarde_t)42), !=, NULL);
}

void tests_sauvegarde(void)
{
    LANCER(test_sauvegarder_format);
    LANCER(test_sauvegarder_sans_fichier_temporaire);
    LANCER(test_sauvegarder_garde_l_ancien_en_cas_d_echec);
    LANCER(test_charger_cas_valides);
    LANCER(test_charger_refus);
    LANCER(test_charger_ligne_trop_longue);
    LANCER(test_charger_fichier_absent);
    LANCER(test_charger_migration_ancien_format);
    LANCER(test_aller_retour_toutes_les_classes);
    LANCER(test_fusionner_cas);
    LANCER(test_fusionner_invalide_ne_change_rien);
    LANCER(test_sauvegarde_message);
}
