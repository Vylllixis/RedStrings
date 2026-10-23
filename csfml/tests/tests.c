/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests.c (graphique) ────────────────────────────────────────────────
** │ But         Tests des fonctions de la version graphique qui ne
** │             touchent pas à SFML : UTF-8, courbes d'animation,
** │             couleurs, recherche par nom et accès sûrs à la table des
** │             classes. Aucune fenêtre n'est ouverte.
** │ Lancer      make csfml-tests
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <string.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_carte.h"
#include "gui_jeu.h"
#include "gui_utf8.h"
#include "gui_yugioh.h"
#include "lanceur.h"
#include "tests_audio.h"
#include "tests_fonctions.h"
#include "tests_null.h"

static int proche(float a, float b)
{
    return fabsf(a - b) < 0.0001f;
}

static void test_utf8_encoder(void)
{
    char o[4];

    V_CMP(utf8_encoder('A', o), ==, 1);
    V_CMP(o[0], ==, 'A');
    V_CMP(utf8_encoder(0xE9, o), ==, 2);
    V_CMP((unsigned char)o[0], ==, 0xC3);
    V_CMP((unsigned char)o[1], ==, 0xA9);
    V_CMP(utf8_encoder(0x20AC, o), ==, 3);
    V_CMP(utf8_encoder(0x1F600, o), ==, 4);
    V_CMP(utf8_encoder(0xD800, o), ==, 0);  /* substitut */
    V_CMP(utf8_encoder(0xDFFF, o), ==, 0);
    V_CMP(utf8_encoder(0x110000, o), ==, 0);  /* hors Unicode */
    V_CMP(utf8_encoder(0xFFFFFFFF, o), ==, 0);
}

static void test_utf8_aller_retour(void)
{
    const uint32_t points[] = {'a', 0xE9, 0x7FF, 0x800, 0x20AC, 0xFFFF, 0x10000, 0x10FFFF};
    char o[5] = {0};
    uint32_t d[4];
    int n = 0;

    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); i++) {
        memset(o, 0, sizeof(o));
        n = utf8_encoder(points[i], o);
        V_CMP(n, >, 0);
        V_CMP(utf8_decoder(o, d, 4), ==, 1);
        V_CMP(d[0], ==, points[i]);
        V_CMP(d[1], ==, 0);
    }
}

static void test_utf8_decoder_refuse_le_invalide(void)
{
    uint32_t d[8];

    V_CMP(utf8_decoder("é", d, 8), ==, 1);
    V_CMP(d[0], ==, 0xE9);
    V_CMP(utf8_decoder("\xC3", d, 8), ==, 0);  /* séquence tronquée */
    V_CMP(utf8_decoder("\xE2\x82", d, 8), ==, 0);
    V_CMP(utf8_decoder("\xC0\x80", d, 8), ==, 0);  /* codage trop long du NUL */
    V_CMP(utf8_decoder("\xE0\x80\x80", d, 8), ==, 0);
    V_CMP(utf8_decoder("\xED\xA0\x80", d, 8), ==, 0);  /* substitut UTF-16 */
    V_CMP(utf8_decoder("\xF4\x90\x80\x80", d, 8), ==, 0);  /* au-dessus de U+10FFFF */
    V_CMP(utf8_decoder("\x80", d, 8), ==, 0);  /* octet de suite isolé */
    V_CMP(utf8_decoder("\xFF", d, 8), ==, 0);
    V_CMP(utf8_decoder("a\x80" "b", d, 8), ==, 2);
    V_CMP(d[0], ==, 'a');
    V_CMP(d[1], ==, 'b');
}

static void test_utf8_decoder_borne(void)
{
    uint32_t d[4] = {9, 9, 9, 9};

    V_CMP(utf8_decoder("abcdef", d, 4), ==, 3);  /* max - 1 : place pour le 0 final */
    V_CMP(d[3], ==, 0);
    V_CMP(utf8_decoder("abc", d, 1), ==, 0);
    V_CMP(d[0], ==, 0);
    V_CMP(utf8_decoder("abc", d, 0), ==, 0);
    V_CMP(utf8_decoder("", d, 4), ==, 0);
    V_CMP(d[0], ==, 0);
}

static void test_utf8_outils(void)
{
    char s[16] = "Prêtre";
    char v[16] = "";

    V_CMP(utf8_nb_caracteres("Prêtre"), ==, 6);
    V_CMP(utf8_nb_caracteres(""), ==, 0);
    utf8_supprimer_dernier(s);
    V_TEXTE_EGAL(s, "Prêtr");
    strcpy(s, "é");
    utf8_supprimer_dernier(s);
    V_CMP(s[0], ==, '\0');
    utf8_supprimer_dernier(v);                          /* chaîne vide : rien ne se passe */
    V_CMP(v[0], ==, '\0');
    strcpy(s, "Élémentalis");
    utf8_majuscules(s);
    V_TEXTE_EGAL(s, "ÉLÉMENTALIS");
}

static void test_animation(void)
{
    V_VRAI(proche(bornerf(5.f, 0.f, 1.f), 1.f));
    V_VRAI(proche(bornerf(-5.f, 0.f, 1.f), 0.f));
    V_VRAI(proche(progression(0.f, 1.f, 2.f), 0.f));
    V_VRAI(proche(progression(2.f, 1.f, 2.f), 0.5f));
    V_VRAI(proche(progression(9.f, 1.f, 2.f), 1.f));
    V_VRAI(proche(sortie_cubique(0.f), 0.f));
    V_VRAI(proche(sortie_cubique(1.f), 1.f));
    V_VRAI(proche(lerpf(10.f, 20.f, 0.5f), 15.f));
    V_CMP(approcher(0.f, 10.f, 8.f, 1.f / 60.f), >, 0.f);
    V_CMP(approcher(0.f, 10.f, 8.f, 1.f / 60.f), <, 10.f);
}

static void test_couleurs(void)
{
    sfColor c = {200, 100, 50, 255};

    V_CMP(avec_alpha(c, 0.5f).a, ==, 127);
    V_CMP(avec_alpha(c, 5.f).a, ==, 255);  /* jamais de dépassement */
    V_CMP(avec_alpha(c, -1.f).a, ==, 0);
    V_CMP(melanger(c, (sfColor){0, 0, 0, 255}, 0.f).r, ==, 200);
    V_CMP(melanger(c, (sfColor){0, 0, 0, 255}, 1.f).r, ==, 0);
    V_CMP(melanger(c, (sfColor){0, 0, 0, 255}, 7.f).r, ==, 0);
}

static void test_contient_nom(void)
{
    V_VRAI(contient_nom("Kaelen", ""));
    V_VRAI(contient_nom("Kaelen", "kael"));
    V_VRAI(contient_nom("Kaelen", "LEN"));
    V_FAUX(contient_nom("Kaelen", "x"));
    V_FAUX(contient_nom("Ka", "Kaelen"));
    V_FAUX(contient_nom("", "a"));
}

static void test_classes_sures(void)
{
    V_TEXTE_EGAL(nom_classe(GUERRIER), "Guerrier");
    V_TEXTE_EGAL(nom_classe(NB_CLASSES), "Guerrier");  /* hors table : classe 0 */
    V_TEXTE_EGAL(nom_classe((classe_t)1000), "Guerrier");
    V_CMP(classe_info((classe_t)-1)->nom, ==, classes[0].nom);
    V_TEXTE_EGAL(nom_classe_maj(MAGE), "MAGE");
    V_TEXTE_EGAL(nom_classe_maj((classe_t)999), "GUERRIER");
    V_CMP(couleur_classe((classe_t)999).a, ==, 255);
    V_TEXTE_EGAL(nom_stat((stat_t)99), "POINTS DE VIE");
    V_TEXTE_EGAL(abrege_stat(STAT_ATQ), "ATQ");
    for (int i = 0; i < NB_CLASSES; i++)
        V_CMP(strlen(nom_classe_maj((classe_t)i)), <, NOM_MAX);
}

static void test_mode_arguments(void)
{
    char exe[] = "redstrings_csfml";
    char yugioh[] = "Yu-Gi-Oh!";
    char proche[] = "yu-gi-oh!";
    char sans_point[] = "Yu-Gi-Oh";
    char colle[] = "Yu-Gi-Oh!x";
    char *aucun_arg[] = {exe};
    char *bon[] = {exe, yugioh};
    char *casse[] = {exe, proche};
    char *incomplet[] = {exe, sans_point};
    char *trop_long[] = {exe, colle};
    char *deux[] = {exe, yugioh, yugioh};
    mode_visuel_t m = MODE_YUGIOH;

    V_CMP(mode_depuis_arguments(1, aucun_arg, &m), ==, 0);
    V_CMP(m, ==, MODE_NORMAL);
    V_CMP(mode_depuis_arguments(2, bon, &m), ==, 0);
    V_CMP(m, ==, MODE_YUGIOH);
    V_CMP(mode_depuis_arguments(2, casse, &m), ==, -1);
    V_CMP(m, ==, MODE_NORMAL);
    V_CMP(mode_depuis_arguments(2, incomplet, &m), ==, -1);
    V_CMP(m, ==, MODE_NORMAL);
    V_CMP(mode_depuis_arguments(2, trop_long, &m), ==, -1);
    V_CMP(m, ==, MODE_NORMAL);
    V_CMP(mode_depuis_arguments(3, deux, &m), ==, -1);
    V_CMP(m, ==, MODE_NORMAL);
}

/** Une carte par classe, chacune différente, avec des données de carte valides. */
static void test_cartes_yugioh(void)
{
    const carte_classe_t *a = NULL;

    for (int i = 0; i < NB_CLASSES; i++) {
        a = carte_de_classe((classe_t)i);
        V_CMP_BLOQUANT(a->ressource, !=, NULL);
        V_CMP(a->ressource[0], !=, '\0');
        V_CMP(a->code, >, 0);
        V_CMP((unsigned)a->cadre, <, NB_CADRES);
        V_CMP((unsigned)a->attribut, <, NB_ATTRIBUTS);
        V_CMP(a->niveau, >=, 1);
        V_CMP(a->niveau, <=, 12);
        for (int j = 0; j < i; j++) {
            V_CMP(carte_de_classe((classe_t)j)->code, !=, a->code);
            V_TEXTE_DIFF(carte_de_classe((classe_t)j)->ressource, a->ressource);
        }
    }
    V_CMP(carte_de_classe((classe_t)999), ==, carte_de_classe(GUERRIER));
    V_TEXTE_EGAL(nom_attribut((attribut_carte_t)-1), "Ténèbres");
    V_TEXTE_EGAL(nom_cadre(CADRE_XYZ), "Xyz");
}

int main(void)
{
    LANCER(test_utf8_encoder);
    LANCER(test_utf8_aller_retour);
    LANCER(test_utf8_decoder_refuse_le_invalide);
    LANCER(test_utf8_decoder_borne);
    LANCER(test_utf8_outils);
    LANCER(test_animation);
    LANCER(test_couleurs);
    LANCER(test_contient_nom);
    LANCER(test_classes_sures);
    LANCER(test_mode_arguments);
    LANCER(test_cartes_yugioh);
    tests_audio();
    tests_fonctions();
    tests_null();
    return lanceur_bilan();
}
