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

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "gui_sons.h"
#include "tests_audio.h"

#define FICHIER_REGLAGES "tests_reglages_audio.tmp"

static void test_evenements(void)
{
    for (int i = 0; i < NB_SONS; i++) {
        const info_son_t *info = son_info((son_t)i);

        assert(info != NULL && info->nom != NULL);
        assert(son_depuis_nom(info->nom) == i);         // noms uniques
        assert(info->priorite >= 1 && info->priorite <= 3);
        assert(info->max >= 1 && info->delai >= 0.f);
    }
    assert(son_info(NB_SONS) == NULL);
    assert(son_depuis_nom("carte.inexistante") == -1);
    assert(musique_depuis_nom("musique.combat") == MUSIQUE_COMBAT);
    assert(strcmp(dossier_theme(THEME_YUGIOH), "yugioh") == 0);
}

static void test_manifeste_ligne(void)
{
    pack_t p;
    char msg[160];
    const entree_son_t *e = &p.sons[SON_CARTE_SELECTION];

    pack_vider(&p, "assets/audio/standard");
    assert(pack_lire_ligne(&p, "# commentaire seul\n", msg, sizeof(msg)) == 0);
    assert(pack_lire_ligne(&p, "   \n", msg, sizeof(msg)) == 0);
    assert(pack_lire_ligne(&p, "carte.selection volume=0.5 hauteur=0.1 "
        "rare=pop_rare.ogg@0.25 pop_1.ogg pop_2.ogg # fin\n", msg, sizeof(msg)) == 0);
    assert(e->nb == 2 && strcmp(e->fichiers[1], "pop_2.ogg") == 0);
    assert(e->volume > 0.49f && e->volume < 0.51f && e->hauteur > 0.09f);
    assert(strcmp(e->rare, "pop_rare.ogg") == 0 && e->proba_rare > 0.24f);
    assert(pack_lire_ligne(&p, "musique.menu volume=0.7 menu.ogg", msg, sizeof(msg)) == 0);
    assert(strcmp(p.musiques[MUSIQUE_MENU].fichier, "menu.ogg") == 0);
}

static void test_manifeste_refuse(void)
{
    pack_t p;
    char msg[160];

    pack_vider(&p, "x");
    assert(pack_lire_ligne(&p, "carte.inconnue a.ogg", msg, sizeof(msg)) == -1);
    assert(pack_lire_ligne(&p, "ui.survol ../../secret.ogg", msg, sizeof(msg)) == -1);
    assert(pack_lire_ligne(&p, "ui.survol dossier/a.ogg", msg, sizeof(msg)) == -1);
    assert(pack_lire_ligne(&p, "ui.survol image.png", msg, sizeof(msg)) == -1);
    assert(pack_lire_ligne(&p, "ui.survol volume=2 a.ogg", msg, sizeof(msg)) == -1);
    assert(pack_lire_ligne(&p, "ui.survol a.ogg b.ogg c.ogg d.ogg e.ogg", msg,
        sizeof(msg)) == -1);
    assert(pack_lire_ligne(&p, "ui.survol volume=0.5", msg, sizeof(msg)) == -1);
    assert(pack_lire_ligne(&p, "musique.menu a.ogg b.ogg", msg, sizeof(msg)) == -1);
    assert(p.sons[SON_UI_SURVOL].nb == 0);              // rien d'enregistré à moitié
    assert(pack_lire_ligne(&p, "ui.survol a.ogg", msg, sizeof(msg)) == 0);
    assert(pack_lire_ligne(&p, "ui.survol b.ogg", msg, sizeof(msg)) == -1);
    assert(strcmp(p.sons[SON_UI_SURVOL].fichiers[0], "a.ogg") == 0);
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
    assert(son_resoudre(&theme, &standard, SON_COMBAT_KO, &source) != NULL);
    assert(source == &theme);                           // le thème a priorité
    assert(son_resoudre(&theme, &standard, SON_UI_SAISIE, &source) != NULL);
    assert(source == &standard);                        // repli sur le standard
    assert(son_resoudre(&theme, &standard, SON_COMBAT_SOIN, &source) == NULL);
    assert(source == NULL);                             // silence, sans erreur
    assert(son_resoudre(NULL, &standard, SON_COMBAT_KO, &source) != NULL);
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

        assert(v >= 0 && v < 3 && v != prec && !rare);  // jamais deux fois de suite
        vus[v]++;
        prec = v;
    }
    assert(vus[0] > 0 && vus[1] > 0 && vus[2] > 0);
    snprintf(e.rare, sizeof(e.rare), "rare.ogg");
    e.proba_rare = 1.f;
    variante_choisir(&e, -1, &graine, &rare);
    assert(rare == 1);
    e.proba_rare = 0.f;
    for (int i = 0; i < 100; i++) {
        variante_choisir(&e, -1, &graine, &rare);
        assert(rare == 0);
    }
}

static void test_volumes(void)
{
    reglages_audio_t r;

    reglages_defaut(&r);
    r.general = 100;
    r.effets = 100;
    r.musique = 0;
    assert(volume_effectif(&r, 0, 1.f) > 99.f);
    assert(volume_effectif(&r, 1, 1.f) < 0.001f);       // musique et effets séparés
    assert(volume_effectif(&r, 0, 0.5f) > 49.f && volume_effectif(&r, 0, 0.5f) < 51.f);
    r.effets = 50;
    assert(volume_effectif(&r, 0, 1.f) > 24.f && volume_effectif(&r, 0, 1.f) < 26.f);
    r.coupe = 1;
    assert(volume_effectif(&r, 0, 1.f) < 0.001f);       // coupé : silence immédiat
    r.coupe = 0;
    r.general = 250;
    assert(volume_effectif(&r, 0, 1.f) <= 100.f);
}

static void test_rythme_et_voix(void)
{
    voix_info_t v[3] = {{1, 1, 0.f}, {1, 2, 0.5f}, {1, 1, 0.2f}};

    assert(son_autorise(SON_UI_SAISIE, -100.f, 0, 1.f));
    assert(!son_autorise(SON_UI_SAISIE, 1.f, 0, 1.01f));            // trop rapproché
    assert(son_autorise(SON_UI_SAISIE, 1.f, 0, 1.1f));
    assert(!son_autorise(SON_COMBAT_VICTOIRE, -100.f, 1, 5.f));      // déjà en cours
    assert(voix_choisir(v, 3, 1) == 0);         // la plus ancienne des plus faibles
    v[0].priorite = 3;
    assert(voix_choisir(v, 3, 1) == 2);
    v[2].priorite = 3;
    v[1].priorite = 3;
    assert(voix_choisir(v, 3, 2) == -1);        // un son important n'est pas coupé
    v[1].active = 0;
    assert(voix_choisir(v, 3, 1) == 1);         // une voix libre d'abord
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
    assert(reglages_ecrire(&r, FICHIER_REGLAGES) == 0);
    reglages_defaut(&lu);
    assert(reglages_lire(&lu, FICHIER_REGLAGES) == 0);
    assert(lu.general == 33 && lu.musique == 0 && lu.coupe == 1);
    assert(lu.theme == CHOIX_THEME_YUGIOH);
    f = fopen(FICHIER_REGLAGES, "w");
    assert(f != NULL);
    fprintf(f, "general=abc\neffets=999\ntheme=inconnu\nn'importe quoi\n");
    fclose(f);
    reglages_defaut(&lu);
    assert(reglages_lire(&lu, FICHIER_REGLAGES) == 0);
    assert(lu.general == 80 && lu.effets == VOLUME_MAX && lu.theme == CHOIX_THEME_AUTO);
    remove(FICHIER_REGLAGES);
    assert(reglages_lire(&lu, FICHIER_REGLAGES) == -1);  // absent : défauts gardés
    assert(theme_effectif(CHOIX_THEME_AUTO, 1) == THEME_YUGIOH);
    assert(theme_effectif(CHOIX_THEME_AUTO, 0) == THEME_STANDARD);
    assert(theme_effectif(CHOIX_THEME_STANDARD, 1) == THEME_STANDARD);
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
            assert(pack_chemin(p, p->sons[s].fichiers[v], chemin, sizeof(chemin)) == 0);
            f = fopen(chemin, "rb");
            assert(f != NULL);                  // chaque fichier cité existe
            fclose(f);
        }
    }
    assert(d != NULL);
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);

        if (n > 4 && strcmp(e->d_name + n - 4, ".ogg") == 0)
            assert(cite(p, e->d_name));         // aucun fichier oublié dans le dossier
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

    assert(pack_charger(&standard, "assets/audio/standard", "sons.txt", msg, sizeof(msg)) == 0);
    assert(pack_charger(&yugioh, "assets/audio/yugioh", "sons.txt", msg, sizeof(msg)) == 0);
    verifier_fichiers(&standard);
    verifier_fichiers(&yugioh);
    for (int s = 0; s < NB_SONS; s++) {
        assert(standard.sons[s].nb > 0);        // le standard couvre tout : repli complet
        son_resoudre(&yugioh, &standard, (son_t)s, &source);
        assert(source == ((s == SON_UI_SAISIE || s == SON_UI_EFFACEMENT || s == SON_UI_VALEUR)
            ? &standard : &yugioh));            // Yu-Gi-Oh! a ses propres sons
    }
    for (int m = 0; m < NB_MUSIQUES; m++)
        assert(standard.musiques[m].fichier[0] != '\0' && yugioh.musiques[m].fichier[0] != '\0');
}

void tests_audio(void)
{
    test_evenements();
    test_manifeste_ligne();
    test_manifeste_refuse();
    test_repli();
    test_variantes();
    test_volumes();
    test_rythme_et_voix();
    test_reglages();
    test_packs_livres();
}
