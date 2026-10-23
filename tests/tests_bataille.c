/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_bataille.c ───────────────────────────────────────────────────
** │ But         Un ou plusieurs tests par fonction du moteur de combat :
** │             état initial, accès aux combattants, dégâts, soin, cibles
** │             valides, résolution de chaque action, journal, tours,
** │             effets, victoire, ordinateur.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <string.h>
#include "bataille.h"
#include "lanceur.h"
#include "tests_bataille.h"

static personnage_t p(const char *nom, int pv, int atq, int def, int soin, classe_t c)
{
    personnage_t x = {0};

    snprintf(x.nom, sizeof(x.nom), "%s", nom);
    x.pv = pv;
    x.atq = atq;
    x.def = def;
    x.soin = soin;
    x.classe = c;
    return x;
}

/* Deux équipes de 2 : joueurs A0 A1, adversaires B0 B1. */
static void duel_2v2(bataille_t *b)
{
    personnage_t j[2] = {p("A0", 100, 40, 0, 30, GUERRIER), p("A1", 80, 20, 50, 25, PRETRE)};
    personnage_t a[2] = {p("B0", 90, 30, 0, 10, MAGE), p("B1", 60, 50, 10, 0, ARCHER)};

    V_CMP(bataille_init(b, j, 2, a, 2), ==, 0);
}

static void test_init_etat(void)
{
    bataille_t b;
    personnage_t j = p("J", 70, 10, 5, 3, GUERRIER);
    personnage_t a = p("A", 50, 10, 5, 3, MAGE);

    V_CMP(bataille_init(&b, &j, 1, &a, 1), ==, 0);
    V_CMP(b.gagnant, ==, -1);
    V_CMP(b.tour, ==, 0);
    V_CMP(b.manche, ==, 0);
    V_CMP(b.nb_ordre, ==, 2);
    V_CMP(b.equipes[0].taille, ==, 1);
    V_CMP(b.equipes[1].taille, ==, 1);
    V_CMP(b.equipes[0].membres[0].pv, ==, 70);
    V_CMP(b.equipes[0].membres[0].pv_max, ==, 70);
    V_CMP(b.equipes[1].membres[0].pv, ==, 50);
    V_CMP(b.equipes[1].membres[0].nb_effets, ==, 0);
    V_CMP(b.equipes[0].membres[0].defensifs, ==, 0);
    V_TEXTE_EGAL(b.equipes[0].membres[0].perso.nom, "J");
    V_CMP(b.nb_journal, ==, 1);
    V_CMP_BLOQUANT(strstr(bataille_journal(&b, 0), "1 contre 1"), !=, NULL);
}

static void test_init_refus(void)
{
    bataille_t b;
    personnage_t ok = p("Ok", 10, 1, 1, 1, GUERRIER);
    personnage_t neg = p("Neg", -5, 1, 1, 1, GUERRIER);
    personnage_t grand[TAILLE_EQUIPE_MAX + 1];

    for (int i = 0; i <= TAILLE_EQUIPE_MAX; i++)
        grand[i] = ok;
    V_CMP(bataille_init(&b, &ok, 1, &neg, 1), ==, -1);  // PV négatifs
    V_CMP(bataille_init(&b, grand, TAILLE_EQUIPE_MAX, grand, TAILLE_EQUIPE_MAX), ==, 0);  // équipe pleine
    V_CMP(bataille_init(&b, &ok, 1, grand, TAILLE_EQUIPE_MAX + 1), ==, -1);
    V_CMP(bataille_init(&b, &ok, 1, &ok, -1), ==, -1);
}

static void test_init_copie_les_personnages(void)
{
    bataille_t b;
    personnage_t j = p("J", 70, 10, 5, 3, GUERRIER);
    personnage_t a = p("A", 50, 10, 5, 3, MAGE);
    evenement_t ev[MAX_EVENEMENTS];

    V_CMP(bataille_init(&b, &j, 1, &a, 1), ==, 0);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev), >=, 1);
    V_CMP(a.pv, ==, 50);  // les originaux ne bougent pas
    V_CMP(j.pv, ==, 70);  // les originaux ne bougent pas
}

static void test_ordre_equipes_inegales(void)
{
    bataille_t b;
    personnage_t j[3] = {p("J0", 9, 1, 0, 0, GUERRIER), p("J1", 9, 1, 0, 0, GUERRIER), p("J2", 9, 1, 0, 0, GUERRIER)};
    personnage_t a[1] = {p("A0", 9, 1, 0, 0, MAGE)};

    V_CMP(bataille_init(&b, j, 3, a, 1), ==, 0);
    V_CMP(b.nb_ordre, ==, 4);
    V_CMP(b.ordre[0].equipe, ==, 0);
    V_CMP(b.ordre[0].index, ==, 0);
    V_CMP(b.ordre[1].equipe, ==, 1);
    V_CMP(b.ordre[1].index, ==, 0);
    V_CMP(b.ordre[2].equipe, ==, 0);
    V_CMP(b.ordre[2].index, ==, 1);
    V_CMP(b.ordre[3].equipe, ==, 0);  // l'équipe de 1 n'a plus de tour
    V_CMP(b.ordre[3].index, ==, 2);  // l'équipe de 1 n'a plus de tour
}

static void test_actif(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];

    duel_2v2(&b);
    V_CMP(bataille_actif(&b).equipe, ==, 0);
    V_CMP(bataille_actif(&b).index, ==, 0);
    bataille_agir(&b, ACTION_DEFENDRE, (ref_t){0, 0}, ev);
    V_CMP(bataille_actif(&b).equipe, ==, 1);
    V_CMP(bataille_actif(&b).index, ==, 0);
    bataille_agir(&b, ACTION_DEFENDRE, (ref_t){1, 0}, ev);
    V_CMP(bataille_actif(&b).equipe, ==, 0);
    V_CMP(bataille_actif(&b).index, ==, 1);
    b.tour = -1;
    V_CMP(bataille_actif(&b).equipe, ==, -1);  // état corrompu : référence invalide
    b.tour = b.nb_ordre;
    V_CMP(bataille_actif(&b).index, ==, -1);
}

static void test_lire_et_combattant(void)
{
    bataille_t b;
    const combattant_t *lu = NULL;
    combattant_t *ecr = NULL;

    duel_2v2(&b);
    lu = bataille_lire(&b, (ref_t){1, 1});
    V_CMP_BLOQUANT(lu, !=, NULL);
    V_TEXTE_EGAL(lu->perso.nom, "B1");
    ecr = bataille_combattant(&b, (ref_t){1, 1});
    V_CMP(ecr, ==, lu);  // même combattant
    ecr->pv = 7;
    V_CMP(bataille_lire(&b, (ref_t){1, 1})->pv, ==, 7);
    V_CMP(bataille_lire(&b, (ref_t){0, 2}), ==, NULL);  // équipe de 2 : l'index 2 n'existe pas
    V_CMP(bataille_lire(&b, (ref_t){2, 0}), ==, NULL);
    V_CMP(bataille_lire(&b, (ref_t){0, -1}), ==, NULL);
    V_CMP(bataille_combattant(&b, (ref_t){-1, 0}), ==, NULL);
}

static void test_vivant_et_garde(void)
{
    bataille_t b;
    combattant_t *c = NULL;

    duel_2v2(&b);
    c = bataille_combattant(&b, (ref_t){0, 0});
    V_VRAI(combattant_vivant(c));
    V_FAUX(combattant_en_garde(c));
    c->pv = 0;
    V_FAUX(combattant_vivant(c));
    c->pv = 1;
    V_VRAI(combattant_vivant(c));
    c->effets[0] = (effet_t){EFFET_GARDE, 1};
    c->nb_effets = 1;
    V_VRAI(combattant_en_garde(c));
    c->nb_effets = 0;
    V_FAUX(combattant_en_garde(c));
}

static void test_degats(void)
{
    bataille_t b;
    const combattant_t *a0 = NULL;
    combattant_t *a1 = NULL;

    duel_2v2(&b);
    a0 = bataille_lire(&b, (ref_t){0, 0});
    a1 = bataille_combattant(&b, (ref_t){0, 1});                    // def 50
    V_CMP(bataille_degats(a0, bataille_lire(&b, (ref_t){1, 0})), ==, 40);  // def 0
    V_CMP(bataille_degats(a0, a1), ==, 20);  // 40 x 50 %
    a1->effets[0] = (effet_t){EFFET_GARDE, 1};
    a1->nb_effets = 1;
    V_CMP(bataille_degats(a0, a1), ==, 10);  // garde : encore -50 %
    a1->perso.def = 100;
    V_CMP(bataille_degats(a0, a1), ==, 1);  // minimum 1
}

static void test_soin(void)
{
    bataille_t b;
    const combattant_t *pretre = NULL;
    combattant_t *c = NULL;

    duel_2v2(&b);
    pretre = bataille_lire(&b, (ref_t){0, 1});                      // soin 25
    c = bataille_combattant(&b, (ref_t){0, 0});
    V_CMP(bataille_soin(pretre, c), ==, 0);  // en pleine forme
    c->pv = 90;
    V_CMP(bataille_soin(pretre, c), ==, 10);  // plafonné à ce qui manque
    c->pv = 50;
    V_CMP(bataille_soin(pretre, c), ==, 25);  // plafonné au soin
    c->pv = 0;
    V_CMP(bataille_soin(pretre, c), ==, 25);  // (la validité est vérifiée ailleurs)
    V_CMP(bataille_soin(bataille_lire(&b, (ref_t){1, 1}), c), ==, 0);  // B1 : soin 0
}

static void test_cible_valide_matrice(void)
{
    bataille_t b;

    duel_2v2(&b);                                                   // actif : A0
    V_FAUX(bataille_cible_valide(&b, ACTION_ATTAQUER, (ref_t){0, 0}));
    V_FAUX(bataille_cible_valide(&b, ACTION_ATTAQUER, (ref_t){0, 1}));
    V_VRAI(bataille_cible_valide(&b, ACTION_ATTAQUER, (ref_t){1, 0}));
    V_VRAI(bataille_cible_valide(&b, ACTION_ATTAQUER, (ref_t){1, 1}));
    V_FAUX(bataille_cible_valide(&b, ACTION_DEFENDRE, (ref_t){1, 0}));
    V_FAUX(bataille_cible_valide(&b, ACTION_DEFENDRE, (ref_t){0, 1}));
    V_VRAI(bataille_cible_valide(&b, ACTION_DEFENDRE, (ref_t){0, 0}));  // soi-même seulement
    V_FAUX(bataille_cible_valide(&b, ACTION_SOIGNER, (ref_t){0, 1}));  // en pleine forme
    bataille_combattant(&b, (ref_t){0, 1})->pv = 10;
    V_VRAI(bataille_cible_valide(&b, ACTION_SOIGNER, (ref_t){0, 1}));
    V_FAUX(bataille_cible_valide(&b, ACTION_SOIGNER, (ref_t){1, 1}));  // pas un adversaire
    bataille_combattant(&b, (ref_t){1, 1})->pv = 0;
    V_FAUX(bataille_cible_valide(&b, ACTION_ATTAQUER, (ref_t){1, 1}));  // déjà K.O.
    V_FAUX(bataille_cible_valide(&b, (action_t)NB_ACTIONS, (ref_t){1, 0}));
    V_FAUX(bataille_cible_valide(&b, (action_t)-1, (ref_t){1, 0}));
    b.gagnant = 0;
    V_FAUX(bataille_cible_valide(&b, ACTION_ATTAQUER, (ref_t){1, 0}));  // combat fini
}

static void test_cibles_liste(void)
{
    bataille_t b;
    ref_t c[NB_EQUIPES * TAILLE_EQUIPE_MAX];

    duel_2v2(&b);
    V_CMP(bataille_cibles(&b, ACTION_ATTAQUER, c), ==, 2);
    V_CMP(c[0].equipe, ==, 1);
    V_CMP(c[0].index, ==, 0);
    V_CMP(c[1].index, ==, 1);
    V_CMP(bataille_cibles(&b, ACTION_DEFENDRE, c), ==, 1);
    V_CMP(c[0].equipe, ==, 0);
    V_CMP(c[0].index, ==, 0);
    V_CMP(bataille_cibles(&b, ACTION_SOIGNER, c), ==, 0);
    bataille_combattant(&b, (ref_t){0, 0})->pv = 5;
    bataille_combattant(&b, (ref_t){0, 1})->pv = 5;
    V_CMP(bataille_cibles(&b, ACTION_SOIGNER, c), ==, 2);  // soi-même compris
    bataille_combattant(&b, (ref_t){1, 0})->pv = 0;
    V_CMP(bataille_cibles(&b, ACTION_ATTAQUER, c), ==, 1);
    V_CMP(c[0].index, ==, 1);
}

static void test_action_possible(void)
{
    bataille_t b;

    duel_2v2(&b);
    V_VRAI(bataille_action_possible(&b, ACTION_ATTAQUER));
    V_VRAI(bataille_action_possible(&b, ACTION_DEFENDRE));
    V_FAUX(bataille_action_possible(&b, ACTION_SOIGNER));
    bataille_combattant(&b, (ref_t){0, 0})->pv = 1;
    V_VRAI(bataille_action_possible(&b, ACTION_SOIGNER));
    b.gagnant = 1;
    V_FAUX(bataille_action_possible(&b, ACTION_ATTAQUER));
}

static void test_agir_refus_ne_change_rien(void)
{
    bataille_t b;
    bataille_t avant;
    evenement_t ev[MAX_EVENEMENTS];

    duel_2v2(&b);
    avant = b;
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 1}, ev), ==, -1);  // allié
    V_CMP(bataille_agir(&b, ACTION_SOIGNER, (ref_t){0, 1}, ev), ==, -1);  // personne à soigner
    V_CMP(bataille_agir(&b, ACTION_DEFENDRE, (ref_t){1, 0}, ev), ==, -1);  // pas soi
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){9, 9}, ev), ==, -1);
    V_CMP(memcmp(&b, &avant, sizeof(b)), ==, 0);  // aucun effet de bord
}

static void test_agir_attaque_evenements(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];

    duel_2v2(&b);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev), ==, 1);
    V_CMP(ev[0].type, ==, EV_ATTAQUE);
    V_CMP(ev[0].valeur, ==, 40);
    V_CMP(ev[0].pv_apres, ==, 50);
    V_CMP(ev[0].garde, ==, 0);
    V_CMP(ev[0].acteur.equipe, ==, 0);
    V_CMP(ev[0].acteur.index, ==, 0);
    V_CMP(ev[0].cible.equipe, ==, 1);
    V_CMP_BLOQUANT(strstr(bataille_journal(&b, 0), "A0 attaque B0"), !=, NULL);
    V_CMP_BLOQUANT(strstr(bataille_journal(&b, 0), "40 dégâts"), !=, NULL);
}

static void test_agir_soin_evenements(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];

    duel_2v2(&b);
    bataille_combattant(&b, (ref_t){0, 1})->pv = 10;
    bataille_combattant(&b, (ref_t){0, 0})->pv = 10;
    V_CMP(bataille_agir(&b, ACTION_SOIGNER, (ref_t){0, 1}, ev), ==, 1);  // A0 (soin 30) soigne A1
    V_CMP(ev[0].type, ==, EV_SOIN);
    V_CMP(ev[0].valeur, ==, 30);
    V_CMP(ev[0].pv_apres, ==, 40);
    V_CMP_BLOQUANT(strstr(bataille_journal(&b, 0), "A0 soigne A1 de 30"), !=, NULL);
    bataille_agir(&b, ACTION_DEFENDRE, (ref_t){1, 0}, ev);
    V_CMP(bataille_agir(&b, ACTION_SOIGNER, (ref_t){0, 1}, ev), ==, 1);  // A1 se soigne lui-même
    V_CMP_BLOQUANT(strstr(bataille_journal(&b, 0), "A1 se soigne"), !=, NULL);
}

static void test_agir_defense_et_expiration(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    int n = 0;

    duel_2v2(&b);
    V_CMP(bataille_agir(&b, ACTION_DEFENDRE, (ref_t){0, 0}, ev), ==, 1);
    V_CMP(ev[0].type, ==, EV_GARDE);
    V_CMP(ev[0].valeur, ==, GARDE_REDUCTION);
    V_VRAI(combattant_en_garde(bataille_lire(&b, (ref_t){0, 0})));
    bataille_agir(&b, ACTION_DEFENDRE, (ref_t){1, 0}, ev);                      // B0
    bataille_agir(&b, ACTION_DEFENDRE, (ref_t){0, 1}, ev);                      // A1
    n = bataille_agir(&b, ACTION_DEFENDRE, (ref_t){1, 1}, ev);                  // B1, puis retour à A0
    V_CMP(n, ==, 2);  // la garde de A0 expire
    V_CMP(ev[1].type, ==, EV_GARDE_FIN);  // la garde de A0 expire
    V_CMP(ev[1].acteur.index, ==, 0);  // la garde de A0 expire
    V_FAUX(combattant_en_garde(bataille_lire(&b, (ref_t){0, 0})));
    V_CMP(b.manche, ==, 1);
}

static void test_defensifs_compteur(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];

    duel_2v2(&b);
    bataille_agir(&b, ACTION_DEFENDRE, (ref_t){0, 0}, ev);
    V_CMP(bataille_lire(&b, (ref_t){0, 0})->defensifs, ==, 1);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev), ==, -1);  // refus : B0 ne s'attaque pas lui-même
    V_CMP(bataille_lire(&b, (ref_t){0, 0})->defensifs, ==, 1);
    bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 0}, ev);                      // B0 attaque A0
    V_CMP(bataille_lire(&b, (ref_t){1, 0})->defensifs, ==, 0);  // une attaque remet à zéro
}

static void test_tour_saute_les_ko(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];

    duel_2v2(&b);
    bataille_combattant(&b, (ref_t){1, 0})->pv = 0;                             // B0 est déjà K.O.
    bataille_agir(&b, ACTION_DEFENDRE, (ref_t){0, 0}, ev);
    V_CMP(bataille_actif(&b).equipe, ==, 0);  // B0 est sauté
    V_CMP(bataille_actif(&b).index, ==, 1);  // B0 est sauté
    bataille_combattant(&b, (ref_t){0, 1})->pv = 0;
    V_CMP(bataille_agir(&b, ACTION_DEFENDRE, (ref_t){0, 1}, ev), ==, -1);  // le combattant actif (A1) est K.O.
}

static void test_victoire_et_blocage(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j = p("J", 50, 99, 0, 0, GUERRIER);
    personnage_t a = p("A", 10, 1, 0, 0, MAGE);
    int n = 0;

    V_CMP(bataille_init(&b, &j, 1, &a, 1), ==, 0);
    n = bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev);
    V_CMP(n, ==, 3);
    V_CMP(ev[0].type, ==, EV_ATTAQUE);
    V_CMP(ev[1].type, ==, EV_KO);
    V_CMP(ev[2].type, ==, EV_VICTOIRE);
    V_CMP(b.gagnant, ==, 0);
    V_CMP(ev[2].valeur, ==, 0);
    V_CMP_BLOQUANT(strstr(bataille_journal(&b, 0), "vaincue"), !=, NULL);
    V_CMP(bataille_agir(&b, ACTION_ATTAQUER, (ref_t){1, 0}, ev), ==, -1);  // plus rien à jouer
    V_CMP(bataille_agir(&b, ACTION_DEFENDRE, (ref_t){0, 0}, ev), ==, -1);
}

static void test_defaite(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j = p("J", 5, 1, 0, 0, GUERRIER);
    personnage_t a = p("A", 50, 99, 0, 0, MAGE);

    V_CMP(bataille_init(&b, &j, 1, &a, 1), ==, 0);
    bataille_agir(&b, ACTION_DEFENDRE, (ref_t){0, 0}, ev);
    bataille_agir(&b, ACTION_ATTAQUER, (ref_t){0, 0}, ev);
    V_CMP(b.gagnant, ==, 1);
    V_CMP_BLOQUANT(strstr(bataille_journal(&b, 0), "Ton équipe"), !=, NULL);
}

static void test_journal(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t j = p("J", 99, 1, 0, 0, GUERRIER);
    personnage_t a = p("A", 99, 1, 0, 0, MAGE);

    V_CMP(bataille_init(&b, &j, 1, &a, 1), ==, 0);
    V_CMP(bataille_journal(&b, 1), ==, NULL);
    V_CMP_BLOQUANT(bataille_journal(&b, 0), !=, NULL);
    for (int i = 0; i < 20; i++)
        bataille_agir(&b, ACTION_DEFENDRE, (ref_t){i % 2, 0}, ev);
    V_CMP(b.nb_journal, >, JOURNAL_MAX);
    for (int i = 0; i < JOURNAL_MAX; i++)
        V_CMP_BLOQUANT(bataille_journal(&b, i), !=, NULL);
    V_CMP(bataille_journal(&b, JOURNAL_MAX), ==, NULL);  // tampon circulaire borné
    V_CMP(bataille_journal(&b, -1), ==, NULL);
    V_CMP_BLOQUANT(strstr(bataille_journal(&b, 0), "garde"), !=, NULL);  // la plus récente en premier
    for (int i = 0; i < JOURNAL_MAX; i++)
        V_CMP(strlen(bataille_journal(&b, i)), <, JOURNAL_LIGNE);
}

static void test_ia_achever(void)
{
    bataille_t b;
    action_t a = ACTION_DEFENDRE;
    ref_t c = {0, 0};

    duel_2v2(&b);
    b.tour = 1;                                                                 // B0 joue
    bataille_combattant(&b, (ref_t){0, 1})->pv = 5;                             // A1 : 1 coup suffit
    bataille_combattant(&b, (ref_t){1, 0})->pv = 10;                            // B0 est très blessé
    for (int i = 0; i < 50; i++) {
        bataille_ia(&b, &a, &c);
        V_CMP(a, ==, ACTION_ATTAQUER);  // il achève, sans hésiter
        V_CMP(c.equipe, ==, 0);  // il achève, sans hésiter
        V_CMP(c.index, ==, 1);  // il achève, sans hésiter
    }
}

static void test_ia_soigne_et_se_garde(void)
{
    bataille_t b;
    action_t a = ACTION_ATTAQUER;
    ref_t c = {0, 0};
    personnage_t j = p("J", 500, 1, 99, 0, GUERRIER);                           // trop solide pour être achevé
    personnage_t s = p("S", 100, 1, 0, 50, PRETRE);

    V_CMP(bataille_init(&b, &j, 1, &s, 1), ==, 0);
    b.tour = 1;
    bataille_combattant(&b, (ref_t){1, 0})->pv = 40;                            // 40 % : sous 50 %, pas sous 25 %
    bataille_ia(&b, &a, &c);
    V_CMP(a, ==, ACTION_SOIGNER);
    V_CMP(c.equipe, ==, 1);
    V_CMP(c.index, ==, 0);
    bataille_combattant(&b, (ref_t){1, 0})->perso.soin = 0;                     // plus de soin possible
    bataille_combattant(&b, (ref_t){1, 0})->pv = 20;                            // 20 % : sous 25 %
    bataille_ia(&b, &a, &c);
    V_CMP(a, ==, ACTION_DEFENDRE);
    V_CMP(c.equipe, ==, 1);
    bataille_combattant(&b, (ref_t){1, 0})->effets[0] = (effet_t){EFFET_GARDE, 1};
    bataille_combattant(&b, (ref_t){1, 0})->nb_effets = 1;
    bataille_ia(&b, &a, &c);
    V_CMP(a, ==, ACTION_ATTAQUER);  // déjà en garde : il attaque
}

static void test_ia_ne_joue_que_des_coups_valides(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    action_t a = ACTION_ATTAQUER;
    ref_t c = {0, 0};

    duel_2v2(&b);
    for (int i = 0; i < 300 && b.gagnant == -1; i++) {
        bataille_ia(&b, &a, &c);
        V_VRAI(bataille_cible_valide(&b, a, c));
        V_CMP(bataille_agir(&b, a, c, ev), >=, 1);
    }
}

void tests_bataille(void)
{
    LANCER(test_init_etat);
    LANCER(test_init_refus);
    LANCER(test_init_copie_les_personnages);
    LANCER(test_ordre_equipes_inegales);
    LANCER(test_actif);
    LANCER(test_lire_et_combattant);
    LANCER(test_vivant_et_garde);
    LANCER(test_degats);
    LANCER(test_soin);
    LANCER(test_cible_valide_matrice);
    LANCER(test_cibles_liste);
    LANCER(test_action_possible);
    LANCER(test_agir_refus_ne_change_rien);
    LANCER(test_agir_attaque_evenements);
    LANCER(test_agir_soin_evenements);
    LANCER(test_agir_defense_et_expiration);
    LANCER(test_defensifs_compteur);
    LANCER(test_tour_saute_les_ko);
    LANCER(test_victoire_et_blocage);
    LANCER(test_defaite);
    LANCER(test_journal);
    LANCER(test_ia_achever);
    LANCER(test_ia_soigne_et_se_garde);
    LANCER(test_ia_ne_joue_que_des_coups_valides);
}
