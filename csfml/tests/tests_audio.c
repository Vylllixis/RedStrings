/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_audio.c ──────────────────────────────────────────────────────
** │ But         Tests du cœur du système audio, sans carte son : table des
** │             événements, manifeste, repli entre thèmes, variantes,
** │             volumes, rythme des lectures, choix des voix, réglages.
** │ Contenu     un test_* par règle, lancés par tests_audio()
** │ Lancer      make csfml-tests
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "gui_sons.h"
#include "lanceur.h"
#include "tests_audio.h"

#define FICHIER_REGLAGES "tests_reglages_audio.tmp"

static void test_evenements(void)
{
    for (int i = 0; i < NB_SONS; i++) {
        const info_son_t *info = son_info((son_t)i);

        V_CMP_BLOQUANT(info, !=, NULL);
        V_CMP_BLOQUANT(info->nom, !=, NULL);
        V_CMP(son_depuis_nom(info->nom), ==, i);  // noms uniques
        V_CMP(info->priorite, >=, 1);
        V_CMP(info->priorite, <=, 3);
        V_CMP(info->max, >=, 1);
        V_CMP(info->delai, >=, 0.f);
    }
    V_CMP(son_info(NB_SONS), ==, NULL);
    V_CMP(son_depuis_nom("carte.inexistante"), ==, -1);
    V_CMP(musique_depuis_nom("musique.combat"), ==, MUSIQUE_COMBAT);
    V_TEXTE_EGAL(dossier_theme(THEME_YUGIOH), "yugioh");
}

static void test_manifeste_ligne(void)
{
    pack_t p;
    char msg[160];
    const entree_son_t *e = &p.sons[SON_CARTE_SELECTION];

    pack_vider(&p, "assets/audio/standard");
    V_CMP(pack_lire_ligne(&p, "# commentaire seul\n", msg, sizeof(msg)), ==, 0);
    V_CMP(pack_lire_ligne(&p, "   \n", msg, sizeof(msg)), ==, 0);
    V_CMP(pack_lire_ligne(&p, "carte.selection volume=0.5 hauteur=0.1 " "rare=pop_rare.ogg@0.25 pop_1.ogg pop_2.ogg # fin\n", msg, sizeof(msg)), ==, 0);
    V_CMP(e->nb, ==, 2);
    V_TEXTE_EGAL(e->fichiers[1], "pop_2.ogg");
    V_CMP(e->volume, >, 0.49f);
    V_CMP(e->volume, <, 0.51f);
    V_CMP(e->hauteur, >, 0.09f);
    V_TEXTE_EGAL(e->rare, "pop_rare.ogg");
    V_CMP(e->proba_rare, >, 0.24f);
    V_CMP(pack_lire_ligne(&p, "musique.menu volume=0.7 menu.ogg", msg, sizeof(msg)), ==, 0);
    V_TEXTE_EGAL(p.musiques[MUSIQUE_MENU].fichier, "menu.ogg");
}

static void test_manifeste_refuse(void)
{
    pack_t p;
    char msg[160];

    pack_vider(&p, "x");
    V_CMP(pack_lire_ligne(&p, "carte.inconnue a.ogg", msg, sizeof(msg)), ==, -1);
    V_CMP(pack_lire_ligne(&p, "ui.survol ../../secret.ogg", msg, sizeof(msg)), ==, -1);
    V_CMP(pack_lire_ligne(&p, "ui.survol dossier/a.ogg", msg, sizeof(msg)), ==, -1);
    V_CMP(pack_lire_ligne(&p, "ui.survol image.png", msg, sizeof(msg)), ==, -1);
    V_CMP(pack_lire_ligne(&p, "ui.survol volume=2 a.ogg", msg, sizeof(msg)), ==, -1);
    V_CMP(pack_lire_ligne(&p, "ui.survol a.ogg b.ogg c.ogg d.ogg e.ogg", msg, sizeof(msg)), ==, -1);
    V_CMP(pack_lire_ligne(&p, "ui.survol volume=0.5", msg, sizeof(msg)), ==, -1);
    V_CMP(pack_lire_ligne(&p, "musique.menu a.ogg b.ogg", msg, sizeof(msg)), ==, -1);
    V_CMP(p.sons[SON_UI_SURVOL].nb, ==, 0);  // rien d'enregistré à moitié
    V_CMP(pack_lire_ligne(&p, "ui.survol a.ogg", msg, sizeof(msg)), ==, 0);
    V_CMP(pack_lire_ligne(&p, "ui.survol b.ogg", msg, sizeof(msg)), ==, -1);
    V_TEXTE_EGAL(p.sons[SON_UI_SURVOL].fichiers[0], "a.ogg");
}

static void test_repli(void)
{
    pack_t theme;
    pack_t standard;
    const pack_t *source = NULL;
    char msg[160];

    pack_vider(&theme, "t");
    pack_vider(&standard, "s");
    pack_lire_ligne(&theme, "combat.ko ko_theme.ogg", msg, sizeof(msg));
    pack_lire_ligne(&standard, "combat.ko ko.ogg", msg, sizeof(msg));
    pack_lire_ligne(&standard, "ui.saisie touche.ogg", msg, sizeof(msg));
    V_CMP_BLOQUANT(son_resoudre(&theme, &standard, SON_COMBAT_KO, &source), !=, NULL);
    V_CMP(source, ==, &theme);  // le thème a priorité
    V_CMP_BLOQUANT(son_resoudre(&theme, &standard, SON_UI_SAISIE, &source), !=, NULL);
    V_CMP(source, ==, &standard);  // repli sur le standard
    V_CMP(son_resoudre(&theme, &standard, SON_COMBAT_SOIN, &source), ==, NULL);
    V_CMP(source, ==, NULL);  // silence, sans erreur
    V_CMP_BLOQUANT(son_resoudre(NULL, &standard, SON_COMBAT_KO, &source), !=, NULL);
}

static void test_variantes(void)
{
    entree_son_t e = {0};
    unsigned int graine = 42;
    int rare = 0;
    int prec = -1;
    int vus[SON_VARIANTES_MAX] = {0};

    e.nb = 3;
    for (int i = 0; i < 300; i++) {
        int v = variante_choisir(&e, prec, &graine, &rare);

        V_CMP(v, >=, 0);  // jamais deux fois de suite
        V_CMP(v, <, 3);  // jamais deux fois de suite
        V_CMP(v, !=, prec);  // jamais deux fois de suite
        V_FAUX(rare);  // jamais deux fois de suite
        vus[v]++;
        prec = v;
    }
    V_CMP(vus[0], >, 0);
    V_CMP(vus[1], >, 0);
    V_CMP(vus[2], >, 0);
    snprintf(e.rare, sizeof(e.rare), "rare.ogg");
    e.proba_rare = 1.f;
    variante_choisir(&e, -1, &graine, &rare);
    V_CMP(rare, ==, 1);
    e.proba_rare = 0.f;
    for (int i = 0; i < 100; i++) {
        variante_choisir(&e, -1, &graine, &rare);
        V_CMP(rare, ==, 0);
    }
}

static void test_volumes(void)
{
    reglages_audio_t r;

    reglages_defaut(&r);
    r.general = 100;
    r.effets = 100;
    r.musique = 0;
    V_CMP(volume_effectif(&r, 0, 1.f), >, 99.f);
    V_CMP(volume_effectif(&r, 1, 1.f), <, 0.001f);  // musique et effets séparés
    V_CMP(volume_effectif(&r, 0, 0.5f), >, 49.f);
    V_CMP(volume_effectif(&r, 0, 0.5f), <, 51.f);
    r.effets = 50;
    V_CMP(volume_effectif(&r, 0, 1.f), >, 24.f);
    V_CMP(volume_effectif(&r, 0, 1.f), <, 26.f);
    r.coupe = 1;
    V_CMP(volume_effectif(&r, 0, 1.f), <, 0.001f);  // coupé : silence immédiat
    r.coupe = 0;
    r.general = 250;
    V_CMP(volume_effectif(&r, 0, 1.f), <=, 100.f);
}

static void test_rythme_et_voix(void)
{
    voix_info_t v[3] = {{1, 1, 0.f}, {1, 2, 0.5f}, {1, 1, 0.2f}};

    V_VRAI(son_autorise(SON_UI_SAISIE, -100.f, 0, 1.f));
    V_FAUX(son_autorise(SON_UI_SAISIE, 1.f, 0, 1.01f));  // trop rapproché
    V_VRAI(son_autorise(SON_UI_SAISIE, 1.f, 0, 1.1f));
    V_FAUX(son_autorise(SON_COMBAT_VICTOIRE, -100.f, 1, 5.f));  // déjà en cours
    V_CMP(voix_choisir(v, 3, 1), ==, 0);  // la plus ancienne des plus faibles
    v[0].priorite = 3;
    V_CMP(voix_choisir(v, 3, 1), ==, 2);
    v[2].priorite = 3;
    v[1].priorite = 3;
    V_CMP(voix_choisir(v, 3, 2), ==, -1);  // un son important n'est pas coupé
    v[1].active = 0;
    V_CMP(voix_choisir(v, 3, 1), ==, 1);  // une voix libre d'abord
}

static void test_reglages(void)
{
    reglages_audio_t r;
    reglages_audio_t lu;
    FILE *f = NULL;

    reglages_defaut(&r);
    r.general = 33;
    r.musique = 0;
    r.coupe = 1;
    r.theme = CHOIX_THEME_YUGIOH;
    V_CMP(reglages_ecrire(&r, FICHIER_REGLAGES), ==, 0);
    reglages_defaut(&lu);
    V_CMP(reglages_lire(&lu, FICHIER_REGLAGES), ==, 0);
    V_CMP(lu.general, ==, 33);
    V_CMP(lu.musique, ==, 0);
    V_CMP(lu.coupe, ==, 1);
    V_CMP(lu.theme, ==, CHOIX_THEME_YUGIOH);
    f = fopen(FICHIER_REGLAGES, "w");
    V_CMP_BLOQUANT(f, !=, NULL);
    fprintf(f, "general=abc\neffets=999\ntheme=inconnu\nn'importe quoi\n");
    fclose(f);
    reglages_defaut(&lu);
    V_CMP(reglages_lire(&lu, FICHIER_REGLAGES), ==, 0);
    V_CMP(lu.general, ==, 80);
    V_CMP(lu.effets, ==, VOLUME_MAX);
    V_CMP(lu.theme, ==, CHOIX_THEME_AUTO);
    remove(FICHIER_REGLAGES);
    V_CMP(reglages_lire(&lu, FICHIER_REGLAGES), ==, -1);  // absent : défauts gardés
    V_CMP(theme_effectif(CHOIX_THEME_AUTO, 1), ==, THEME_YUGIOH);
    V_CMP(theme_effectif(CHOIX_THEME_AUTO, 0), ==, THEME_STANDARD);
    V_CMP(theme_effectif(CHOIX_THEME_STANDARD, 1), ==, THEME_STANDARD);
}

/* Le fichier `nom` est-il cité par le pack (bruitage, variante rare ou musique) ? */
static int cite(const pack_t *p, const char *nom)
{
    for (int s = 0; s < NB_SONS; s++) {
        for (int v = 0; v < p->sons[s].nb; v++)
            if (strcmp(p->sons[s].fichiers[v], nom) == 0)
                return 1;
        if (strcmp(p->sons[s].rare, nom) == 0)
            return 1;
    }
    for (int m = 0; m < NB_MUSIQUES; m++)
        if (strcmp(p->musiques[m].fichier, nom) == 0)
            return 1;
    return 0;
}

static void verifier_fichiers(const pack_t *p)
{
    char chemin[SON_CHEMIN_MAX];
    DIR *d = opendir(p->dossier);
    struct dirent *e = NULL;
    FILE *f = NULL;

    for (int s = 0; s < NB_SONS; s++) {
        for (int v = 0; v < p->sons[s].nb; v++) {
            V_CMP(pack_chemin(p, p->sons[s].fichiers[v], chemin, sizeof(chemin)), ==, 0);
            f = fopen(chemin, "rb");
            V_CMP_BLOQUANT(f, !=, NULL);  // chaque fichier cité existe
            fclose(f);
        }
    }
    V_CMP_BLOQUANT(d, !=, NULL);
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);

        if (n > 4 && strcmp(e->d_name + n - 4, ".ogg") == 0)
            V_VRAI(cite(p, e->d_name));  // aucun fichier oublié dans le dossier
    }
    closedir(d);
}

/* Les packs livrés : manifestes valides, fichiers présents, repli voulu. */
static void test_packs_livres(void)
{
    static pack_t standard;
    static pack_t yugioh;
    const pack_t *source = NULL;
    char msg[300];

    V_CMP(pack_charger(&standard, "assets/audio/standard", "sons.txt", msg, sizeof(msg)), ==, 0);
    V_CMP(pack_charger(&yugioh, "assets/audio/yugioh", "sons.txt", msg, sizeof(msg)), ==, 0);
    verifier_fichiers(&standard);
    verifier_fichiers(&yugioh);
    for (int s = 0; s < NB_SONS; s++) {
        V_CMP(standard.sons[s].nb, >, 0);  // le standard couvre tout : repli complet
        son_resoudre(&yugioh, &standard, (son_t)s, &source);
        V_CMP(source, ==, ((s == SON_UI_SAISIE || s == SON_UI_EFFACEMENT || s == SON_UI_VALEUR) ? &standard : &yugioh));  // Yu-Gi-Oh! a ses propres sons
    }
    for (int m = 0; m < NB_MUSIQUES; m++)
        do {
            V_CMP(standard.musiques[m].fichier[0], !=, '\0');
            V_CMP(yugioh.musiques[m].fichier[0], !=, '\0');
        } while (0);
}

void tests_audio(void)
{
    LANCER(test_evenements);
    LANCER(test_manifeste_ligne);
    LANCER(test_manifeste_refuse);
    LANCER(test_repli);
    LANCER(test_variantes);
    LANCER(test_volumes);
    LANCER(test_rythme_et_voix);
    LANCER(test_reglages);
    LANCER(test_packs_livres);
}
