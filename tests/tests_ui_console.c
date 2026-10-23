/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_ui_console.c ─────────────────────────────────────────────────
** │ But         Un ou plusieurs tests par fonction de l'interface
** │             console : l'affichage est capturé et comparé, le clavier
** │             est simulé. Chaque test tourne dans son propre processus
** │             et travaille dans un dossier jetable.
** ╰──────────────────────────────────────────────────────────────────────
*/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "bataille_console.h"
#include "boite.h"
#include "composition_console.h"
#include "couleurs.h"
#include "lanceur.h"
#include "liste_console.h"
#include "menu_personnages.h"
#include "my_string.h"
#include "outils_tests.h"
#include "redstrings.h"
#include "roster.h"
#include "saisie.h"
#include "sauvegarde.h"
#include "tests_ui_console.h"

/* Dossier de travail neuf : personnages.txt n'est jamais celui du vrai jeu. */
static void dossier_jetable(const char *nom)
{
    char chemin[128];

    mkdir("/tmp/redstrings_tests", 0700);
    snprintf(chemin, sizeof(chemin), "/tmp/redstrings_tests/%s", nom);
    mkdir(chemin, 0700);
    V_CMP(chdir(chemin), ==, 0);
}

static personnage_t perso(const char *nom, int pv, classe_t c)
{
    personnage_t p = {0};

    snprintf(p.nom, sizeof(p.nom), "%s", nom);
    p.pv = pv;
    p.atq = 30;
    p.def = 10;
    p.soin = 10;
    p.classe = c;
    return p;
}

static void remplir(roster_t *r, int n)
{
    char nom[16];
    personnage_t p;

    roster_init(r);
    for (int i = 0; i < n; i++) {
        snprintf(nom, sizeof(nom), "P%d", i + 1);
        p = perso(nom, 50 + i, (classe_t)(i % NB_CLASSES));
        V_CMP(roster_ajouter(r, &p), ==, 0);
    }
}

/* ---------------------------- boîtes ------------------------------- */

static void test_largeur_visible_cas(void)
{
    V_CMP(largeur_visible(""), ==, 0);
    V_CMP(largeur_visible("abc"), ==, 3);
    V_CMP(largeur_visible("é"), ==, 1);  // UTF-8 : colonnes, pas octets
    V_CMP(largeur_visible("█░"), ==, 2);  // UTF-8 : colonnes, pas octets
    V_CMP(largeur_visible(ROUGE "abc" RESET), ==, 3);  // codes couleur ignorés
    V_CMP(largeur_visible("\033[38;2;1;2;3mx\033[0m"), ==, 1);
    V_CMP(largeur_visible("a\033[31"), ==, 1);  // code non terminé : ignoré
}

static void test_boite_haut_separateur_bas(void)
{
    char s[512];

    capture_debut();
    boite_haut(CYAN);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "╭"));
    V_VRAI(contient(s, "╮"));
    V_VRAI(contient(s, LIGNE));
    V_VRAI(contient(s, CYAN));
    capture_debut();
    boite_separateur(ROUGE);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "├"));
    V_VRAI(contient(s, "┤"));
    V_VRAI(contient(s, ROUGE));
    capture_debut();
    boite_bas("");
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "╰"));
    V_VRAI(contient(s, "╯"));
    V_CMP(largeur_visible(LIGNE), ==, LARGEUR_BOITE);  // la constante colle à la largeur
}

static void test_boite_ligne_alignement(void)
{
    char s[512];

    capture_debut();
    boite_ligne("", "  bonjour %d", 42);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "bonjour 42"));
    V_CMP(largeur_visible(s), ==, LARGEUR_BOITE + 2 + 1);  // 2 bordures + '\n' (compté 1)
    capture_debut();
    boite_ligne(VERT, "  " GRAS "gras" RESET " é█");
    capture_fin(s, sizeof(s));
    V_CMP(largeur_visible(s), ==, LARGEUR_BOITE + 2 + 1);  // couleurs et UTF-8 ne décalent rien
    capture_debut();
    boite_ligne("", "%s", "très très très très très très très long");     // plus large que la boîte : pas de coupure
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "très long"));
}

static void test_aligner_texte_cas(void)
{
    char d[64];

    aligner_texte(d, sizeof(d), "ab", 5);
    V_TEXTE_EGAL(d, "ab   ");
    aligner_texte(d, sizeof(d), "abcdef", 3);
    V_TEXTE_EGAL(d, "abcdef");  // jamais coupé
    aligner_texte(d, sizeof(d), "é█", 4);
    V_TEXTE_EGAL(d, "é█  ");  // aligné en colonnes
    aligner_texte(d, sizeof(d), ROUGE "x" RESET, 3);
    V_CMP(largeur_visible(d), ==, 3);
    aligner_texte(d, 4, "abcdef", 10);
    V_TEXTE_EGAL(d, "abc");  // borné par la taille du tampon
}

static void test_boite_champ_cas(void)
{
    char s[512];

    capture_debut();
    boite_champ("", "Classe", "%s", "Mage");
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "Classe"));
    V_VRAI(contient(s, "Mage"));
    V_CMP(largeur_visible(s), ==, LARGEUR_BOITE + 2 + 1);
    V_CMP(strstr(s, "Classe"), <, strstr(s, "Mage"));
}

/* ---------------------------- saisies ------------------------------ */

static void test_afficher_erreur_cas(void)
{
    char s[256];

    capture_debut();
    afficher_erreur("Oups");
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "✗"));
    V_VRAI(contient(s, "Oups"));
    V_VRAI(contient(s, ROUGE));
}

static void test_vider_buffer_cas(void)
{
    char buf[16];

    entree_simulee("jeté\nconservé\n");
    vider_buffer();                                                        // jette jusqu'à la fin de ligne
    V_CMP(lire_ligne(buf, sizeof(buf)), ==, 0);
    V_TEXTE_EGAL(buf, "conservé");
    entree_simulee("");
    vider_buffer();                                                        // entrée fermée : ne boucle pas
}

static void test_lire_ligne_cas(void)
{
    char buf[8];

    entree_simulee("abc\n");
    V_CMP(lire_ligne(buf, sizeof(buf)), ==, 0);
    V_TEXTE_EGAL(buf, "abc");
    entree_simulee("\n");
    V_CMP(lire_ligne(buf, sizeof(buf)), ==, 0);
    V_CMP(buf[0], ==, '\0');
    entree_simulee("1234567890\nsuite\n");                                 // trop long : tronqué, fin jetée
    V_CMP(lire_ligne(buf, sizeof(buf)), ==, 0);
    V_TEXTE_EGAL(buf, "1234567");
    V_CMP(lire_ligne(buf, sizeof(buf)), ==, 0);
    V_TEXTE_EGAL(buf, "suite");
    entree_simulee("sans fin");                                            // dernière ligne sans '\n'
    V_CMP(lire_ligne(buf, sizeof(buf)), ==, 0);
    V_TEXTE_EGAL(buf, "sans fi");
    entree_simulee("");
    V_CMP(lire_ligne(buf, sizeof(buf)), ==, SAISIE_FIN);
    V_CMP(buf[0], ==, '\0');
    V_CMP(lire_ligne(buf, 1), ==, SAISIE_FIN);  // tampon trop petit
}

static void test_lire_entier_bornes(void)
{
    int v = 0;
    int m = muet_commencer();

    entree_simulee("5\n");
    V_CMP(lire_entier("?", 5, 5, &v), ==, 0);  // min == max
    V_CMP(v, ==, 5);  // min == max
    entree_simulee("-3\n");
    V_CMP(lire_entier("?", -5, 5, &v), ==, 0);
    V_CMP(v, ==, -3);
    entree_simulee("2147483648\n7\n");                                     // dépassement d'int : refusé
    V_CMP(lire_entier("?", 0, 100, &v), ==, 0);
    V_CMP(v, ==, 7);
    entree_simulee(" 4\n");                                                // strtol accepte l'espace de tête
    V_CMP(lire_entier("?", 0, 9, &v), ==, 0);
    V_CMP(v, ==, 4);
    entree_simulee("4 \n9\n");                                             // texte après le nombre : refusé
    V_CMP(lire_entier("?", 0, 9, &v), ==, 0);
    V_CMP(v, ==, 9);
    V_CMP(lire_entier("?", 9, 0, &v), ==, SAISIE_FIN);  // bornes inversées
    muet_finir(m);
}

static void test_confirmer_reponses(void)
{
    static const char *oui[] = {"o\n", "O\n", "oui\n", "OUI\n", "Oui\n", NULL};
    static const char *non[] = {"n\n", "non\n", "\n", "q\n", "N\n", NULL};
    int m = muet_commencer();

    for (int i = 0; oui[i] != NULL; i++) {
        entree_simulee(oui[i]);
        V_CMP(confirmer("?"), ==, 1);
    }
    for (int i = 0; non[i] != NULL; i++) {
        entree_simulee(non[i]);
        V_CMP(confirmer("?"), ==, 0);
    }
    entree_simulee("peut-être\nx\no\n");                                   // réponses inconnues redemandées
    V_CMP(confirmer("?"), ==, 1);
    entree_simulee("");
    V_CMP(confirmer("?"), ==, 0);  // Ctrl+D : non
    muet_finir(m);
}

/* ---------------------------- liste -------------------------------- */

static void test_afficher_liste_contenu(void)
{
    roster_t r;
    char s[4096];

    remplir(&r, 3);
    r.data[1].pv = 0;
    capture_debut();
    afficher_liste(&r, NULL, 0);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "Personnages (3)"));
    V_VRAI(contient(s, "ID"));
    V_VRAI(contient(s, "P1"));
    V_VRAI(contient(s, "P2"));
    V_VRAI(contient(s, "P3"));
    V_VRAI(contient(s, "K.O."));  // P2 est K.O.
    V_CMP(strstr(s, "P1"), <, strstr(s, "P2"));
    V_CMP(strstr(s, "P2"), <, strstr(s, "P3"));
    roster_liberer(&r);
}

static void test_afficher_liste_groupes(void)
{
    roster_t r;
    groupe_liste_t g = {NULL, 1, "✔ ton équipe", CYAN};
    char s[4096];

    remplir(&r, 2);
    g.membres = &r.data[1];
    capture_debut();
    afficher_liste(&r, &g, 1);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "✔ ton équipe"));
    V_CMP(strstr(s, "✔ ton équipe"), >, strstr(s, "P2"));  // sur la ligne du 2e seulement
    V_BOOL(strstr(s, "✔ ton équipe") == strrchr(s, '\xE2') - 0 || contient(s, "ton équipe"));
    roster_liberer(&r);
}

static void test_afficher_liste_vide(void)
{
    roster_t r;
    char s[1024];

    roster_init(&r);
    capture_debut();
    afficher_liste(&r, NULL, 0);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "Personnages (0)"));
    V_FAUX(contient(s, "P1"));
}

/* ---------------------------- menus -------------------------------- */

static void test_menu_indisponible_raisons(void)
{
    roster_t r;
    personnage_t p = perso("A", 10, GUERRIER);

    roster_init(&r);
    V_CMP(menu_indisponible(&r, CHOIX_AJOUTER), ==, NULL);
    V_CMP(menu_indisponible(&r, CHOIX_QUITTER), ==, NULL);
    V_CMP(menu_indisponible(&r, CHOIX_CHARGER), ==, NULL);
    V_CMP_BLOQUANT(menu_indisponible(&r, CHOIX_AFFICHER), !=, NULL);
    V_CMP_BLOQUANT(menu_indisponible(&r, CHOIX_SAUVEGARDER), !=, NULL);
    V_CMP_BLOQUANT(menu_indisponible(&r, CHOIX_COMBAT), !=, NULL);
    roster_ajouter(&r, &p);
    V_CMP(menu_indisponible(&r, CHOIX_AFFICHER), ==, NULL);
    V_CMP(menu_indisponible(&r, CHOIX_SAUVEGARDER), ==, NULL);
    V_CMP_BLOQUANT(menu_indisponible(&r, CHOIX_COMBAT), !=, NULL);  // il faut 2 personnages en forme
    roster_ajouter(&r, &p);
    V_CMP(menu_indisponible(&r, CHOIX_COMBAT), ==, NULL);
    r.data[0].pv = 0;
    V_CMP_BLOQUANT(menu_indisponible(&r, CHOIX_COMBAT), !=, NULL);  // un K.O. ne compte pas
    roster_liberer(&r);
}

static void test_menu_afficher_cas(void)
{
    roster_t r;
    char s[8192];
    int m = 0;

    remplir(&r, 3);
    m = muet_commencer();
    muet_finir(m);
    entree_simulee("P2\n");
    capture_debut();
    menu_afficher(&r);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "Personnages (3)"));
    V_VRAI(contient(s, "Quel personnage"));
    V_VRAI(contient(s, "╭"));  // la fiche de P2 est affichée
    entree_simulee("3\n");                                                 // par ID
    capture_debut();
    menu_afficher(&r);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "P3"));
    entree_simulee("inconnu\nq\n");                                        // redemande, puis annule
    capture_debut();
    menu_afficher(&r);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "introuvable"));
    V_VRAI(contient(s, "Annulé"));
    roster_liberer(&r);
}

static void test_menu_ajouter_cas(void)
{
    roster_t r;
    int m = muet_commencer();

    roster_init(&r);
    entree_simulee("Neo\n50\n40\n30\n20\n1\n");
    menu_ajouter(&r);
    V_CMP(r.len, ==, 1);
    V_TEXTE_EGAL(r.data[0].nom, "Neo");
    entree_simulee("Neo\nq\n");                                            // doublon refusé, puis annulation
    menu_ajouter(&r);
    V_CMP(r.len, ==, 1);
    entree_simulee("Trin\n10\n10\n10\n10\n2\n");
    menu_ajouter(&r);
    V_CMP(r.len, ==, 2);
    V_TEXTE_EGAL(r.data[1].nom, "Trin");
    muet_finir(m);
    roster_liberer(&r);
}

static void test_menu_sauvegarder_et_charger(void)
{
    roster_t r;
    roster_t autre;
    char s[1024];

    dossier_jetable("sauv");
    remplir(&r, 3);
    capture_debut();
    menu_sauvegarder(&r);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "3 personnage"));
    V_VRAI(contient(s, FICHIER_SAUVEGARDE));
    roster_init(&autre);
    capture_debut();
    menu_charger(&autre);                                                  // liste vide : pas de confirmation
    capture_fin(s, sizeof(s));
    V_CMP(autre.len, ==, 3);
    V_VRAI(contient(s, "3 personnage"));
    V_TEXTE_EGAL(autre.data[2].nom, "P3");
    entree_simulee("n\n");
    capture_debut();
    menu_charger(&autre);                                                  // liste non vide : demande
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "Attention"));
    V_VRAI(contient(s, "annulé"));
    V_CMP(autre.len, ==, 3);
    entree_simulee("o\n");
    capture_debut();
    menu_charger(&autre);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "chargé"));
    V_CMP(autre.len, ==, 3);
    roster_liberer(&r);
    roster_liberer(&autre);
    remove(FICHIER_SAUVEGARDE);
}

static void test_menu_charger_fichier_absent(void)
{
    roster_t r;
    char s[1024];

    dossier_jetable("abs");
    roster_init(&r);
    capture_debut();
    menu_charger(&r);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "✗"));
    V_VRAI(contient(s, "ouvrir"));
    V_CMP(r.len, ==, 0);
}

static void test_menu_fusionner_cas(void)
{
    roster_t r;
    roster_t base;
    char s[2048];

    dossier_jetable("fus");
    remplir(&base, 2);
    V_CMP(sauvegarder(&base, FICHIER_SAUVEGARDE), ==, SAUVEGARDE_OK);
    remplir(&r, 1);                                                        // contient déjà P1
    capture_debut();
    menu_fusionner(&r);
    capture_fin(s, sizeof(s));
    V_CMP(r.len, ==, 2);
    V_VRAI(contient(s, "1 personnage ajouté"));
    V_VRAI(contient(s, "1 ignoré"));
    capture_debut();
    menu_fusionner(&r);
    capture_fin(s, sizeof(s));
    V_CMP(r.len, ==, 2);
    V_VRAI(contient(s, "Aucun personnage ajouté"));
    roster_liberer(&r);
    roster_liberer(&base);
    remove(FICHIER_SAUVEGARDE);
}

/* ---------------------------- combat ------------------------------- */

static void test_choisir_equipe_cas(void)
{
    roster_t r;
    personnage_t eq[TAILLE_EQUIPE_MAX];
    int m = muet_commencer();

    remplir(&r, 6);
    entree_simulee("P1\n3\nP5\n\n");                                       // nom, ID, nom, ligne vide
    V_CMP(choisir_equipe(&r, "Équipe", eq, NULL, 0), ==, 3);
    V_TEXTE_EGAL(eq[0].nom, "P1");
    V_TEXTE_EGAL(eq[1].nom, "P3");
    V_TEXTE_EGAL(eq[2].nom, "P5");
    entree_simulee("P1\nP1\nP9\nP2\n\n");                                  // doublon et inconnu refusés
    V_CMP(choisir_equipe(&r, "Équipe", eq, NULL, 0), ==, 2);
    entree_simulee("q\n");
    V_CMP(choisir_equipe(&r, "Équipe", eq, NULL, 0), ==, SAISIE_ANNULEE);
    entree_simulee("P1\n");                                                // Ctrl+D en cours de route
    V_CMP(choisir_equipe(&r, "Équipe", eq, NULL, 0), ==, SAISIE_FIN);
    entree_simulee("\nP1\n\n");                                            // ligne vide avec 0 choisi : ignorée
    V_CMP(choisir_equipe(&r, "Équipe", eq, NULL, 0), ==, 1);
    entree_simulee("P1\nP2\nP3\nP4\nP5\n");                                // s'arrête à 4 : TAILLE_EQUIPE_MAX
    V_CMP(choisir_equipe(&r, "Équipe", eq, NULL, 0), ==, TAILLE_EQUIPE_MAX);
    muet_finir(m);
    roster_liberer(&r);
}

static void test_choisir_equipe_exclut_l_autre(void)
{
    roster_t r;
    personnage_t eq[TAILLE_EQUIPE_MAX];
    personnage_t autre[1];
    int m = muet_commencer();

    remplir(&r, 4);
    autre[0] = r.data[0];
    entree_simulee("P1\nP2\n\n");                                          // P1 est déjà de l'autre camp
    V_CMP(choisir_equipe(&r, "Adverse", eq, autre, 1), ==, 1);
    V_TEXTE_EGAL(eq[0].nom, "P2");
    r.data[2].pv = 0;                                                      // un K.O. est refusé
    entree_simulee("P3\nP4\n\n");
    V_CMP(choisir_equipe(&r, "Adverse", eq, autre, 1), ==, 1);
    V_TEXTE_EGAL(eq[0].nom, "P4");
    muet_finir(m);
    roster_liberer(&r);
}

static void test_menu_bataille_refus_et_annulation(void)
{
    roster_t r;
    char s[4096];

    remplir(&r, 1);
    capture_debut();
    menu_bataille(&r);                                                     // un seul personnage
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "au moins 2"));
    roster_liberer(&r);
    remplir(&r, 3);
    entree_simulee("q\n");
    capture_debut();
    menu_bataille(&r);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "Combat annulé"));
    entree_simulee("P1\nP2\nP3\n\n");                                      // il ne resterait personne à l'adversaire
    capture_debut();
    menu_bataille(&r);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "Garde au moins un personnage"));
    roster_liberer(&r);
}

static void test_menu_bataille_combat_complet(void)
{
    roster_t r;
    char entree[4096] = "P1\n\nP2\n\n";                                    // 1 contre 1
    char s[65536];

    remplir(&r, 2);
    r.data[0].atq = 99;
    r.data[1].pv = 5;
    for (int i = 0; i < 40; i++)
        strcat(entree, "1\n1\n");                                          // Attaquer, première cible
    entree_simulee(entree);
    capture_debut();
    menu_bataille(&r);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "Tour de"));
    V_VRAI(contient(s, "Victoire"));
    V_CMP(r.data[1].pv, ==, 5);  // le roster n'est jamais modifié
    roster_liberer(&r);
}

static void test_menu_bataille_abandon(void)
{
    roster_t r;
    char s[16384];

    remplir(&r, 2);
    entree_simulee("P1\n\nP2\n\nq\n");                                     // q à la première action
    capture_debut();
    menu_bataille(&r);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "abandonné"));
    V_FAUX(contient(s, "Victoire"));
    V_FAUX(contient(s, "Défaite"));
    roster_liberer(&r);
}

/* ---------------------------- programme ---------------------------- */

static void test_redstrings_quitter(void)
{
    char s[8192];

    dossier_jetable("quit");
    entree_simulee("0\n");
    capture_debut();
    V_CMP(redstrings(), ==, 0);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "Menu principal"));
    V_VRAI(contient(s, "À bientôt"));
    entree_simulee("");                                                    // Ctrl+D quitte aussi
    capture_debut();
    V_CMP(redstrings(), ==, 0);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "À bientôt"));
}

static void test_redstrings_parcours(void)
{
    char s[65536];

    dossier_jetable("parcours");
    entree_simulee("2\n3\n"                                                // afficher / combat : grisés, refusés
        "1\nNeo\n50\n40\n30\n20\n1\n"                                      // créer Neo
        "1\nTrin\n60\n40\n30\n20\n2\n"                                     // créer Trin
        "4\n"                                                              // sauvegarder
        "5\no\n"                                                           // charger (confirmé)
        "6\n"                                                              // fusionner
        "2\nNeo\n"                                                         // afficher Neo
        "9\n0\n");                                                         // 9 invalide, puis quitter
    capture_debut();
    V_CMP(redstrings(), ==, 0);
    capture_fin(s, sizeof(s));
    V_VRAI(contient(s, "Aucun personnage"));
    V_VRAI(contient(s, "au moins 2 personnages"));
    V_VRAI(contient(s, "Neo a été créé"));
    V_VRAI(contient(s, "Trin a été créé"));
    V_VRAI(contient(s, "2 personnage(s) sauvegardé"));
    V_BOOL(contient(s, "2 personnages chargé") || contient(s, "2 personnage chargé") || contient(s, "chargé"));
    V_VRAI(contient(s, "Valeur invalide"));
    V_CMP(access(FICHIER_SAUVEGARDE, F_OK), ==, 0);
    remove(FICHIER_SAUVEGARDE);
}

void tests_ui_console(void)
{
    LANCER(test_largeur_visible_cas);
    LANCER(test_boite_haut_separateur_bas);
    LANCER(test_boite_ligne_alignement);
    LANCER(test_aligner_texte_cas);
    LANCER(test_boite_champ_cas);
    LANCER(test_afficher_erreur_cas);
    LANCER(test_vider_buffer_cas);
    LANCER(test_lire_ligne_cas);
    LANCER(test_lire_entier_bornes);
    LANCER(test_confirmer_reponses);
    LANCER(test_afficher_liste_contenu);
    LANCER(test_afficher_liste_groupes);
    LANCER(test_afficher_liste_vide);
    LANCER(test_menu_indisponible_raisons);
    LANCER(test_menu_afficher_cas);
    LANCER(test_menu_ajouter_cas);
    LANCER(test_menu_sauvegarder_et_charger);
    LANCER(test_menu_charger_fichier_absent);
    LANCER(test_menu_fusionner_cas);
    LANCER(test_choisir_equipe_cas);
    LANCER(test_choisir_equipe_exclut_l_autre);
    LANCER(test_menu_bataille_refus_et_annulation);
    LANCER(test_menu_bataille_combat_complet);
    LANCER(test_menu_bataille_abandon);
    LANCER(test_redstrings_quitter);
    LANCER(test_redstrings_parcours);
}
