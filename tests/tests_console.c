/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_console.c ────────────────────────────────────────────────────
** │ But         Tester les saisies de la console avec un vrai stdin : le
** │             clavier est remplacé par un fichier, et l'affichage est
** │             muet pendant le test.
** │ Contenu     entree, muet_debut / muet_fin, remplir, dossier jetable,
** │             un test_* par comportement
** │ Vérifie     q annule à chaque étape, rien n'est enregistré, les noms
** │             refusés sont redemandés, Ctrl+D est distinct de q, les
** │             options du menu sans sens sont refusées, charger demande
** │             confirmation avant d'écraser, fusionner n'écrase rien
** ╰──────────────────────────────────────────────────────────────────────
*/

#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "composition_console.h"
#include "liste_console.h"
#include "menu_personnages.h"
#include "personnage.h"
#include "roster.h"
#include "saisie.h"
#include "sauvegarde.h"
#include "lanceur.h"
#include "tests_console.h"

#define FICHIER_ENTREE "tests_entree_tmp.txt"
#define DOSSIER_TEST "tests_dossier_tmp"

/* Le prochain scanf / fgets lira `texte` ; un texte vide simule Ctrl+D. */
static void entree(const char *texte)
{
    FILE *f = fopen(FICHIER_ENTREE, "w");

    V_CMP_BLOQUANT(f, !=, NULL);
    fputs(texte, f);
    fclose(f);
    V_CMP_BLOQUANT(freopen(FICHIER_ENTREE, "r", stdin), !=, NULL);
}

static int muet_debut(void)
{
    int sauve = 0;
    int nul = open("/dev/null", O_WRONLY);

    fflush(stdout);
    sauve = dup(1);
    dup2(nul, 1);
    close(nul);
    return sauve;
}

static void muet_fin(int sauve)
{
    fflush(stdout);
    dup2(sauve, 1);
    close(sauve);
}

static void test_lire_entier(void)
{
    int v = -1;
    int m = muet_debut();

    entree("q\n");
    V_CMP(lire_entier("x", 0, 99, &v), ==, SAISIE_ANNULEE);
    V_CMP(v, ==, -1);
    entree("abc\n150\n-3\n12\n");
    V_CMP(lire_entier("x", 0, 99, &v), ==, 0);  // refusés, puis accepté
    V_CMP(v, ==, 12);  // refusés, puis accepté
    entree("");
    V_CMP(lire_entier("x", 0, 99, &v), ==, SAISIE_FIN);  // Ctrl+D n'est pas q
    entree("q\n3\n");
    V_CMP(lire_entier_menu("x", 0, 5, &v), ==, 0);  // q refusé dans un menu
    V_CMP(v, ==, 3);  // q refusé dans un menu
    muet_fin(m);
}

static void test_lire_nom(void)
{
    char nom[NOM_MAX] = "";
    char trop_long[200];
    char saisie[260];
    int m = muet_debut();

    entree("q\n");
    V_CMP(lire_nom("Nom", nom, sizeof(nom)), ==, SAISIE_ANNULEE);
    entree("\n\nAr thur\nArthur\n");
    V_CMP(lire_nom("Nom", nom, sizeof(nom)), ==, 0);
    V_TEXTE_EGAL(nom, "Arthur");
    entree("a23456789012345678901234567890\nLong\n");        // 30 caractères : refusé
    V_CMP(lire_nom("Nom", nom, sizeof(nom)), ==, 0);
    V_TEXTE_EGAL(nom, "Long");
    entree("a2345678901234567890123456789\n");               // 29 : accepté tel quel
    V_CMP(lire_nom("Nom", nom, sizeof(nom)), ==, 0);
    V_CMP(strlen(nom), ==, 29);
    memset(trop_long, 'x', sizeof(trop_long) - 1);
    trop_long[sizeof(trop_long) - 1] = '\0';
    snprintf(saisie, sizeof(saisie), "%s\nbon\n", trop_long);
    entree(saisie);                                           // ligne énorme : pas de débordement
    V_CMP(lire_nom("Nom", nom, sizeof(nom)), ==, 0);
    V_TEXTE_EGAL(nom, "bon");
    entree("123\n007\n42\nAda2\n");                          // que des chiffres : refusé (ce serait un ID)
    V_CMP(lire_nom("Nom", nom, sizeof(nom)), ==, 0);
    V_TEXTE_EGAL(nom, "Ada2");
    entree("");
    V_CMP(lire_nom("Nom", nom, sizeof(nom)), ==, SAISIE_FIN);
    V_CMP(lire_nom("Nom", nom, 5), ==, SAISIE_FIN);  // tampon trop petit : refusé
    muet_fin(m);
}

static void test_confirmer(void)
{
    int m = muet_debut();

    entree("o\n");
    V_CMP(confirmer("?"), ==, 1);
    entree("OUI\n");
    V_CMP(confirmer("?"), ==, 1);
    entree("n\n");
    V_CMP(confirmer("?"), ==, 0);
    entree("\n");
    V_CMP(confirmer("?"), ==, 0);  // Entrée seule : non, le choix sûr
    entree("q\n");
    V_CMP(confirmer("?"), ==, 0);
    entree("peut-être\n42\no\n");
    V_CMP(confirmer("?"), ==, 1);  // réponse bizarre : on redemande
    entree("");
    V_CMP(confirmer("?"), ==, 0);  // Ctrl+D : non
    muet_fin(m);
}

static void test_creation_annulable(void)
{
    personnage_t tab[1] = {{"Zed", 10, 10, 10, GUERRIER, 10}};
    personnage_t p = {0};
    int m = muet_debut();

    entree("q\n");
    V_CMP(creer_personnage(&p, tab, 1), ==, SAISIE_ANNULEE);  // au nom
    entree("Ada\nq\n");
    V_CMP(creer_personnage(&p, tab, 1), ==, SAISIE_ANNULEE);  // à la 1re statistique
    entree("Ada\n50\n40\nq\n");
    V_CMP(creer_personnage(&p, tab, 1), ==, SAISIE_ANNULEE);  // à la 3e
    entree("Ada\n50\n40\n30\n20\nq\n");
    V_CMP(creer_personnage(&p, tab, 1), ==, SAISIE_ANNULEE);  // au choix de classe
    entree("Ada\n50\n40\n30\n");
    V_CMP(creer_personnage(&p, tab, 1), ==, SAISIE_FIN);  // Ctrl+D en cours de route
    entree("Zed\nAda\n50\n40\n30\n20\n1\n");
    memset(&p, 0, sizeof(p));
    V_CMP(creer_personnage(&p, tab, 1), ==, 0);  // « Zed » refusé, « Ada » créé
    V_TEXTE_EGAL(p.nom, "Ada");
    V_CMP(p.pv, >=, STAT_MIN);
    V_CMP(p.pv, <=, STAT_MAX);
    muet_fin(m);
}

static void test_composition_annulable(void)
{
    roster_t r;
    personnage_t equipe[TAILLE_EQUIPE_MAX];
    personnage_t a = {"Ada", 50, 40, 20, GUERRIER, 10};
    personnage_t b = {"Bob", 50, 40, 20, MAGE, 10};
    int m = muet_debut();

    roster_init(&r);
    V_CMP(roster_ajouter(&r, &a), ==, 0);
    V_CMP(roster_ajouter(&r, &b), ==, 0);
    entree("q\n");
    V_CMP(choisir_equipe(&r, "Équipe", equipe, NULL, 0), ==, SAISIE_ANNULEE);
    entree("Ada\nq\n");
    V_CMP(choisir_equipe(&r, "Équipe", equipe, NULL, 0), ==, SAISIE_ANNULEE);  // même après un choix
    entree("Ada\n\n");
    V_CMP(choisir_equipe(&r, "Équipe", equipe, NULL, 0), ==, 1);
    entree("");
    V_CMP(choisir_equipe(&r, "Équipe", equipe, NULL, 0), ==, SAISIE_FIN);
    V_CMP(r.len, ==, 2);  // le roster est intact
    roster_liberer(&r);
    muet_fin(m);
}

/* Un roster de n personnages « P0 », « P1 »... ; ceux d'indice impair sont K.O. si ko vaut 1. */
static void remplir(roster_t *r, int n, int ko)
{
    personnage_t p = {"", 50, 40, 20, GUERRIER, 10};

    roster_init(r);
    for (int i = 0; i < n; i++) {
        snprintf(p.nom, sizeof(p.nom), "P%d", i);
        p.pv = ko && i % 2 == 1 ? 0 : 50;
        V_CMP(roster_ajouter(r, &p), ==, 0);
    }
}

static void test_roster_trouver(void)
{
    roster_t r;
    personnage_t p = {"2", 50, 40, 20, MAGE, 10};

    remplir(&r, 3, 0);
    V_CMP(roster_trouver(&r, "P1"), ==, 1);  // par le nom
    V_CMP(roster_trouver(&r, "1"), ==, 0);  // par l'ID (1 = premier)
    V_CMP(roster_trouver(&r, "3"), ==, 2);
    V_CMP(roster_trouver(&r, "0"), ==, -1);
    V_CMP(roster_trouver(&r, "4"), ==, -1);
    V_CMP(roster_trouver(&r, "-1"), ==, -1);
    V_CMP(roster_trouver(&r, ""), ==, -1);
    V_CMP(roster_trouver(&r, "p1"), ==, -1);  // sensible à la casse, comme le reste
    V_CMP(roster_trouver(&r, "99999999999999999999"), ==, -1);  // pas de dépassement d'int
    V_CMP(roster_trouver(&r, "1x"), ==, -1);
    V_CMP(roster_ajouter(&r, &p), ==, 0);
    V_CMP(roster_trouver(&r, "2"), ==, 3);  // un nom exact passe avant l'ID (cas défensif)
    roster_liberer(&r);
    V_CMP(roster_trouver(&r, "1"), ==, -1);  // roster vide
}

static void test_menu_indisponible(void)
{
    roster_t r;

    remplir(&r, 0, 0);
    V_CMP_BLOQUANT(menu_indisponible(&r, CHOIX_AFFICHER), !=, NULL);
    V_CMP_BLOQUANT(menu_indisponible(&r, CHOIX_COMBAT), !=, NULL);
    V_CMP_BLOQUANT(menu_indisponible(&r, CHOIX_SAUVEGARDER), !=, NULL);  // pas de sauvegarde vide
    V_CMP(menu_indisponible(&r, CHOIX_AJOUTER), ==, NULL);
    V_CMP(menu_indisponible(&r, CHOIX_CHARGER), ==, NULL);
    V_CMP(menu_indisponible(&r, CHOIX_FUSIONNER), ==, NULL);
    roster_liberer(&r);
    remplir(&r, 1, 0);
    V_CMP(menu_indisponible(&r, CHOIX_AFFICHER), ==, NULL);
    V_CMP(menu_indisponible(&r, CHOIX_SAUVEGARDER), ==, NULL);
    V_CMP_BLOQUANT(menu_indisponible(&r, CHOIX_COMBAT), !=, NULL);  // 1 seul : pas de combat
    roster_liberer(&r);
    remplir(&r, 4, 1);                                           // 2 en forme, 2 K.O.
    V_CMP(roster_nb_en_forme(&r), ==, 2);
    V_CMP(menu_indisponible(&r, CHOIX_COMBAT), ==, NULL);
    r.data[0].pv = 0;
    V_CMP(roster_nb_en_forme(&r), ==, 1);
    V_CMP_BLOQUANT(menu_indisponible(&r, CHOIX_COMBAT), !=, NULL);
    roster_liberer(&r);
}

static void test_composition_par_numero(void)
{
    roster_t r;
    personnage_t equipe[TAILLE_EQUIPE_MAX];
    int m = muet_debut();

    remplir(&r, 4, 1);                                           // P1 et P3 sont K.O.
    entree("1\n2\n3\n\n");                                     // P0, puis P1 (K.O. : refusé), P2
    V_CMP(choisir_equipe(&r, "Équipe", equipe, NULL, 0), ==, 2);
    V_TEXTE_EGAL(equipe[0].nom, "P0");
    V_TEXTE_EGAL(equipe[1].nom, "P2");
    entree("P0\n1\n9\nP2\n");                                 // doublon par numéro, hors liste
    V_CMP(choisir_equipe(&r, "Équipe", equipe, NULL, 0), ==, 2);
    entree("1\n3\n");                                            // P0 est déjà engagé chez l'autre équipe
    V_CMP(choisir_equipe(&r, "Équipe", equipe, &r.data[0], 1), ==, 1);
    V_TEXTE_EGAL(equipe[0].nom, "P2");
    muet_fin(m);
    roster_liberer(&r);
}

static void ecrire_fichier_perso(const char *contenu)
{
    FILE *f = fopen(FICHIER_SAUVEGARDE, "w");

    V_CMP_BLOQUANT(f, !=, NULL);
    fputs(contenu, f);
    fclose(f);
}

/* Ces tests écrivent « personnages.txt » : ils le font dans un dossier jetable. */
static void entrer_dossier_jetable(void)
{
    remove(DOSSIER_TEST "/" FICHIER_SAUVEGARDE);    // reste d'une exécution interrompue
    remove(DOSSIER_TEST "/" FICHIER_ENTREE);
    rmdir(DOSSIER_TEST);
    V_CMP(mkdir(DOSSIER_TEST, 0700), ==, 0);
    V_CMP(chdir(DOSSIER_TEST), ==, 0);
}

static void sortir_dossier_jetable(void)
{
    remove(FICHIER_SAUVEGARDE);
    remove(FICHIER_ENTREE);                 // celui d'ici : entree() écrit dans le dossier courant
    V_CMP(chdir(".."), ==, 0);
    V_CMP(rmdir(DOSSIER_TEST), ==, 0);
}

static void test_fusionner(void)
{
    roster_t r;
    int ajoutes = -1;
    int ignores = -1;

    entrer_dossier_jetable();
    remplir(&r, 2, 0);                                   // P0 et P1 existent déjà
    ecrire_fichier_perso("P1 99 99 99 0 99\nZed 10 10 10 0 10\nAda 20 20 20 1 20\n");
    V_CMP(fusionner(&r, FICHIER_SAUVEGARDE, &ajoutes, &ignores), ==, SAUVEGARDE_OK);
    V_CMP(ajoutes, ==, 2);
    V_CMP(ignores, ==, 1);
    V_CMP(r.len, ==, 4);
    V_CMP(r.data[1].pv, ==, 50);  // P1 n'a pas été écrasé
    V_TEXTE_EGAL(r.data[2].nom, "Zed");
    V_TEXTE_EGAL(r.data[3].nom, "Ada");
    V_CMP(fusionner(&r, FICHIER_SAUVEGARDE, &ajoutes, &ignores), ==, SAUVEGARDE_OK);
    V_CMP(ajoutes, ==, 0);  // rejouer ne duplique rien
    V_CMP(ignores, ==, 3);  // rejouer ne duplique rien
    V_CMP(r.len, ==, 4);  // rejouer ne duplique rien
    ecrire_fichier_perso("Bob 10 10 10 0 10\nCaro 10 10 10 999 10\n");
    V_CMP(fusionner(&r, FICHIER_SAUVEGARDE, &ajoutes, &ignores), ==, SAUVEGARDE_ERR_FORMAT);
    V_CMP(r.len, ==, 4);  // fichier invalide : rien n'est ajouté, même Bob
    V_CMP(ajoutes, ==, 0);  // fichier invalide : rien n'est ajouté, même Bob
    remove(FICHIER_SAUVEGARDE);
    V_CMP(fusionner(&r, FICHIER_SAUVEGARDE, &ajoutes, &ignores), ==, SAUVEGARDE_ERR_OUVERTURE);
    V_CMP(r.len, ==, 4);
    roster_liberer(&r);
    sortir_dossier_jetable();
}

static void test_charger_confirmation(void)
{
    roster_t r;
    int m = muet_debut();

    entrer_dossier_jetable();
    ecrire_fichier_perso("Zed 10 10 10 0 10\n");
    remplir(&r, 3, 0);
    entree("n\n");
    menu_charger(&r);
    V_CMP(r.len, ==, 3);  // refusé : rien n'est écrasé
    V_TEXTE_EGAL(r.data[0].nom, "P0");  // refusé : rien n'est écrasé
    entree("\n");
    menu_charger(&r);
    V_CMP(r.len, ==, 3);  // Entrée seule = non
    entree("q\n");
    menu_charger(&r);
    V_CMP(r.len, ==, 3);
    entree("");
    menu_charger(&r);
    V_CMP(r.len, ==, 3);  // Ctrl+D = non
    entree("oui\n");
    menu_charger(&r);
    V_CMP(r.len, ==, 1);  // confirmé : remplacé
    V_TEXTE_EGAL(r.data[0].nom, "Zed");  // confirmé : remplacé
    roster_liberer(&r);
    remplir(&r, 0, 0);
    entree("");                                                     // liste vide : aucune question
    menu_charger(&r);
    V_CMP(r.len, ==, 1);
    roster_liberer(&r);
    ecrire_fichier_perso("Zed 10 10 10 0 10\nCaro 10 10 10 999 10\n");
    remplir(&r, 2, 0);
    entree("o\n");
    menu_charger(&r);
    V_CMP(r.len, ==, 2);  // fichier invalide : intact
    V_TEXTE_EGAL(r.data[0].nom, "P0");  // fichier invalide : intact
    roster_liberer(&r);
    sortir_dossier_jetable();
    muet_fin(m);
}

static void test_nom_est_un_id(void)
{
    V_VRAI(nom_est_un_id("3"));
    V_VRAI(nom_est_un_id("007"));
    V_VRAI(nom_est_un_id("1234567890123"));
    V_FAUX(nom_est_un_id(""));
    V_FAUX(nom_est_un_id("Ada"));
    V_FAUX(nom_est_un_id("Ada2"));
    V_FAUX(nom_est_un_id("2Ada"));
    V_FAUX(nom_est_un_id("-3"));
    V_FAUX(nom_est_un_id("3.5"));
}

/* Un nom fait de chiffres est refusé au chargement comme à la fusion : il serait confondu avec un ID. */
static void test_nom_nombre_refuse(void)
{
    roster_t r;
    int ajoutes = 0;
    int ignores = 0;

    entrer_dossier_jetable();
    ecrire_fichier_perso("Ada 10 10 10 0 10\n123 10 10 10 0 10\n");
    roster_init(&r);
    V_CMP(charger(&r, FICHIER_SAUVEGARDE), ==, SAUVEGARDE_ERR_FORMAT);
    V_CMP(r.len, ==, 0);
    V_CMP(fusionner(&r, FICHIER_SAUVEGARDE, &ajoutes, &ignores), ==, SAUVEGARDE_ERR_FORMAT);
    V_CMP(r.len, ==, 0);  // pas même « Ada »
    V_CMP(ajoutes, ==, 0);  // pas même « Ada »
    ecrire_fichier_perso("Ada1 10 10 10 0 10\n");
    V_CMP(charger(&r, FICHIER_SAUVEGARDE), ==, SAUVEGARDE_OK);  // un nom avec un chiffre reste permis
    V_CMP(r.len, ==, 1);  // un nom avec un chiffre reste permis
    roster_liberer(&r);
    sortir_dossier_jetable();
}

/* Retire les codes couleur ANSI (ESC [ ... m) : on compare le texte tel que l'œil le lit. */
static void sans_couleurs(char *texte)
{
    size_t j = 0;

    for (size_t i = 0; texte[i] != '\0'; i++) {
        if (texte[i] == '\033') {
            while (texte[i] != '\0' && texte[i] != 'm')
                i++;
            if (texte[i] == '\0')
                break;
        } else {
            texte[j++] = texte[i];
        }
    }
    texte[j] = '\0';
}

/* La liste montre bien l'ID de chacun, dans l'ordre, sous un en-tête « ID ». */
static void test_liste_montre_les_id(void)
{
    roster_t r;
    char texte[2048] = "";
    int sauve = 0;
    FILE *f = NULL;
    size_t n = 0;

    remplir(&r, 3, 1);                                    // P1 est K.O.
    fflush(stdout);
    sauve = dup(1);
    V_CMP_BLOQUANT(freopen(FICHIER_ENTREE, "w", stdout), !=, NULL);
    afficher_liste(&r, NULL, 0);
    fflush(stdout);
    dup2(sauve, 1);
    close(sauve);
    clearerr(stdout);
    f = fopen(FICHIER_ENTREE, "r");
    V_CMP_BLOQUANT(f, !=, NULL);
    n = fread(texte, 1, sizeof(texte) - 1, f);
    texte[n] = '\0';
    fclose(f);
    sans_couleurs(texte);
    V_CMP_BLOQUANT(strstr(texte, "ID  Nom"), !=, NULL);
    V_CMP_BLOQUANT(strstr(texte, "Personnages (3)"), !=, NULL);
    V_CMP_BLOQUANT(strstr(texte, " 1  P0"), !=, NULL);  // l'ID, puis le nom
    V_CMP_BLOQUANT(strstr(texte, " 2  P1"), !=, NULL);
    V_CMP_BLOQUANT(strstr(texte, " 3  P2"), !=, NULL);
    V_CMP_BLOQUANT(strstr(texte, "K.O."), !=, NULL);
    roster_liberer(&r);
}

/* La base livrée dans exemples/ se charge sans erreur et couvre bien les 4 catégories. */
static void test_base_exemple(void)
{
    roster_t r;
    int vues[NB_CATEGORIES] = {0};

    roster_init(&r);
    V_CMP(charger(&r, "exemples/personnages_base.txt"), ==, SAUVEGARDE_OK);
    V_CMP(r.len, ==, 8);
    for (int i = 0; i < r.len; i++) {
        V_FAUX(est_ko(&r.data[i]));
        vues[classes[r.data[i].classe].categorie]++;
    }
    for (int c = 0; c < NB_CATEGORIES; c++)
        V_CMP(vues[c], ==, 2);
    roster_liberer(&r);
}

void tests_console(void)
{
    LANCER(test_lire_entier);
    LANCER(test_lire_nom);
    LANCER(test_confirmer);
    LANCER(test_creation_annulable);
    LANCER(test_composition_annulable);
    LANCER(test_roster_trouver);
    LANCER(test_menu_indisponible);
    LANCER(test_composition_par_numero);
    LANCER(test_fusionner);
    LANCER(test_charger_confirmation);
    LANCER(test_base_exemple);
    LANCER(test_nom_est_un_id);
    LANCER(test_nom_nombre_refuse);
    LANCER(test_liste_montre_les_id);
    remove(FICHIER_ENTREE);
}
