/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_null.c ───────────────────────────────────────────────────────
** │ But         Chaque fonction publique est appelée avec des pointeurs
** │             NULL, des tailles négatives ou nulles, des indices et des
** │             énumérations hors limites. Elle doit répondre « erreur »
** │             (0, -1, NULL, rien) sans planter : les sanitizers
** │             détectent le moindre accès invalide.
** │ Contenu     libmy · personnage · roster · sauvegarde · bataille ·
** │             saisie et boîtes · menus de la console
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bataille.h"
#include "bataille_console.h"
#include "boite.h"
#include "composition_console.h"
#include "lanceur.h"
#include "liste_console.h"
#include "menu_personnages.h"
#include "my.h"
#include "my_string.h"
#include "personnage.h"
#include "roster.h"
#include "saisie.h"
#include "sauvegarde.h"
#include "tests_null.h"

/* Chaîne NULL que le compilateur ne peut pas deviner (pas d'avertissement de format). */
static const char *volatile g_nul = NULL;

/* Entrée et sortie vers /dev/null : ces tests ne doivent ni lire ni afficher. */
static void isoler(void)
{
    int nul = open("/dev/null", O_RDWR);

    V_CMP(nul, >=, 0);
    dup2(nul, STDIN_FILENO);
    dup2(nul, STDOUT_FILENO);
    close(nul);
}

static personnage_t un_perso(void)
{
    personnage_t p = {"Bob", 50, 20, 10, 5, GUERRIER};

    return p;
}

/* ------------------------------------------------------------------ */
/*  libmy                                                              */
/* ------------------------------------------------------------------ */

static void test_null_chaines(void)
{
    char buf[8] = "ab";
    char *vide[1] = {NULL};

    V_CMP(my_strlen(NULL), ==, 0);
    V_TEXTE_EGAL(NULL, NULL);
    V_CMP(my_strcmp(NULL, "a"), <, 0);
    V_CMP(my_strcmp("a", NULL), >, 0);
    V_CMP(my_strncmp(NULL, "a", 3), <=, 0);
    V_CMP(my_strncmp("a", NULL, 3), >=, 0);
    V_CMP(my_strncmp("a", "b", -1), ==, 0);
    V_CMP(my_strstr(NULL, "a"), ==, NULL);
    V_CMP(my_strstr(buf, NULL), ==, NULL);
    V_CMP(my_strcpy(NULL, "a"), ==, NULL);
    V_CMP(my_strcpy(buf, NULL), ==, buf);
    V_CMP(my_strncpy(NULL, "a", 2), ==, NULL);
    V_CMP(my_strncpy(buf, NULL, 2), ==, buf);
    V_CMP(my_strncpy(buf, "zz", -5), ==, buf);
    V_CMP(my_strcat(NULL, "a"), ==, NULL);
    V_CMP(my_strcat(buf, NULL), ==, buf);
    V_CMP(my_strncat(NULL, "a", 2), ==, NULL);
    V_CMP(my_strncat(buf, NULL, 2), ==, buf);
    V_CMP(my_strncat(buf, "zz", -5), ==, buf);
    V_CMP(my_strdup(NULL), ==, NULL);
    V_CMP(my_revstr(NULL), ==, NULL);
    V_CMP(my_strupcase(NULL), ==, NULL);
    V_CMP(my_strlowcase(NULL), ==, NULL);
    V_CMP(my_strcapitalize(NULL), ==, NULL);
    V_CMP(my_str_isalpha(NULL), ==, 0);
    V_CMP(my_str_islower(NULL), ==, 0);
    V_CMP(my_str_isupper(NULL), ==, 0);
    V_CMP(my_str_isnum(NULL), ==, 0);
    V_CMP(my_str_isprintable(NULL), ==, 0);
    V_CMP(my_getnbr(NULL), ==, 0);
    V_CMP(my_arraylen(NULL), ==, 0);
    V_CMP(my_arraylen(vide), ==, 0);
}

static void test_null_affichage_lib(void)
{
    isoler();
    my_putstr(NULL);
    my_showstr(NULL);
    my_showmem(NULL, 10);
    my_showmem("abc", -4);
    my_put_nbr(-2147483647 - 1);
    my_show_word_array(NULL);
    V_CMP(my_print_params(3, NULL), ==, 0);
    V_CMP(my_print_params(-1, NULL), ==, 0);
    V_CMP(my_rev_params(3, NULL), ==, 0);
    V_CMP(my_rev_params(-1, NULL), ==, 0);
    V_CMP(my_sort_params(3, NULL), ==, 0);
    V_CMP(my_sort_params(-1, NULL), ==, 0);
}

static void test_null_tableaux_lib(void)
{
    char *av[3] = {"b", NULL, "a"};
    char *vide[1] = {NULL};
    char **mots = NULL;
    char *a = "x";
    char *colle = NULL;

    isoler();
    V_CMP(my_arraystrlen(3, NULL), ==, 0);
    V_CMP(my_arraystrlen(-1, av), ==, 0);
    V_CMP(my_arraystrlen(3, av), ==, 2);  // un élément NULL compte pour 0
    V_CMP(my_str_to_word_array(NULL), ==, NULL);
    mots = my_str_to_word_array("");
    V_CMP_BLOQUANT(mots, !=, NULL);
    V_CMP(mots[0], ==, NULL);
    free(mots);
    V_CMP(my_print_params(3, av), ==, 0);
    V_CMP(my_rev_params(3, av), ==, 0);
    V_CMP(my_sort_params(3, av), ==, 0);
    V_CMP(concat_params(3, NULL), ==, NULL);
    V_CMP(concat_params(-1, av), ==, NULL);
    colle = concat_params(3, av);
    V_CMP_BLOQUANT(colle, !=, NULL);
    free(colle);
    colle = concat_params(0, vide);
    V_CMP_BLOQUANT(colle, !=, NULL);
    V_CMP(colle[0], ==, '\0');
    free(colle);
    my_swap_params(NULL, NULL);
    my_swap_params(&a, NULL);
    my_swap(NULL, NULL);
    my_swap(&(int){1}, NULL);
}

static void test_null_nombres_lib(void)
{
    int tab[3] = {3, 1, 2};

    my_sort_int_array(NULL, 5);
    my_sort_int_array(tab, -1);
    my_sort_int_array(tab, 0);
    V_CMP(my_compute_power_rec(2, -1), ==, 0);
    V_CMP(my_compute_square_root(-4), ==, 0);
    V_CMP(my_compute_square_root(0), ==, 0);
    V_CMP(my_is_prime(-7), ==, 0);
    V_CMP(my_is_prime(0), ==, 0);
    V_CMP(my_is_prime(1), ==, 0);
    V_CMP(my_find_prime_sup(-5), ==, 2);
    V_CMP(my_getnbr(""), ==, 0);
    V_CMP(my_getnbr("99999999999999999999"), ==, 0);
    V_CMP(my_getnbr("-2147483648"), ==, -2147483647 - 1);
}

/* ------------------------------------------------------------------ */
/*  Personnages, roster, sauvegarde                                    */
/* ------------------------------------------------------------------ */

static void test_null_personnage(void)
{
    personnage_t p = un_perso();
    classe_t ordre[NB_CLASSES];

    isoler();
    V_CMP(valeur_stat(NULL, STAT_PV), ==, 0);
    V_CMP(valeur_stat(&p, (stat_t)99), ==, 0);
    V_CMP(valeur_stat(&p, (stat_t)-1), ==, 0);
    V_CMP(stat_finale((classe_t)999, STAT_PV, 50), ==, 50);
    V_CMP(stat_finale(GUERRIER, (stat_t)999, 50), ==, 50);
    V_CMP(stat_finale(GUERRIER, STAT_PV, 100000), <=, STAT_MAX);
    appliquer_classe(NULL);
    p.classe = (classe_t)999;
    appliquer_classe(&p);
    V_CMP(trouver_personnage(NULL, 3, "a"), ==, -1);
    V_CMP(trouver_personnage(&p, 1, NULL), ==, -1);
    V_CMP(trouver_personnage(&p, -1, "a"), ==, -1);
    V_CMP(nom_est_un_id(NULL), ==, 0);
    V_CMP(nom_est_un_id(""), ==, 0);
    V_CMP_BLOQUANT(nom_categorie((categorie_t)99), !=, NULL);
    V_CMP_BLOQUANT(nom_categorie((categorie_t)-1), !=, NULL);
    V_CMP_BLOQUANT(couleur_ansi((classe_t)999), !=, NULL);
    V_CMP_BLOQUANT(couleur_ansi((classe_t)-1), !=, NULL);
    V_FAUX(classe_valide(-1));
    V_FAUX(classe_valide(NB_CLASSES));
    ordre_classes(NULL);
    ordre_classes(ordre);
    afficher_personnage(NULL);
    p.classe = (classe_t)-5;
    afficher_personnage(&p);
    V_CMP(calculer_degats(NULL, &p), ==, 0);
    V_CMP(calculer_degats(&p, NULL), ==, 0);
    V_CMP(est_ko(NULL), ==, 1);
}

static void test_null_roster(void)
{
    roster_t r;
    personnage_t p = un_perso();

    roster_init(NULL);
    roster_liberer(NULL);
    V_CMP(roster_ajouter(NULL, &p), ==, -1);
    roster_init(&r);
    V_CMP(roster_ajouter(&r, NULL), ==, -1);
    V_CMP(r.len, ==, 0);
    V_CMP(roster_trouver(NULL, "Bob"), ==, -1);
    V_CMP(roster_trouver(&r, NULL), ==, -1);
    V_CMP(roster_trouver(&r, "1"), ==, -1);
    V_CMP(roster_trouver(&r, "-1"), ==, -1);
    V_CMP(roster_nb_en_forme(NULL), ==, 0);
    V_CMP(roster_nb_en_forme(&r), ==, 0);
    r.len = 5;                          // incohérent : len > 0 mais data NULL
    V_CMP(roster_nb_en_forme(&r), ==, 0);
    V_CMP(roster_trouver(&r, "Bob"), ==, -1);
    V_CMP(roster_trouver(&r, "2"), ==, -1);
    r.len = 0;
    roster_liberer(&r);
}

static void test_null_sauvegarde(void)
{
    roster_t r;
    int a = 0;
    int i = 0;

    roster_init(&r);
    V_CMP(sauvegarder(NULL, "tests_null_tmp.txt"), !=, SAUVEGARDE_OK);
    V_CMP(sauvegarder(&r, NULL), !=, SAUVEGARDE_OK);
    V_CMP(sauvegarder(&r, ""), !=, SAUVEGARDE_OK);
    V_CMP(sauvegarder(&r, "/dossier/inexistant/x.txt"), !=, SAUVEGARDE_OK);
    V_CMP(charger(NULL, "x.txt"), !=, SAUVEGARDE_OK);
    V_CMP(charger(&r, NULL), !=, SAUVEGARDE_OK);
    V_CMP(charger(&r, ""), !=, SAUVEGARDE_OK);
    V_CMP(charger(&r, "/"), !=, SAUVEGARDE_OK);  // un dossier
    V_CMP(fusionner(NULL, "x.txt", &a, &i), !=, SAUVEGARDE_OK);
    V_CMP(fusionner(&r, NULL, &a, &i), !=, SAUVEGARDE_OK);
    V_CMP(fusionner(&r, "n_existe_pas.txt", NULL, NULL), !=, SAUVEGARDE_OK);
    V_CMP_BLOQUANT(sauvegarde_message((sauvegarde_t)99), !=, NULL);
    V_CMP_BLOQUANT(sauvegarde_message((sauvegarde_t)-1), !=, NULL);
    r.len = 3;                          // incohérent : len > 0 mais data NULL
    V_CMP(sauvegarder(&r, "tests_null_tmp.txt"), !=, SAUVEGARDE_OK);
    r.len = 0;
    remove("tests_null_tmp.txt");
    roster_liberer(&r);
}

/* ------------------------------------------------------------------ */
/*  Combat                                                             */
/* ------------------------------------------------------------------ */

static void test_null_bataille_nulle(void)
{
    personnage_t p = un_perso();
    evenement_t ev[MAX_EVENEMENTS];
    ref_t cibles[NB_EQUIPES * TAILLE_EQUIPE_MAX];
    action_t a = ACTION_ATTAQUER;
    ref_t c = {0, 0};

    V_CMP(bataille_init(NULL, &p, 1, &p, 1), ==, -1);
    V_CMP(bataille_lire(NULL, c), ==, NULL);
    V_CMP(bataille_combattant(NULL, c), ==, NULL);
    V_CMP(combattant_vivant(NULL), ==, 0);
    V_CMP(combattant_en_garde(NULL), ==, 0);
    V_CMP(bataille_degats(NULL, NULL), ==, 0);
    V_CMP(bataille_soin(NULL, NULL), ==, 0);
    V_CMP(bataille_cible_valide(NULL, ACTION_ATTAQUER, c), ==, 0);
    V_CMP(bataille_cibles(NULL, ACTION_ATTAQUER, cibles), ==, 0);
    V_CMP(bataille_action_possible(NULL, ACTION_ATTAQUER), ==, 0);
    V_CMP(bataille_agir(NULL, ACTION_ATTAQUER, c, ev), ==, -1);
    V_CMP(bataille_journal(NULL, 0), ==, NULL);
    bataille_ia(NULL, &a, &c);
    bataille_ia(NULL, NULL, NULL);
    (void)bataille_actif(NULL);
}

static void test_null_bataille_arguments(void)
{
    bataille_t b;
    personnage_t p = un_perso();
    personnage_t dead = un_perso();
    personnage_t equipe[TAILLE_EQUIPE_MAX + 1];
    evenement_t ev[MAX_EVENEMENTS];
    ref_t cibles[NB_EQUIPES * TAILLE_EQUIPE_MAX];

    for (int i = 0; i <= TAILLE_EQUIPE_MAX; i++)
        equipe[i] = p;
    dead.pv = 0;
    V_CMP(bataille_init(&b, NULL, 1, &p, 1), ==, -1);
    V_CMP(bataille_init(&b, &p, 1, NULL, 1), ==, -1);
    V_CMP(bataille_init(&b, &p, 0, &p, 1), ==, -1);
    V_CMP(bataille_init(&b, &p, -3, &p, 1), ==, -1);
    V_CMP(bataille_init(&b, equipe, TAILLE_EQUIPE_MAX + 1, &p, 1), ==, -1);
    V_CMP(bataille_init(&b, &dead, 1, &p, 1), ==, -1);
    V_CMP(bataille_init(&b, &p, 1, &p, 1), ==, 0);
    V_CMP(bataille_cibles(&b, ACTION_ATTAQUER, NULL), ==, 0);
    V_CMP(bataille_cibles(&b, (action_t)99, cibles), ==, 0);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, NULL), ==, -1);
    V_CMP(bataille_agir(&b, (action_t)99, (ref_t){1, 0}, ev), ==, -1);
    V_CMP(bataille_agir(&b, (action_t)-1, (ref_t){1, 0}, ev), ==, -1);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){-1, 0}, ev), ==, -1);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){NB_EQUIPES, 0}, ev), ==, -1);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, -1}, ev), ==, -1);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 99}, ev), ==, -1);
    V_CMP(bataille_cible_valide(&b, (action_t)99, (ref_t){1, 0}), ==, 0);
    V_CMP(bataille_action_possible(&b, (action_t)99), ==, 0);
}

static void test_null_bataille_indices(void)
{
    bataille_t b;
    personnage_t p = un_perso();

    V_CMP(bataille_init(&b, &p, 1, &p, 1), ==, 0);
    V_CMP(bataille_lire(&b, (ref_t){-1, 0}), ==, NULL);
    V_CMP(bataille_lire(&b, (ref_t){0, -1}), ==, NULL);
    V_CMP(bataille_lire(&b, (ref_t){NB_EQUIPES, 0}), ==, NULL);
    V_CMP(bataille_lire(&b, (ref_t){0, TAILLE_EQUIPE_MAX}), ==, NULL);
    V_CMP(bataille_lire(&b, (ref_t){0, 1}), ==, NULL);  // hors de la taille de l'équipe
    V_CMP(bataille_combattant(&b, (ref_t){9, 9}), ==, NULL);
    V_CMP(bataille_journal(&b, -1), ==, NULL);
    V_CMP(bataille_journal(&b, 100000), ==, NULL);
    V_CMP_BLOQUANT(bataille_journal(&b, 0), !=, NULL);
    b.tour = 999;                                               // état corrompu
    bataille_ia(&b, &(action_t){0}, &(ref_t){0, 0});
}

static void test_null_bataille_degats(void)
{
    bataille_t b;
    personnage_t p = un_perso();
    const combattant_t *c = NULL;

    V_CMP(bataille_init(&b, &p, 1, &p, 1), ==, 0);
    c = bataille_lire(&b, (ref_t){0, 0});
    V_CMP_BLOQUANT(c, !=, NULL);
    V_CMP(bataille_degats(c, NULL), ==, 0);
    V_CMP(bataille_degats(NULL, c), ==, 0);
    V_CMP(bataille_soin(c, NULL), ==, 0);
    V_CMP(bataille_soin(NULL, c), ==, 0);
}

/* ------------------------------------------------------------------ */
/*  Console : saisie, boîtes, menus                                    */
/* ------------------------------------------------------------------ */

static void test_null_saisie(void)
{
    char buf[8];
    int n = 0;

    isoler();                           // entrée vide : lire_ligne renvoie la fin
    afficher_erreur(NULL);
    V_CMP(lire_ligne(NULL, 8), ==, SAISIE_FIN);
    V_CMP(lire_ligne(buf, 0), <, 0);
    V_CMP(lire_ligne(buf, 1), <=, 0);
    V_CMP(lire_entier(NULL, 1, 5, &n), <, 0);
    V_CMP(lire_entier("?", 1, 5, NULL), <, 0);
    V_CMP(lire_entier("?", 5, 1, &n), <, 0);
    V_CMP(lire_entier_menu(NULL, 1, 5, &n), <, 0);
    V_CMP(lire_entier_menu("?", 1, 5, NULL), <, 0);
    V_CMP(lire_nom(NULL, buf, sizeof(buf)), <, 0);
    V_CMP(lire_nom("?", NULL, 8), <, 0);
    V_CMP(lire_nom("?", buf, 0), <, 0);
    V_CMP(confirmer(NULL), ==, 0);
    vider_buffer();
}

static void test_null_boites(void)
{
    char dst[16];

    isoler();
    boite_haut(NULL);
    boite_separateur(NULL);
    boite_bas(NULL);
    boite_ligne(NULL, g_nul, 1);
    boite_ligne("", g_nul, 1);
    boite_champ(NULL, NULL, g_nul, 1);
    boite_champ("", "x", g_nul, 1);
    V_CMP(largeur_visible(NULL), ==, 0);
    aligner_texte(NULL, 16, "x", 5);
    aligner_texte(dst, 0, "x", 5);
    aligner_texte(dst, 16, NULL, 5);
    V_TEXTE_EGAL(dst, "     ");  // NULL se comporte comme ""
    aligner_texte(dst, 16, "x", -3);
    aligner_texte(dst, 16, "x", 100000);
    aligner_texte(dst, 1, "xyz", 2);
    V_CMP(dst[0], ==, '\0');
}

static void test_null_menus(void)
{
    roster_t r;
    personnage_t eq[TAILLE_EQUIPE_MAX];
    groupe_liste_t g = {NULL, 4, NULL, NULL};

    isoler();
    roster_init(&r);
    V_CMP_BLOQUANT(menu_indisponible(NULL, CHOIX_COMBAT), !=, NULL);
    V_CMP(menu_indisponible(&r, -1), ==, NULL);
    V_CMP(menu_indisponible(&r, NB_CHOIX), ==, NULL);
    menu_ajouter(NULL);
    menu_afficher(NULL);
    menu_sauvegarder(NULL);
    menu_charger(NULL);
    menu_fusionner(NULL);
    menu_bataille(NULL);
    afficher_liste(NULL, NULL, 0);
    afficher_liste(&r, NULL, 3);
    afficher_liste(&r, &g, 1);
    afficher_liste(&r, &g, -2);
    V_CMP(choisir_equipe(NULL, "t", eq, NULL, 0), <, 0);
    V_CMP(choisir_equipe(&r, "t", NULL, NULL, 0), <, 0);
    V_CMP(choisir_equipe(&r, NULL, eq, NULL, 0), <, 0);
    V_CMP(choisir_equipe(&r, "t", eq, NULL, 3), <, 0);
    V_CMP(choisir_equipe(&r, "t", eq, NULL, -1), <, 0);
    roster_liberer(&r);
}

static void test_null_creation(void)
{
    personnage_t p = un_perso();
    personnage_t tab[2] = {0};

    isoler();
    V_CMP(creer_personnage(NULL, tab, 0), !=, 0);
    V_CMP(creer_personnage(&p, NULL, 3), !=, 0);
    V_CMP(creer_personnage(&p, tab, -1), !=, 0);
}

void tests_null(void)
{
    LANCER(test_null_chaines);
    LANCER(test_null_affichage_lib);
    LANCER(test_null_tableaux_lib);
    LANCER(test_null_nombres_lib);
    LANCER(test_null_personnage);
    LANCER(test_null_roster);
    LANCER(test_null_sauvegarde);
    LANCER(test_null_bataille_nulle);
    LANCER(test_null_bataille_arguments);
    LANCER(test_null_bataille_indices);
    LANCER(test_null_bataille_degats);
    LANCER(test_null_saisie);
    LANCER(test_null_boites);
    LANCER(test_null_menus);
    LANCER(test_null_creation);
}
