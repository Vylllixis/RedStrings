/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Tests des fonctions de la version graphique qui ne touchent pas à SFML :
 * UTF-8, courbes d'animation, couleurs, recherche par nom et accès sûrs à la
 * table des classes. Aucune fenêtre n'est ouverte.
 */

#include <assert.h>
#include <math.h>
#include <string.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_carte.h"
#include "gui_jeu.h"
#include "gui_utf8.h"
#include "gui_yugioh.h"

static int proche(float a, float b)
{
    return fabsf(a - b) < 0.0001f;
}

static void test_utf8_encoder(void)
{
    char o[4];

    assert(utf8_encoder('A', o) == 1 && o[0] == 'A');
    assert(utf8_encoder(0xE9, o) == 2 && (unsigned char)o[0] == 0xC3 && (unsigned char)o[1] == 0xA9);
    assert(utf8_encoder(0x20AC, o) == 3);
    assert(utf8_encoder(0x1F600, o) == 4);
    assert(utf8_encoder(0xD800, o) == 0);       /* substitut */
    assert(utf8_encoder(0xDFFF, o) == 0);
    assert(utf8_encoder(0x110000, o) == 0);     /* hors Unicode */
    assert(utf8_encoder(0xFFFFFFFF, o) == 0);
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
        assert(n > 0);
        assert(utf8_decoder(o, d, 4) == 1 && d[0] == points[i] && d[1] == 0);
    }
}

static void test_utf8_decoder_refuse_le_invalide(void)
{
    uint32_t d[8];

    assert(utf8_decoder("é", d, 8) == 1 && d[0] == 0xE9);
    assert(utf8_decoder("\xC3", d, 8) == 0);            /* séquence tronquée */
    assert(utf8_decoder("\xE2\x82", d, 8) == 0);
    assert(utf8_decoder("\xC0\x80", d, 8) == 0);        /* codage trop long du NUL */
    assert(utf8_decoder("\xE0\x80\x80", d, 8) == 0);
    assert(utf8_decoder("\xED\xA0\x80", d, 8) == 0);    /* substitut UTF-16 */
    assert(utf8_decoder("\xF4\x90\x80\x80", d, 8) == 0);/* au-dessus de U+10FFFF */
    assert(utf8_decoder("\x80", d, 8) == 0);            /* octet de suite isolé */
    assert(utf8_decoder("\xFF", d, 8) == 0);
    assert(utf8_decoder("a\x80" "b", d, 8) == 2 && d[0] == 'a' && d[1] == 'b');
}

static void test_utf8_decoder_borne(void)
{
    uint32_t d[4] = {9, 9, 9, 9};

    assert(utf8_decoder("abcdef", d, 4) == 3);          /* max - 1 : place pour le 0 final */
    assert(d[3] == 0);
    assert(utf8_decoder("abc", d, 1) == 0 && d[0] == 0);
    assert(utf8_decoder("abc", d, 0) == 0);
    assert(utf8_decoder("", d, 4) == 0 && d[0] == 0);
}

static void test_utf8_outils(void)
{
    char s[16] = "Prêtre";
    char v[16] = "";

    assert(utf8_nb_caracteres("Prêtre") == 6);
    assert(utf8_nb_caracteres("") == 0);
    utf8_supprimer_dernier(s);
    assert(strcmp(s, "Prêtr") == 0);
    strcpy(s, "é");
    utf8_supprimer_dernier(s);
    assert(s[0] == '\0');
    utf8_supprimer_dernier(v);                          /* chaîne vide : rien ne se passe */
    assert(v[0] == '\0');
    strcpy(s, "Élémentalis");
    utf8_majuscules(s);
    assert(strcmp(s, "ÉLÉMENTALIS") == 0);
}

static void test_animation(void)
{
    assert(proche(bornerf(5.f, 0.f, 1.f), 1.f));
    assert(proche(bornerf(-5.f, 0.f, 1.f), 0.f));
    assert(proche(progression(0.f, 1.f, 2.f), 0.f));
    assert(proche(progression(2.f, 1.f, 2.f), 0.5f));
    assert(proche(progression(9.f, 1.f, 2.f), 1.f));
    assert(proche(sortie_cubique(0.f), 0.f) && proche(sortie_cubique(1.f), 1.f));
    assert(proche(lerpf(10.f, 20.f, 0.5f), 15.f));
    assert(approcher(0.f, 10.f, 8.f, 1.f / 60.f) > 0.f);
    assert(approcher(0.f, 10.f, 8.f, 1.f / 60.f) < 10.f);
}

static void test_couleurs(void)
{
    sfColor c = {200, 100, 50, 255};

    assert(avec_alpha(c, 0.5f).a == 127);
    assert(avec_alpha(c, 5.f).a == 255);                /* jamais de dépassement */
    assert(avec_alpha(c, -1.f).a == 0);
    assert(melanger(c, (sfColor){0, 0, 0, 255}, 0.f).r == 200);
    assert(melanger(c, (sfColor){0, 0, 0, 255}, 1.f).r == 0);
    assert(melanger(c, (sfColor){0, 0, 0, 255}, 7.f).r == 0);
}

static void test_contient_nom(void)
{
    assert(contient_nom("Kaelen", ""));
    assert(contient_nom("Kaelen", "kael"));
    assert(contient_nom("Kaelen", "LEN"));
    assert(!contient_nom("Kaelen", "x"));
    assert(!contient_nom("Ka", "Kaelen"));
    assert(!contient_nom("", "a"));
}

static void test_classes_sures(void)
{
    assert(strcmp(nom_classe(GUERRIER), "Guerrier") == 0);
    assert(strcmp(nom_classe(NB_CLASSES), "Guerrier") == 0);        /* hors table : classe 0 */
    assert(strcmp(nom_classe((classe_t)1000), "Guerrier") == 0);
    assert(classe_info((classe_t)-1)->nom == classes[0].nom);
    assert(strcmp(nom_classe_maj(MAGE), "MAGE") == 0);
    assert(strcmp(nom_classe_maj((classe_t)999), "GUERRIER") == 0);
    assert(couleur_classe((classe_t)999).a == 255);
    assert(strcmp(nom_stat((stat_t)99), "POINTS DE VIE") == 0);
    assert(strcmp(abrege_stat(STAT_ATQ), "ATQ") == 0);
    for (int i = 0; i < NB_CLASSES; i++)
        assert(strlen(nom_classe_maj((classe_t)i)) < NOM_MAX);
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

    assert(mode_depuis_arguments(1, aucun_arg, &m) == 0 && m == MODE_NORMAL);
    assert(mode_depuis_arguments(2, bon, &m) == 0 && m == MODE_YUGIOH);
    assert(mode_depuis_arguments(2, casse, &m) == -1 && m == MODE_NORMAL);
    assert(mode_depuis_arguments(2, incomplet, &m) == -1 && m == MODE_NORMAL);
    assert(mode_depuis_arguments(2, trop_long, &m) == -1 && m == MODE_NORMAL);
    assert(mode_depuis_arguments(3, deux, &m) == -1 && m == MODE_NORMAL);
}

/** Une carte par classe, chacune différente, avec des données de carte valides. */
static void test_cartes_yugioh(void)
{
    const carte_classe_t *a = NULL;

    for (int i = 0; i < NB_CLASSES; i++) {
        a = carte_de_classe((classe_t)i);
        assert(a->ressource != NULL && a->ressource[0] != '\0');
        assert(a->code > 0);
        assert((unsigned)a->cadre < NB_CADRES && (unsigned)a->attribut < NB_ATTRIBUTS);
        assert(a->niveau >= 1 && a->niveau <= 12);
        for (int j = 0; j < i; j++) {
            assert(carte_de_classe((classe_t)j)->code != a->code);
            assert(strcmp(carte_de_classe((classe_t)j)->ressource, a->ressource) != 0);
        }
    }
    assert(carte_de_classe((classe_t)999) == carte_de_classe(GUERRIER));
    assert(strcmp(nom_attribut((attribut_carte_t)-1), "Ténèbres") == 0);
    assert(strcmp(nom_cadre(CADRE_XYZ), "Xyz") == 0);
}

int main(void)
{
    test_utf8_encoder();
    test_utf8_aller_retour();
    test_utf8_decoder_refuse_le_invalide();
    test_utf8_decoder_borne();
    test_utf8_outils();
    test_animation();
    test_couleurs();
    test_contient_nom();
    test_classes_sures();
    test_mode_arguments();
    test_cartes_yugioh();
    return 0;
}
