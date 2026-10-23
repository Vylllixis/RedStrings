/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_null.c (graphique) ───────────────────────────────────────────
** │ But         Les fonctions de la version graphique qui n'ont pas
** │             besoin de fenêtre reçoivent des NULL, des tailles nulles
** │ ou négatives et des identifiants hors limites : elles répondent
** │ « erreur » sans planter.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdlib.h>
#include <string.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_carte.h"
#include "gui_app.h"
#include "gui_jeu.h"
#include "gui_sons.h"
#include "gui_utf8.h"
#include "gui_yugioh.h"
#include "lanceur.h"
#include "../src/app/app_interne.h"
#include "../src/audio/audio_interne.h"
#include "tests_null.h"

static void test_null_utf8(void)
{
    char o[4];
    uint32_t pts[4];
    char s[8] = "ab";

    V_CMP(utf8_encoder(0x41, NULL), ==, 0);
    V_CMP(utf8_encoder(0x110000, o), ==, 0);
    V_CMP(utf8_decoder(NULL, pts, 4), ==, 0);
    V_CMP(utf8_decoder("ab", NULL, 4), ==, 0);
    V_CMP(utf8_decoder("ab", pts, 0), ==, 0);
    V_CMP(utf8_decoder("ab", pts, -3), ==, 0);
    V_CMP(utf8_nb_caracteres(NULL), ==, 0);
    utf8_supprimer_dernier(NULL);
    utf8_supprimer_dernier(&s[2]);              // chaîne vide
    utf8_majuscules(NULL);
    s[0] = '\0';
    utf8_supprimer_dernier(s);
}

static void test_null_animation(void)
{
    ressort_t r = {0};

    maj_ressort(NULL, 1.f, 10.f, 1.f, 0.016f);
    maj_ressort(&r, 1.f, 10.f, 1.f, 0.f);
    maj_ressort(&r, 1.f, 10.f, 1.f, -1.f);
    V_CMP(bornerf(0.5f, 1.f, 0.f), >=, 0.f);  // bornes inversées
    V_CMP(progression(0.5f, 0.f, 0.f), >=, 0.f);
    V_CMP(progression(0.5f, 0.f, -1.f), >=, 0.f);
    V_CMP(approcher(0.f, 1.f, 1.f, -1.f), <=, 1.f);
    V_CMP(aleatoire(5.f, 1.f), >=, 1.f);
}

static void test_null_jeu(void)
{
    mode_visuel_t m = MODE_YUGIOH;
    char arg[2] = "x";
    char *av[2] = {arg, NULL};

    V_CMP(contient_nom(NULL, "a"), ==, 0);
    V_CMP(contient_nom("a", NULL), ==, 1);
    V_CMP(contient_nom("Bob", ""), ==, 1);
    V_CMP(mode_depuis_arguments(2, NULL, &m), ==, -1);
    V_CMP(mode_depuis_arguments(2, av, &m), ==, -1);  // argv[1] NULL
    V_CMP(mode_depuis_arguments(1, av, NULL), ==, -1);
    V_CMP(mode_depuis_arguments(-1, av, &m), ==, 0);  // aucun argument
    V_CMP(m, ==, MODE_NORMAL);  // aucun argument
    V_CMP(mode_depuis_arguments(0, NULL, &m), ==, 0);
    V_CMP(m, ==, MODE_NORMAL);
}

static void test_null_classes_graphiques(void)
{
    V_CMP_BLOQUANT(classe_info((classe_t)999), !=, NULL);
    V_CMP_BLOQUANT(classe_info((classe_t)-1), !=, NULL);
    V_CMP_BLOQUANT(nom_classe((classe_t)999), !=, NULL);
    V_CMP_BLOQUANT(nom_classe_maj((classe_t)-1), !=, NULL);
    (void)couleur_classe((classe_t)999);
    (void)couleur_stat((stat_t)99);
    V_CMP_BLOQUANT(nom_stat((stat_t)99), !=, NULL);
    V_CMP_BLOQUANT(abrege_stat((stat_t)-1), !=, NULL);
    V_CMP_BLOQUANT(carte_de_classe((classe_t)-1), !=, NULL);
    V_CMP_BLOQUANT(nom_cadre((cadre_carte_t)99), !=, NULL);
    V_CMP_BLOQUANT(nom_attribut((attribut_carte_t)99), !=, NULL);
    V_CMP_BLOQUANT(symbole_attribut((attribut_carte_t)-1), !=, NULL);
    (void)couleur_cadre((cadre_carte_t)99, 1);
    (void)encre_cadre((cadre_carte_t)-1);
    (void)fond_titre((cadre_carte_t)99);
    (void)couleur_attribut((attribut_carte_t)99);
}

static void test_null_sons_texte(void)
{
    char erreur[16];
    pack_t p;
    char chemin[32];

    V_CMP(son_info((son_t)-1), ==, NULL);
    V_CMP(son_depuis_nom(NULL), ==, -1);
    V_CMP(musique_depuis_nom(NULL), ==, -1);
    V_CMP_BLOQUANT(nom_musique((musique_t)99), !=, NULL);
    V_CMP_BLOQUANT(nom_theme((theme_son_t)99), !=, NULL);
    V_CMP_BLOQUANT(dossier_theme((theme_son_t)-1), !=, NULL);
    pack_vider(NULL, "x");
    pack_vider(&p, NULL);
    pack_vider(&p, "assets");
    V_CMP(pack_lire_ligne(NULL, "a", erreur, sizeof(erreur)), ==, -1);
    V_CMP(pack_lire_ligne(&p, NULL, erreur, sizeof(erreur)), ==, -1);
    V_CMP(pack_lire_ligne(&p, "carte.selection x.ogg", NULL, 0), <=, 0);
    V_CMP(pack_lire_ligne(&p, "????", erreur, 1), ==, -1);
    V_CMP(pack_lire_ligne(&p, "carte.selection volume=nan x.ogg", erreur, sizeof(erreur)), ==, -1);
    V_CMP(pack_charger(NULL, "d", "m", erreur, sizeof(erreur)), ==, -1);
    V_CMP(pack_charger(&p, NULL, "m", erreur, sizeof(erreur)), ==, -1);
    V_CMP(pack_charger(&p, "d", NULL, erreur, sizeof(erreur)), ==, -1);
    V_CMP(pack_charger(&p, "d", "m", NULL, 0), ==, -1);
    V_CMP(pack_chemin(NULL, "a", chemin, sizeof(chemin)), ==, -1);
    V_CMP(pack_chemin(&p, NULL, chemin, sizeof(chemin)), ==, -1);
    V_CMP(pack_chemin(&p, "a", NULL, 0), ==, -1);
    V_CMP(pack_chemin(&p, "a", chemin, 0), ==, -1);
    V_CMP(pack_chemin(&p, "a", chemin, 1), ==, -1);
}

static void test_null_sons_regles(void)
{
    pack_t p;
    reglages_audio_t r;
    entree_son_t e = {0};
    const pack_t *source = NULL;
    unsigned int graine = 1;
    int rare = 0;
    voix_info_t voix[2] = {{1, 2, 0.f}, {0, 1, 0.f}};

    pack_vider(&p, "x");
    reglages_defaut(&r);
    V_CMP(hasard(NULL), >=, 0.f);
    V_CMP(son_resoudre(NULL, NULL, SON_CARTE_SELECTION, &source), ==, NULL);
    V_CMP(son_resoudre(&p, &p, (son_t)-1, NULL), ==, NULL);
    V_CMP(son_resoudre(&p, &p, NB_SONS, &source), ==, NULL);
    V_CMP(variante_choisir(NULL, 0, &graine, &rare), ==, -1);
    V_BOOL(variante_choisir(&e, 0, NULL, NULL) == -1 || variante_choisir(&e, 0, NULL, NULL) == 0);
    V_CMP(variante_choisir(&e, 99, &graine, &rare), <=, 0);
    V_CMP(volume_effectif(NULL, 0, 1.f), <, 0.001f);
    V_CMP(volume_effectif(&r, 0, -5.f), <, 0.001f);
    V_CMP(volume_effectif(&r, 0, 1e30f), <=, 100.f);
    V_CMP(son_autorise((son_t)-1, 0.f, 0, 1.f), ==, 0);
    V_CMP(son_autorise(NB_SONS, 0.f, 0, 1.f), ==, 0);
    V_CMP(son_autorise(SON_CARTE_SELECTION, 0.f, -1, 1.f), >=, 0);
    V_CMP(voix_choisir(NULL, 2, 1), ==, -1);
    V_CMP(voix_choisir(voix, 0, 1), ==, -1);
    V_CMP(voix_choisir(voix, -1, 1), ==, -1);
    (void)theme_effectif((choix_theme_t)99, 1);
    reglages_defaut(NULL);
    V_CMP(reglages_lire(NULL, "x"), ==, -1);
    V_CMP(reglages_lire(&r, NULL), ==, -1);
    V_CMP(reglages_ecrire(NULL, "x"), ==, -1);
    V_CMP(reglages_ecrire(&r, NULL), ==, -1);
    V_CMP(reglages_ecrire(&r, ""), ==, -1);
    V_CMP(reglages_lire(&r, ""), ==, -1);
    V_CMP_BLOQUANT(nom_choix_theme((choix_theme_t)99), !=, NULL);
    r.general = 100000;
    r.theme = (choix_theme_t)99;
    V_CMP(volume_effectif(&r, 1, 1.f), <=, 100.f);
}

/* Pour ces fonctions, 0 veut dire « réussi » : un NULL doit donner une erreur, jamais 0. */
static void test_null_codes_de_retour(void)
{
    char chemin[64];
    char dossier[64];

    V_CMP(creer_app(NULL, MODE_NORMAL), !=, 0);
    V_CMP(app_creer_ressources(NULL), !=, 0);
    V_CMP(chemin_ressource("fonts/x.ttf", NULL, 0), !=, 0);
    V_CMP(chemin_ressource("fonts/x.ttf", chemin, 0), !=, 0);
    V_CMP(chemin_ressource(NULL, chemin, sizeof(chemin)), !=, 0);
    V_CMP(audio_chemin_pack(THEME_STANDARD, NULL, 0), !=, 0);
    V_CMP(audio_chemin_pack(THEME_STANDARD, dossier, 0), !=, 0);
    V_CMP(ouvrir_fenetre(NULL, AFFICHAGE_FENETRE), !=, 0);
}

void tests_null(void)
{
    LANCER(test_null_utf8);
    LANCER(test_null_animation);
    LANCER(test_null_jeu);
    LANCER(test_null_classes_graphiques);
    LANCER(test_null_sons_texte);
    LANCER(test_null_sons_regles);
    LANCER(test_null_codes_de_retour);
    tests_null_fenetre();
}
