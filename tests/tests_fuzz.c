/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_fuzz.c ───────────────────────────────────────────────────────
** │ But         Jeter au programme des entrées au hasard (mais
** │             reproductibles : la graine est fixe) et vérifier qu'il
** │             ne plante pas et que son état reste cohérent.
** │ Contenu     fichiers de sauvegarde corrompus · combats avec des
** │             actions et des cibles aléatoires, bonnes ou absurdes
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bataille.h"
#include "lanceur.h"
#include "my_string.h"
#include "roster.h"
#include "sauvegarde.h"
#include "tests_fuzz.h"

#define FICHIER_FUZZ "tests_fuzz_tmp.txt"
#define ESSAIS 300

static const char VALIDE[] = "Bob 50 20 10 0 5\nAlice 40 45 10 1 8\n";

/* Une sauvegarde valide dont quelques octets sont remplacés au hasard. */
static void ecrire_corrompu(int essai)
{
    char buf[sizeof(VALIDE)];
    FILE *f = fopen(FICHIER_FUZZ, "w");
    size_t n = sizeof(VALIDE) - 1;

    V_CMP_BLOQUANT(f, !=, NULL);
    memcpy(buf, VALIDE, sizeof(VALIDE));
    for (int k = 0; k < 1 + essai % 6; k++)
        buf[(size_t)rand() % n] = (char)(rand() % 256);
    if (essai % 7 == 0)
        n = (size_t)rand() % n;             // fichier tronqué
    fwrite(buf, 1, n, f);
    fclose(f);
}

static void test_fuzz_sauvegarde(void)
{
    roster_t r;
    personnage_t bob = {"Bob", 50, 20, 10, 5, GUERRIER};

    srand(12345);
    for (int i = 0; i < ESSAIS; i++) {
        roster_init(&r);
        V_CMP(roster_ajouter(&r, &bob), ==, 0);
        ecrire_corrompu(i);
        if (charger(&r, FICHIER_FUZZ) != SAUVEGARDE_OK)
            do {
                V_CMP(r.len, ==, 1);
                V_TEXTE_EGAL(r.data[0].nom, "Bob");
            } while (0);  // intact
        else
            V_VRAI(roster_valide(&r));
        for (int j = 0; j < r.len; j++) {
            V_CMP(r.data[j].pv, >=, STAT_MIN);
            V_CMP(r.data[j].pv, <=, STAT_MAX);
            V_VRAI(classe_valide((int)r.data[j].classe));
        }
        roster_liberer(&r);
    }
    remove(FICHIER_FUZZ);
}

static void test_fuzz_fichier_binaire(void)
{
    roster_t r;
    FILE *f = fopen(FICHIER_FUZZ, "wb");

    srand(777);
    V_CMP_BLOQUANT(f, !=, NULL);
    for (int i = 0; i < 20000; i++)
        fputc(rand() % 256, f);
    fclose(f);
    roster_init(&r);
    V_CMP(charger(&r, FICHIER_FUZZ), !=, SAUVEGARDE_OK);
    V_CMP(r.len, ==, 0);
    roster_liberer(&r);
    remove(FICHIER_FUZZ);
}

static personnage_t hasard_perso(void)
{
    personnage_t p = {"P", 1 + rand() % 99, rand() % 100, rand() % 120, rand() % 100,
        (classe_t)(rand() % NB_CLASSES)};

    return p;
}

/* Vrai si les PV de tout le monde restent dans [0, pv_max]. */
static int pv_coherents(const bataille_t *b)
{
    for (int e = 0; e < NB_EQUIPES; e++)
        for (int i = 0; i < b->equipes[e].taille; i++)
            if (b->equipes[e].membres[i].pv < 0
                || b->equipes[e].membres[i].pv > b->equipes[e].membres[i].pv_max)
                return 0;
    return 1;
}

static void test_fuzz_bataille(void)
{
    bataille_t b;
    evenement_t ev[MAX_EVENEMENTS];
    personnage_t a[TAILLE_EQUIPE_MAX];
    personnage_t j[TAILLE_EQUIPE_MAX];
    int nj = 0;
    int na = 0;
    ref_t cible = {0, 0};

    srand(2026);
    for (int essai = 0; essai < 200; essai++) {
        nj = 1 + rand() % TAILLE_EQUIPE_MAX;
        na = 1 + rand() % TAILLE_EQUIPE_MAX;
        for (int i = 0; i < nj; i++)
            j[i] = hasard_perso();
        for (int i = 0; i < na; i++)
            a[i] = hasard_perso();
        V_CMP(bataille_init(&b, j, nj, a, na), ==, 0);
        for (int tour = 0; tour < 2000 && b.gagnant == -1; tour++) {
            cible = (ref_t){rand() % 5 - 1, rand() % 6 - 1};     // parfois absurde
            (void)bataille_agir(&b, (action_t)(rand() % (NB_ACTIONS + 2) - 1), cible, ev);
            V_VRAI(pv_coherents(&b));
            if (rand() % 3 == 0) {
                action_t act = ACTION_ATTAQUER;

                bataille_ia(&b, &act, &cible);
                (void)bataille_agir(&b, act, cible, ev);
                V_VRAI(pv_coherents(&b));
            }
        }
    }
}

void tests_fuzz(void)
{
    LANCER(test_fuzz_sauvegarde);
    LANCER(test_fuzz_fichier_binaire);
    LANCER(test_fuzz_bataille);
}
