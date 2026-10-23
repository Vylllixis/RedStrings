/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_fonctions.c (graphique) ──────────────────────────────────────
** │ But         Un ou plusieurs tests par fonction de la version
** │             graphique qui peut se calculer sans fenêtre : valeurs
** │             exactes, bornes, symétries.
** │ Contenu     anim.c · libelles.c · cartes.c (Yu-Gi-Oh!) · ecran.c ·
** │             carte3d.c · grille.c · règles des scènes (menu, création,
** │             arène)
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "personnage.h"
#include "gui_anim.h"
#include "gui_app.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_ecran.h"
#include "gui_etat.h"
#include "gui_particules.h"
#include "gui_sons.h"
#include "gui_yugioh.h"
#include "lanceur.h"
#include "tests_fonctions.h"
#include "../src/scenes/arene/arene_interne.h"
#include "../src/scenes/creation/creation_interne.h"
#include "../src/scenes/menu/menu_interne.h"

static int egal(float a, float b)
{
    return fabsf(a - b) < 0.0005f;
}

/* ------------------------------ anim.c ----------------------------- */

static void test_bornerf(void)
{
    V_PROCHE(bornerf(5.f, 0.f, 10.f), 5.f);
    V_PROCHE(bornerf(-5.f, 0.f, 10.f), 0.f);
    V_PROCHE(bornerf(50.f, 0.f, 10.f), 10.f);
    V_PROCHE(bornerf(0.f, 0.f, 10.f), 0.f);
    V_PROCHE(bornerf(10.f, 0.f, 10.f), 10.f);
    V_PROCHE(bornerf(5.f, 3.f, 3.f), 3.f);
    V_PROCHE(bornerf(-4.f, -2.f, -1.f), -2.f);
}

static void test_lerpf(void)
{
    V_PROCHE(lerpf(0.f, 10.f, 0.f), 0.f);
    V_PROCHE(lerpf(0.f, 10.f, 1.f), 10.f);
    V_PROCHE(lerpf(0.f, 10.f, 0.5f), 5.f);
    V_PROCHE(lerpf(10.f, 0.f, 0.25f), 7.5f);
    V_PROCHE(lerpf(2.f, 2.f, 0.7f), 2.f);
    V_PROCHE(lerpf(0.f, 10.f, 2.f), 20.f);  // pas de bornage : extrapole
}

static void test_progression(void)
{
    V_PROCHE(progression(0.f, 1.f, 2.f), 0.f);  // avant le début
    V_PROCHE(progression(1.f, 1.f, 2.f), 0.f);
    V_PROCHE(progression(2.f, 1.f, 2.f), 0.5f);
    V_PROCHE(progression(3.f, 1.f, 2.f), 1.f);
    V_PROCHE(progression(99.f, 1.f, 2.f), 1.f);
    V_PROCHE(progression(1.f, 1.f, 0.f), 1.f);  // durée nulle : terminée
}

static void test_courbes(void)
{
    V_PROCHE(sortie_cubique(0.f), 0.f);
    V_PROCHE(sortie_cubique(1.f), 1.f);
    V_CMP(sortie_cubique(0.5f), >, 0.5f);  // démarre vite
    V_PROCHE(entree_cubique(0.f), 0.f);
    V_PROCHE(entree_cubique(1.f), 1.f);
    V_CMP(entree_cubique(0.5f), <, 0.5f);  // démarre lentement
    V_PROCHE(entree_sortie_cubique(0.f), 0.f);
    V_PROCHE(entree_sortie_cubique(1.f), 1.f);
    V_PROCHE(entree_sortie_cubique(0.5f), 0.5f);
    V_CMP(entree_sortie_cubique(0.25f), <, 0.25f);
    V_CMP(entree_sortie_cubique(0.75f), >, 0.75f);
    V_PROCHE(sortie_rebond(0.f), 0.f);
    V_PROCHE(sortie_rebond(1.f), 1.f);
    V_CMP(sortie_rebond(0.8f), >, 1.f);  // dépasse légèrement avant de se poser
    for (int i = 1; i <= 10; i++) {                          // les courbes d'entrée / sortie croissent
        V_CMP(sortie_cubique(i / 10.f), >=, sortie_cubique((i - 1) / 10.f));
        V_CMP(entree_sortie_cubique(i / 10.f), >=, entree_sortie_cubique((i - 1) / 10.f));
    }
}

static void test_approcher(void)
{
    V_PROCHE(approcher(0.f, 10.f, 5.f, 0.f), 0.f);  // dt nul : ne bouge pas
    V_PROCHE(approcher(0.f, 10.f, 0.f, 1.f), 0.f);  // vitesse nulle aussi
    V_CMP(approcher(0.f, 10.f, 5.f, 0.1f), >, 0.f);
    V_CMP(approcher(0.f, 10.f, 5.f, 0.1f), <, 10.f);
    V_PROCHE(approcher(0.f, 10.f, 50.f, 5.f), 10.f);  // converge vers la cible
    V_CMP(approcher(20.f, 10.f, 5.f, 0.1f), <, 20.f);  // par le haut aussi
    V_CMP(approcher(0.f, 10.f, 5.f, 0.2f), >, approcher(0.f, 10.f, 5.f, 0.1f));
    V_PROCHE(approcher(3.f, 3.f, 5.f, 1.f), 3.f);
}

static void test_ressort(void)
{
    ressort_t r = {0.f, 0.f};

    maj_ressort(&r, 10.f, 100.f, 10.f, 0.016f);
    V_CMP(r.x, >, 0.f);  // il part vers la cible
    V_CMP(r.v, >, 0.f);  // il part vers la cible
    for (int i = 0; i < 2000; i++)
        maj_ressort(&r, 10.f, 100.f, 10.f, 0.016f);
    V_PROCHE(r.x, 10.f);  // se stabilise
    V_CMP(fabsf(r.v), <, 0.01f);  // se stabilise
    r = (ressort_t){5.f, 3.f};
    maj_ressort(&r, 5.f, 100.f, 10.f, 0.f);
    V_PROCHE(r.x, 5.f);  // dt nul : inchangé
    V_PROCHE(r.v, 3.f);  // dt nul : inchangé
}

static void test_aleatoire(void)
{
    float v = 0.f;

    for (int i = 0; i < 1000; i++) {
        v = aleatoire(2.f, 5.f);
        V_CMP(v, >=, 2.f);
        V_CMP(v, <=, 5.f);
    }
    V_PROCHE(aleatoire(3.f, 3.f), 3.f);
    for (int i = 0; i < 100; i++) {
        v = aleatoire(5.f, 2.f);                                        // bornes inversées acceptées
        V_CMP(v, >=, 2.f);
        V_CMP(v, <=, 5.f);
    }
}

static void test_avec_alpha(void)
{
    sfColor c = {10, 20, 30, 200};

    V_CMP(avec_alpha(c, 0.5f).a, ==, 100);
    V_CMP(avec_alpha(c, 0.5f).r, ==, 10);
    V_CMP(avec_alpha(c, 0.f).a, ==, 0);
    V_CMP(avec_alpha(c, 1.f).a, ==, 200);
    V_CMP(avec_alpha(c, 5.f).a, ==, 255);  // borné
    V_CMP(avec_alpha(c, -1.f).a, ==, 0);
}

static void test_melanger(void)
{
    sfColor a = {0, 100, 200, 255};
    sfColor b = {100, 200, 0, 255};
    sfColor m = melanger(a, b, 0.5f);

    V_CMP(m.r, ==, 50);
    V_CMP(m.g, ==, 150);
    V_CMP(m.b, ==, 100);
    V_CMP(m.a, ==, 255);
    V_CMP(melanger(a, b, 0.f).r, ==, 0);
    V_CMP(melanger(a, b, 1.f).r, ==, 100);
    V_CMP(melanger(a, b, -3.f).r, ==, 0);  // t borné à 0..1
    V_CMP(melanger(a, b, 9.f).r, ==, 100);  // t borné à 0..1
}

static void test_griser(void)
{
    sfColor c = {255, 0, 0, 255};
    sfColor g = griser(c, 1.f);

    V_CMP(g.a, ==, 255);  // rouge éteint, tire vers le gris
    V_CMP(g.r, <, 255);  // rouge éteint, tire vers le gris
    V_CMP(g.g, >, 0);  // rouge éteint, tire vers le gris
    V_CMP(griser(c, 0.f).r, ==, 255);  // t = 0 : inchangé
    V_CMP(griser(c, 0.f).g, ==, 0);  // t = 0 : inchangé
    V_CMP(griser((sfColor){255, 255, 255, 255}, 1.f).b, >=, griser((sfColor){255, 255, 255, 255}, 1.f).r);
    V_CMP(griser((sfColor){0, 0, 0, 9}, 1.f).a, ==, 9);  // l'opacité est conservée
}

/* ---------------------------- libelles.c --------------------------- */

static void test_classe_info_et_noms(void)
{
    for (int i = 0; i < NB_CLASSES; i++) {
        V_CMP(classe_info((classe_t)i), ==, &classes[i]);
        V_TEXTE_EGAL(nom_classe((classe_t)i), classes[i].nom);
    }
    V_CMP(classe_info((classe_t)NB_CLASSES), ==, &classes[0]);  // identifiant corrompu : repli
    V_CMP(classe_info((classe_t)-3), ==, &classes[0]);
    V_TEXTE_EGAL(nom_classe((classe_t)999), classes[0].nom);
}

static void test_nom_classe_maj(void)
{
    V_TEXTE_EGAL(nom_classe_maj(GUERRIER), "GUERRIER");
    V_CMP(strncmp(nom_classe_maj(SAMOURAI), "SAMOURA", 7), ==, 0);
    V_TEXTE_EGAL(nom_classe_maj(ELEMENTALISTE), "ÉLÉMENTALISTE");  // accents capitalisés
    V_CMP(nom_classe_maj(MAGE), ==, nom_classe_maj(MAGE));  // calculé une fois
    V_TEXTE_EGAL(nom_classe_maj((classe_t)999), "GUERRIER");
    for (int i = 0; i < NB_CLASSES; i++)
        V_CMP(strlen(nom_classe_maj((classe_t)i)), >, 0);
}

static void test_couleur_classe(void)
{
    for (int i = 0; i < NB_CLASSES; i++) {
        sfColor c = couleur_classe((classe_t)i);

        V_CMP(c.r, ==, classes[i].couleur.r);
        V_CMP(c.g, ==, classes[i].couleur.g);
        V_CMP(c.b, ==, classes[i].couleur.b);
        V_CMP(c.a, ==, 255);
    }
    V_CMP(couleur_classe((classe_t)999).r, ==, classes[0].couleur.r);
}

static void test_stats_libelles(void)
{
    V_TEXTE_EGAL(nom_stat(STAT_PV), "POINTS DE VIE");
    V_TEXTE_EGAL(nom_stat(STAT_ATQ), "ATTAQUE");
    V_TEXTE_EGAL(nom_stat(STAT_DEF), "DÉFENSE");
    V_TEXTE_EGAL(nom_stat(STAT_SOIN), "SOIN");
    V_TEXTE_EGAL(abrege_stat(STAT_PV), "PV");
    V_TEXTE_EGAL(abrege_stat(STAT_ATQ), "ATQ");
    V_TEXTE_EGAL(abrege_stat(STAT_DEF), "DEF");
    V_TEXTE_EGAL(abrege_stat(STAT_SOIN), "SOI");
    V_TEXTE_EGAL(nom_stat(NB_STATS), "POINTS DE VIE");  // hors table : repli
    V_TEXTE_EGAL(abrege_stat((stat_t)-1), "PV");
    for (int s = 0; s < NB_STATS; s++)
        for (int t = s + 1; t < NB_STATS; t++)                             // une couleur par statistique
            V_CMP(memcmp(&(sfColor){couleur_stat((stat_t)s).r, couleur_stat((stat_t)s).g, couleur_stat((stat_t)s).b, 0}, &(sfColor){couleur_stat((stat_t)t).r, couleur_stat((stat_t)t).g, couleur_stat((stat_t)t).b, 0}, sizeof(sfColor)), !=, 0);
}

/* ------------------------- cartes Yu-Gi-Oh! ------------------------ */

static void test_carte_de_classe(void)
{
    for (int i = 0; i < NB_CLASSES; i++) {
        const carte_classe_t *k = carte_de_classe((classe_t)i);

        V_CMP_BLOQUANT(k, !=, NULL);
        V_CMP_BLOQUANT(k->ressource, !=, NULL);
        V_CMP(k->code, >, 0);
        V_CMP(k->niveau, >=, 1);
        V_CMP(k->niveau, <=, 12);
        V_CMP(carte_de_classe((classe_t)i), ==, k);  // table statique
    }
    V_CMP(carte_de_classe((classe_t)999), ==, carte_de_classe(GUERRIER));
}

static void test_cadres_et_attributs(void)
{
    V_TEXTE_EGAL(nom_cadre(CADRE_XYZ), "Xyz");
    for (int c = 0; c < NB_CADRES; c++) {
        V_CMP(strlen(nom_cadre((cadre_carte_t)c)), >, 0);
        V_CMP(couleur_cadre((cadre_carte_t)c, 0).a, ==, 255);
        V_CMP(couleur_cadre((cadre_carte_t)c, 1).a, ==, 255);
        (void)encre_cadre((cadre_carte_t)c);
        (void)fond_titre((cadre_carte_t)c);
    }
    for (int a = 0; a < NB_ATTRIBUTS; a++) {
        V_CMP(strlen(nom_attribut((attribut_carte_t)a)), >, 0);
        V_CMP(strlen(symbole_attribut((attribut_carte_t)a)), >, 0);
        V_CMP(couleur_attribut((attribut_carte_t)a).a, ==, 255);
    }
    V_TEXTE_EGAL(nom_attribut((attribut_carte_t)-1), "Ténèbres");  // repli documenté
    V_TEXTE_EGAL(nom_cadre((cadre_carte_t)99), nom_cadre((cadre_carte_t)-1));
}

/* ------------------------------ ecran.c ---------------------------- */

static app_t *app_vide(void)
{
    app_t *app = calloc(1, sizeof(app_t));

    V_CMP_BLOQUANT(app, !=, NULL);
    app->ecran = (sfFloatRect){0.f, 0.f, LARGEUR, HAUTEUR};
    return app;
}

static void test_ecran_visible_et_couvrant(void)
{
    app_t *app = app_vide();
    sfFloatRect v = {0};
    sfFloatRect c = {0};

    app->ecran = (sfFloatRect){-100.f, -50.f, 1480.f, 820.f};
    v = ecran_visible(app);
    V_PROCHE(v.left, -100.f);
    V_PROCHE(v.width, 1480.f);
    V_PROCHE(v.height, 820.f);
    c = ecran_couvrant(app);
    V_CMP(c.left, <, v.left);  // plus grand que l'écran
    V_CMP(c.top, <, v.top);  // plus grand que l'écran
    V_PROCHE(v.left - c.left, v.top - c.top);  // même débord partout
    V_PROCHE(c.width, v.width + 2.f * (v.left - c.left));
    V_PROCHE(c.height, v.height + 2.f * (v.top - c.top));
    free(app);
}

static void test_decalages(void)
{
    app_t *app = app_vide();

    V_PROCHE(decal_gauche(app), 0.f);  // écran 16:9 exact
    V_PROCHE(decal_droite(app), 0.f);  // écran 16:9 exact
    app->ecran = (sfFloatRect){-100.f, 0.f, LARGEUR + 200.f, HAUTEUR};        // fenêtre plus large
    V_PROCHE(decal_gauche(app), 0.f);
    V_PROCHE(decal_droite(app), 0.f);
    app->ecran = (sfFloatRect){100.f, 0.f, LARGEUR - 200.f, HAUTEUR};         // fenêtre plus étroite
    V_PROCHE(decal_gauche(app), 100.f);
    V_PROCHE(decal_droite(app), -100.f);
    free(app);
}

static void test_vers_ecran(void)
{
    app_t *app = app_vide();
    sfVector2f p = vers_ecran(app, 0.f, 0.f);

    V_PROCHE(p.x, 0.f);
    V_PROCHE(p.y, 0.f);
    p = vers_ecran(app, LARGEUR, HAUTEUR);
    V_PROCHE(p.x, LARGEUR);
    V_PROCHE(p.y, HAUTEUR);
    app->ecran = (sfFloatRect){10.f, 20.f, LARGEUR * 2.f, HAUTEUR * 2.f};
    p = vers_ecran(app, LARGEUR / 2.f, HAUTEUR / 2.f);                        // milieu de l'interface
    V_PROCHE(p.x, 10.f + LARGEUR);
    V_PROCHE(p.y, 20.f + HAUTEUR);
    free(app);
}

/* ------------------------- géométrie de la carte 3D ---------------- */

static void test_carte3d_pose(void)
{
    carte3d_t k = carte3d_pose((sfVector2f){300.f, 200.f}, 250.f);

    V_PROCHE(k.centre.x, 300.f);
    V_PROCHE(k.centre.y, 200.f);
    V_PROCHE(k.largeur, 250.f);
    V_PROCHE(k.alpha, 1.f);
    V_PROCHE(k.rot_x, 0.f);
    V_PROCHE(k.rot_y, 0.f);
    V_PROCHE(k.rot_z, 0.f);
    V_PROCHE(k.reflet, 0.f);
    V_PROCHE(k.ombre, 0.f);
    V_PROCHE(k.gris, 0.f);
}

static void test_carte3d_point(void)
{
    carte3d_t k = carte3d_pose((sfVector2f){300.f, 200.f}, 200.f);
    sfVector2f c = carte3d_point(&k, 0.5f, 0.5f);
    sfVector2f hg = carte3d_point(&k, 0.f, 0.f);
    sfVector2f bd = carte3d_point(&k, 1.f, 1.f);

    V_PROCHE(c.x, 300.f);  // le milieu est au centre
    V_PROCHE(c.y, 200.f);  // le milieu est au centre
    V_PROCHE(hg.x, 200.f);  // largeur 200, à plat
    V_PROCHE(bd.x, 400.f);  // largeur 200, à plat
    V_CMP(hg.y, <, c.y);  // symétrique verticalement
    V_CMP(bd.y, >, c.y);  // symétrique verticalement
    V_PROCHE(c.y - hg.y, bd.y - c.y);  // symétrique verticalement
    V_PROCHE((bd.y - hg.y) / (bd.x - hg.x), (float)FACE_H / FACE_L);  // proportions d'une carte
    k.rot_y = 3.14159265f;                                                    // retournée : miroir horizontal
    hg = carte3d_point(&k, 0.f, 0.5f);
    V_CMP(hg.x, >, 300.f);
    k.rot_y = 0.7f;                                                           // perspective : un bord plus petit
    V_FAUX(egal(fabsf(carte3d_point(&k, 1.f, 0.f).y - 200.f), fabsf(carte3d_point(&k, 0.f, 0.f).y - 200.f)));
}

static void test_carte3d_face_visible(void)
{
    carte3d_t k = carte3d_pose((sfVector2f){0.f, 0.f}, 100.f);

    V_VRAI(carte3d_face_visible(&k));
    k.rot_y = 3.14159265f;
    V_FAUX(carte3d_face_visible(&k));  // on voit le dos
    k.rot_y = 1.f;
    V_VRAI(carte3d_face_visible(&k));
    k.rot_y = 2.f;
    V_FAUX(carte3d_face_visible(&k));
    k.rot_y = 0.f;
    k.rot_x = 3.14159265f;
    V_FAUX(carte3d_face_visible(&k));  // basculée sur le dos
}

/* ------------------------------ grille.c --------------------------- */

static void test_grille_vers_ecran(void)
{
    grille_t g = {0};
    sfVector2f p = {0};

    g.zoom = 0.5f;
    g.pivot = (sfVector2f){100.f, 100.f};
    p = grille_vers_ecran(&g, (sfVector2f){100.f, 100.f});
    V_PROCHE(p.x, 100.f);  // le pivot ne bouge pas
    V_PROCHE(p.y, 100.f);  // le pivot ne bouge pas
    p = grille_vers_ecran(&g, (sfVector2f){300.f, 200.f});
    V_PROCHE(p.x, 200.f);  // le reste se rapproche
    V_PROCHE(p.y, 150.f);  // le reste se rapproche
    g.zoom = 0.f;
    p = grille_vers_ecran(&g, (sfVector2f){300.f, 200.f});
    V_PROCHE(p.x, 300.f);  // zoom invalide : 1
    V_PROCHE(p.y, 200.f);  // zoom invalide : 1
}

/* --------------------------- règles des scènes --------------------- */

static void test_menu_nb_ko(void)
{
    app_t *app = app_vide();
    personnage_t p = {"P", 10, 1, 1, 1, GUERRIER};

    roster_init(&app->equipe);
    V_CMP(menu_nb_ko(app), ==, 0);
    roster_ajouter(&app->equipe, &p);
    p.pv = 0;
    roster_ajouter(&app->equipe, &p);
    roster_ajouter(&app->equipe, &p);
    V_CMP(menu_nb_ko(app), ==, 2);
    roster_liberer(&app->equipe);
    free(app);
}

static void test_menu_option_principale(void)
{
    app_t *app = app_vide();
    personnage_t p = {"P", 10, 1, 1, 1, GUERRIER};

    roster_init(&app->equipe);
    V_CMP(menu_option_principale(app), ==, 0);  // ajout tant qu'il manque du monde
    roster_ajouter(&app->equipe, &p);
    V_CMP(menu_option_principale(app), ==, 0);
    roster_ajouter(&app->equipe, &p);
    V_CMP(menu_option_principale(app), ==, 2);  // le combat dès 2 personnages
    roster_liberer(&app->equipe);
    free(app);
}

static void test_menu_touche_menu(void)
{
    app_t *app = app_vide();

    app->menu.selection = 1;
    V_CMP(menu_touche_menu(app), ==, -1);  // aucune touche
    app->in.touches[sfKeyNum1] = 1;
    V_CMP(menu_touche_menu(app), ==, 0);
    app->in.touches[sfKeyNum1] = 0;
    app->in.touches[sfKeyNumpad2] = 1;
    V_CMP(menu_touche_menu(app), ==, 1);
    app->in.touches[sfKeyNumpad2] = 0;
    app->in.touches[sfKeyNum3] = 1;
    V_CMP(menu_touche_menu(app), ==, 2);
    app->in.touches[sfKeyNum3] = 0;
    app->in.touches[sfKeyEscape] = 1;
    V_CMP(menu_touche_menu(app), ==, 3);  // quitter
    free(app);
}

static void test_creation_raison_invalide(void)
{
    app_t *app = app_vide();
    creation_t c;
    personnage_t p = {"Pris", 10, 1, 1, 1, GUERRIER};

    memset(&c, 0, sizeof(c));
    roster_init(&app->equipe);
    V_CMP_BLOQUANT(strstr(creation_raison_invalide(app, &c), "nom"), !=, NULL);  // nom vide
    snprintf(c.nom.texte, sizeof(c.nom.texte), "123");
    V_CMP_BLOQUANT(strstr(creation_raison_invalide(app, &c), "nombre"), !=, NULL);
    snprintf(c.nom.texte, sizeof(c.nom.texte), "Neo");
    V_CMP(creation_raison_invalide(app, &c), ==, NULL);  // valide
    roster_ajouter(&app->equipe, &p);
    snprintf(c.nom.texte, sizeof(c.nom.texte), "Pris");
    V_CMP_BLOQUANT(strstr(creation_raison_invalide(app, &c), "déjà pris"), !=, NULL);
    snprintf(c.nom.texte, sizeof(c.nom.texte), "Neo");
    for (int i = 1; i < MAX_PERSONNAGES; i++) {
        snprintf(p.nom, sizeof(p.nom), "X%d", i);
        roster_ajouter(&app->equipe, &p);
    }
    V_CMP_BLOQUANT(strstr(creation_raison_invalide(app, &c), "maximum"), !=, NULL);  // liste pleine
    roster_liberer(&app->equipe);
    free(app);
}

static void test_creation_premiere_classe(void)
{
    classe_t c = GUERRIER;

    for (int cat = 0; cat < NB_CATEGORIES; cat++) {
        c = creation_premiere_classe((categorie_t)cat);
        V_CMP(classes[c].categorie, ==, (categorie_t)cat);
        for (int i = 0; i < (int)c; i++)                                           // la toute première de la catégorie
            V_CMP(classes[i].categorie, !=, (categorie_t)cat);
    }
    V_CMP(creation_premiere_classe((categorie_t)99), ==, GUERRIER);  // catégorie inconnue : repli
}

static void test_arene_meme_et_couleurs(void)
{
    V_VRAI(arene_meme((ref_t){0, 1}, (ref_t){0, 1}));
    V_FAUX(arene_meme((ref_t){0, 1}, (ref_t){1, 1}));
    V_FAUX(arene_meme((ref_t){0, 1}, (ref_t){0, 2}));
    V_CMP(arene_couleur_equipe(0).b, !=, arene_couleur_equipe(1).b);  // deux couleurs d'équipe distinctes
    V_CMP(arene_couleur_equipe(5).r, ==, arene_couleur_equipe(1).r);
    V_CMP(arene_couleur_action(ACTION_ATTAQUER).r, !=, arene_couleur_action(ACTION_DEFENDRE).r);
    V_CMP(arene_couleur_action(NB_ACTIONS).a, ==, arene_couleur_action(ACTION_ATTAQUER).a);  // hors table : repli
    V_CMP(arene_couleur_action(-4).r, ==, arene_couleur_action(ACTION_ATTAQUER).r);
}

static void test_arene_vis_et_ordre(void)
{
    arene_t *ar = calloc(1, sizeof(arene_t));
    personnage_t j[2] = {{"J0", 10, 1, 1, 1, GUERRIER}, {"J1", 10, 1, 1, 1, GUERRIER}};
    personnage_t a[1] = {{"A0", 10, 1, 1, 1, MAGE}};
    ref_t ordre[NB_EQUIPES * TAILLE_EQUIPE_MAX];

    V_CMP_BLOQUANT(ar, !=, NULL);
    V_CMP(bataille_init(&ar->bataille, j, 2, a, 1), ==, 0);
    V_CMP(arene_vis(ar, (ref_t){1, 0}), ==, &ar->vis[1][0]);
    V_CMP(arene_vis(ar, (ref_t){0, 1}), ==, &ar->vis[0][1]);
    ar->vis[0][0].sol.y = 300.f;
    ar->vis[0][1].sol.y = 100.f;
    ar->vis[1][0].sol.y = 200.f;
    V_CMP(arene_ordre_dessin(ar, ordre), ==, 3);  // tous les combattants
    V_VRAI(arene_meme(ordre[0], (ref_t){0, 1}));  // le plus haut à l'écran d'abord
    V_VRAI(arene_meme(ordre[1], (ref_t){1, 0}));
    V_VRAI(arene_meme(ordre[2], (ref_t){0, 0}));
    free(ar);
}

static void test_arene_echelle_monde(void)
{
    app_t *app = app_vide();

    V_PROCHE(arene_echelle_monde(app), 1.f);  // fenêtre 1280 x 720
    app->ecran = (sfFloatRect){0.f, 0.f, LARGEUR * 2.f, HAUTEUR * 2.f};
    V_PROCHE(arene_echelle_monde(app), 2.f);
    app->ecran = (sfFloatRect){0.f, 0.f, 10.f, 10.f};
    V_PROCHE(arene_echelle_monde(app), 0.5f);  // plancher à 0,5
    app->ecran = (sfFloatRect){0.f, 0.f, LARGEUR * 3.f, HAUTEUR};
    V_PROCHE(arene_echelle_monde(app), 1.f);  // le plus petit des deux rapports
    free(app);
}

/* ------------------- particules, audio, ressources ----------------- */

static void test_emettre(void)
{
    app_t *app = app_vide();
    int actives = 0;

    srand(1);
    emettre(app, (sfVector2f){100.f, 50.f}, 10, C_BLANC, 400.f, 3.f);
    for (int i = 0; i < MAX_PARTICULES; i++)
        actives += app->particules[i].vie > 0.f;
    V_CMP(actives, ==, 10);
    V_PROCHE(app->particules[0].pos.x, 100.f);
    V_CMP(app->particules[0].vie_max, >, 0.f);
    V_CMP(app->particules[0].taille, >, 0.f);
    V_CMP(app->particules[0].gravite, >, 0.f);
    emettre(app, (sfVector2f){0.f, 0.f}, 0, C_BLANC, 1.f, 1.f);              // n = 0 : rien
    emettre(app, (sfVector2f){0.f, 0.f}, -4, C_BLANC, 1.f, 1.f);
    emettre(app, (sfVector2f){0.f, 0.f}, MAX_PARTICULES * 3, C_BLANC, 1.f, 1.f);
    actives = 0;
    for (int i = 0; i < MAX_PARTICULES; i++)
        actives += app->particules[i].vie > 0.f;
    V_CMP(actives, ==, MAX_PARTICULES);  // la réserve est bornée
    free(app);
}

static void test_maj_particules(void)
{
    app_t *app = app_vide();
    float y0 = 0.f;

    srand(2);
    emettre(app, (sfVector2f){0.f, 0.f}, 1, C_BLANC, 100.f, 3.f);
    y0 = app->particules[0].pos.y;
    app->dt_jeu = 0.f;
    maj_particules(app);
    V_PROCHE(app->particules[0].pos.y, y0);  // temps arrêté : rien ne bouge
    app->dt_jeu = 0.1f;
    maj_particules(app);
    V_CMP(app->particules[0].vie, <, app->particules[0].vie_max);  // elle vieillit
    for (int i = 0; i < 100; i++)
        maj_particules(app);
    V_CMP(app->particules[0].vie, <=, 0.f);  // puis disparaît
    free(app);
}

static void test_hasard(void)
{
    unsigned int g1 = 42;
    unsigned int g2 = 42;
    unsigned int zero = 0;
    float v = 0.f;

    for (int i = 0; i < 1000; i++) {
        v = hasard(&g1);
        V_CMP(v, >=, 0.f);
        V_CMP(v, <, 1.f);
        V_PROCHE(v, hasard(&g2));  // même graine, même suite
    }
    V_CMP(hasard(&zero), >=, 0.f);  // graine nulle remplacée
    V_CMP(zero, !=, 0);  // graine nulle remplacée
    g1 = 1;
    v = hasard(&g1);
    V_FAUX(egal(v, hasard(&g1)));  // la suite avance
}

static void test_noms_audio(void)
{
    V_TEXTE_EGAL(nom_choix_theme(CHOIX_THEME_AUTO), "auto");
    V_TEXTE_EGAL(nom_choix_theme(CHOIX_THEME_STANDARD), "standard");
    V_TEXTE_EGAL(nom_choix_theme(CHOIX_THEME_YUGIOH), "yugioh");
    V_TEXTE_EGAL(nom_choix_theme((choix_theme_t)99), "auto");
    V_TEXTE_EGAL(nom_musique(MUSIQUE_MENU), "musique.menu");
    V_TEXTE_EGAL(nom_musique(MUSIQUE_COMBAT), "musique.combat");
    V_TEXTE_EGAL(nom_musique((musique_t)99), "");
    V_TEXTE_EGAL(nom_theme(THEME_YUGIOH), "Yu-Gi-Oh!");
    V_TEXTE_EGAL(nom_theme(THEME_STANDARD), "Standard");
    V_TEXTE_EGAL(nom_theme((theme_son_t)-1), "");
    for (int m = 0; m < NB_MUSIQUES; m++)
        V_CMP(musique_depuis_nom(nom_musique((musique_t)m)), ==, m);  // aller-retour nom <-> identifiant
}

static void test_audio_panneau_ouvert(void)
{
    app_t *app = app_vide();

    V_CMP(audio_panneau_ouvert(app), ==, 0);
    app->audio.panneau.ouvert = 1;
    V_CMP(audio_panneau_ouvert(app), ==, 1);
    free(app);
}

static void test_chemin_ressource(void)
{
    char chemin[512];

    V_CMP(chemin_ressource("fonts/NotoSans-Bold.ttf", chemin, sizeof(chemin)), ==, 0);
    V_CMP_BLOQUANT(strstr(chemin, "NotoSans-Bold.ttf"), !=, NULL);
    V_CMP(chemin_ressource("fonts/inexistante.ttf", chemin, sizeof(chemin)), !=, 0);
    V_CMP(chemin_ressource("fonts/NotoSans-Bold.ttf", chemin, 8), !=, 0);  // tampon trop petit : refusé
}

void tests_fonctions(void)
{
    LANCER(test_bornerf);
    LANCER(test_lerpf);
    LANCER(test_progression);
    LANCER(test_courbes);
    LANCER(test_approcher);
    LANCER(test_ressort);
    LANCER(test_aleatoire);
    LANCER(test_avec_alpha);
    LANCER(test_melanger);
    LANCER(test_griser);
    LANCER(test_classe_info_et_noms);
    LANCER(test_nom_classe_maj);
    LANCER(test_couleur_classe);
    LANCER(test_stats_libelles);
    LANCER(test_carte_de_classe);
    LANCER(test_cadres_et_attributs);
    LANCER(test_ecran_visible_et_couvrant);
    LANCER(test_decalages);
    LANCER(test_vers_ecran);
    LANCER(test_carte3d_pose);
    LANCER(test_carte3d_point);
    LANCER(test_carte3d_face_visible);
    LANCER(test_grille_vers_ecran);
    LANCER(test_menu_nb_ko);
    LANCER(test_menu_option_principale);
    LANCER(test_menu_touche_menu);
    LANCER(test_creation_raison_invalide);
    LANCER(test_creation_premiere_classe);
    LANCER(test_arene_meme_et_couleurs);
    LANCER(test_arene_vis_et_ordre);
    LANCER(test_arene_echelle_monde);
    LANCER(test_emettre);
    LANCER(test_maj_particules);
    LANCER(test_hasard);
    LANCER(test_noms_audio);
    LANCER(test_audio_panneau_ouvert);
    LANCER(test_chemin_ressource);
}
