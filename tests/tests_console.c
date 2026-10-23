/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_console.c ────────────────────────────────────────────────────
** │ But         Tester les saisies de la console avec un vrai stdin : le
** │             clavier est remplacé par un fichier, et l'affichage est
** │             muet pendant le test.
** │ Contenu     entree, muet_debut / muet_fin, un test_* par comportement
** │ Vérifie     q annule à chaque étape, rien n'est enregistré, les noms
** │             refusés sont redemandés, Ctrl+D est distinct de q
** ╰──────────────────────────────────────────────────────────────────────
*/

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "composition_console.h"
#include "personnage.h"
#include "roster.h"
#include "saisie.h"
#include "tests_console.h"

#define FICHIER_ENTREE "tests_entree_tmp.txt"

/* Le prochain scanf / fgets lira `texte` ; un texte vide simule Ctrl+D. */
static void entree(const char *texte)
{
    FILE *f = fopen(FICHIER_ENTREE, "w");

    assert(f != NULL);
    fputs(texte, f);
    fclose(f);
    assert(freopen(FICHIER_ENTREE, "r", stdin) != NULL);
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
    assert(lire_entier("x", 0, 99, &v) == SAISIE_ANNULEE && v == -1);
    entree("abc\n150\n-3\n12\n");
    assert(lire_entier("x", 0, 99, &v) == 0 && v == 12);   // refusés, puis accepté
    entree("");
    assert(lire_entier("x", 0, 99, &v) == SAISIE_FIN);      // Ctrl+D n'est pas q
    entree("q\n3\n");
    assert(lire_entier_menu("x", 0, 5, &v) == 0 && v == 3);  // q refusé dans un menu
    muet_fin(m);
}

static void test_lire_nom(void)
{
    char nom[NOM_MAX] = "";
    char trop_long[200];
    char saisie[260];
    int m = muet_debut();

    entree("q\n");
    assert(lire_nom("Nom", nom, sizeof(nom)) == SAISIE_ANNULEE);
    entree("\n\nAr thur\nArthur\n");
    assert(lire_nom("Nom", nom, sizeof(nom)) == 0 && strcmp(nom, "Arthur") == 0);
    entree("123456789012345678901234567890\nLong\n");        // 30 caractères : refusé
    assert(lire_nom("Nom", nom, sizeof(nom)) == 0 && strcmp(nom, "Long") == 0);
    entree("12345678901234567890123456789\n");               // 29 : accepté tel quel
    assert(lire_nom("Nom", nom, sizeof(nom)) == 0 && strlen(nom) == 29);
    memset(trop_long, 'x', sizeof(trop_long) - 1);
    trop_long[sizeof(trop_long) - 1] = '\0';
    snprintf(saisie, sizeof(saisie), "%s\nbon\n", trop_long);
    entree(saisie);                                           // ligne énorme : pas de débordement
    assert(lire_nom("Nom", nom, sizeof(nom)) == 0 && strcmp(nom, "bon") == 0);
    entree("");
    assert(lire_nom("Nom", nom, sizeof(nom)) == SAISIE_FIN);
    assert(lire_nom("Nom", nom, 5) == SAISIE_FIN);            // tampon trop petit : refusé
    muet_fin(m);
}

static void test_confirmer(void)
{
    int m = muet_debut();

    entree("o\n");
    assert(confirmer("?") == 1);
    entree("OUI\n");
    assert(confirmer("?") == 1);
    entree("n\n");
    assert(confirmer("?") == 0);
    entree("\n");
    assert(confirmer("?") == 0);       // Entrée seule : non, le choix sûr
    entree("q\n");
    assert(confirmer("?") == 0);
    entree("peut-être\n42\no\n");
    assert(confirmer("?") == 1);       // réponse bizarre : on redemande
    entree("");
    assert(confirmer("?") == 0);       // Ctrl+D : non
    muet_fin(m);
}

static void test_creation_annulable(void)
{
    personnage_t tab[1] = {{"Zed", 10, 10, 10, GUERRIER, 10}};
    personnage_t p = {0};
    int m = muet_debut();

    entree("q\n");
    assert(creer_personnage(&p, tab, 1) == SAISIE_ANNULEE);          // au nom
    entree("Ada\nq\n");
    assert(creer_personnage(&p, tab, 1) == SAISIE_ANNULEE);          // à la 1re statistique
    entree("Ada\n50\n40\nq\n");
    assert(creer_personnage(&p, tab, 1) == SAISIE_ANNULEE);          // à la 3e
    entree("Ada\n50\n40\n30\n20\nq\n");
    assert(creer_personnage(&p, tab, 1) == SAISIE_ANNULEE);          // au choix de classe
    entree("Ada\n50\n40\n30\n");
    assert(creer_personnage(&p, tab, 1) == SAISIE_FIN);              // Ctrl+D en cours de route
    entree("Zed\nAda\n50\n40\n30\n20\n1\n");
    memset(&p, 0, sizeof(p));
    assert(creer_personnage(&p, tab, 1) == 0);                       // « Zed » refusé, « Ada » créé
    assert(strcmp(p.nom, "Ada") == 0 && p.pv >= STAT_MIN && p.pv <= STAT_MAX);
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
    assert(roster_ajouter(&r, &a) == 0 && roster_ajouter(&r, &b) == 0);
    entree("q\n");
    assert(choisir_equipe(&r, "Équipe", equipe, NULL, 0) == SAISIE_ANNULEE);
    entree("Ada\nq\n");
    assert(choisir_equipe(&r, "Équipe", equipe, NULL, 0) == SAISIE_ANNULEE);   // même après un choix
    entree("Ada\n\n");
    assert(choisir_equipe(&r, "Équipe", equipe, NULL, 0) == 1);
    entree("");
    assert(choisir_equipe(&r, "Équipe", equipe, NULL, 0) == SAISIE_FIN);
    assert(r.len == 2);                                                        // le roster est intact
    roster_liberer(&r);
    muet_fin(m);
}

void tests_console(void)
{
    test_lire_entier();
    test_lire_nom();
    test_confirmer();
    test_creation_annulable();
    test_composition_annulable();
    remove(FICHIER_ENTREE);
}
