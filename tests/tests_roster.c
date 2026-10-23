/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ tests_roster.c ─────────────────────────────────────────────────────
** │ But         Un ou plusieurs tests par fonction de roster.c : liste
** │             dynamique, validité, recherche par nom ou par ID.
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include <string.h>
#include "lanceur.h"
#include "roster.h"
#include "tests_donnees.h"

static personnage_t nomme(const char *nom, int pv)
{
    personnage_t p = {0};

    snprintf(p.nom, sizeof(p.nom), "%s", nom);
    p.pv = pv;
    p.classe = GUERRIER;
    return p;
}

static void test_roster_init(void)
{
    roster_t r;

    memset(&r, 0xAB, sizeof(r));                    // contenu quelconque avant
    roster_init(&r);
    V_CMP(r.data, ==, NULL);
    V_CMP(r.len, ==, 0);
    V_CMP(r.capacite, ==, 0);
}

static void test_roster_ajouter_croissance(void)
{
    roster_t r;
    personnage_t p = nomme("P", 1);
    int capacite_prec = 0;
    int changements = 0;

    roster_init(&r);
    for (int i = 0; i < 100; i++) {
        p.pv = i;
        V_CMP(roster_ajouter(&r, &p), ==, 0);
        V_CMP(r.len, ==, i + 1);
        V_CMP(r.capacite, >=, r.len);
        if (r.capacite != capacite_prec) {
            changements++;
            capacite_prec = r.capacite;
        }
    }
    V_CMP(r.capacite, ==, ROSTER_CAPACITE_INITIALE * 32);  // 4 -> 8 -> ... -> 128
    V_CMP(changements, ==, 6);  // la capacité double à chaque fois
    for (int i = 0; i < 100; i++)
        V_CMP(r.data[i].pv, ==, i);  // l'ordre et le contenu sont conservés
    roster_liberer(&r);
}

static void test_roster_ajouter_copie(void)
{
    roster_t r;
    personnage_t p = nomme("Orig", 5);

    roster_init(&r);
    V_CMP(roster_ajouter(&r, &p), ==, 0);
    p.pv = 99;
    snprintf(p.nom, sizeof(p.nom), "Modif");
    V_TEXTE_EGAL(r.data[0].nom, "Orig");  // c'est une copie
    V_CMP(r.data[0].pv, ==, 5);  // c'est une copie
    roster_liberer(&r);
}

static void test_roster_liberer(void)
{
    roster_t r;
    personnage_t p = nomme("P", 1);

    roster_init(&r);
    roster_ajouter(&r, &p);
    roster_liberer(&r);
    V_CMP(r.data, ==, NULL);
    V_CMP(r.len, ==, 0);
    V_CMP(r.capacite, ==, 0);
    roster_liberer(&r);                                          // double libération sans danger
    V_CMP(r.data, ==, NULL);
}

static void test_roster_valide(void)
{
    roster_t r;
    personnage_t p = nomme("P", 1);

    V_FAUX(roster_valide(NULL));
    roster_init(&r);
    V_VRAI(roster_valide(&r));  // vide : valide
    roster_ajouter(&r, &p);
    V_VRAI(roster_valide(&r));
    r.len = -1;
    V_FAUX(roster_valide(&r));
    r.len = r.capacite + 1;
    V_FAUX(roster_valide(&r));  // plus de personnages que de places
    r.len = 1;
    roster_liberer(&r);
    r.len = 1;
    V_FAUX(roster_valide(&r));  // len > 0 mais pas de données
    r.len = 0;
}

static void test_roster_trouver_cas(void)
{
    roster_t r;
    personnage_t a = nomme("Ann", 1);
    personnage_t b = nomme("Bob", 1);

    roster_init(&r);
    roster_ajouter(&r, &a);
    roster_ajouter(&r, &b);
    V_CMP(roster_trouver(&r, "Ann"), ==, 0);
    V_CMP(roster_trouver(&r, "Bob"), ==, 1);
    V_CMP(roster_trouver(&r, "ann"), ==, -1);  // nom exact
    V_CMP(roster_trouver(&r, "1"), ==, 0);  // ID : position + 1
    V_CMP(roster_trouver(&r, "2"), ==, 1);  // ID : position + 1
    V_CMP(roster_trouver(&r, "0"), ==, -1);
    V_CMP(roster_trouver(&r, "3"), ==, -1);
    V_CMP(roster_trouver(&r, "-1"), ==, -1);
    V_CMP(roster_trouver(&r, ""), ==, -1);
    V_CMP(roster_trouver(&r, "1x"), ==, -1);
    V_CMP(roster_trouver(&r, " 1"), ==, -1);
    V_CMP(roster_trouver(&r, "0000000001"), ==, -1);  // plus de 9 chiffres : refusé
    V_CMP(roster_trouver(&r, "999999999999999999"), ==, -1);  // sans dépassement d'int
    roster_liberer(&r);
}

static void test_roster_trouver_nom_prioritaire(void)
{
    roster_t r;
    personnage_t a = nomme("2", 1);                              // roster incohérent : un nom-nombre
    personnage_t b = nomme("Zed", 1);

    roster_init(&r);
    roster_ajouter(&r, &a);
    roster_ajouter(&r, &b);
    V_CMP(roster_trouver(&r, "2"), ==, 0);  // le nom exact prime sur l'ID
    V_CMP(roster_trouver(&r, "1"), ==, 0);
    roster_liberer(&r);
}

static void test_roster_nb_en_forme(void)
{
    roster_t r;
    personnage_t vivant = nomme("V", 10);
    personnage_t mort = nomme("M", 0);

    roster_init(&r);
    V_CMP(roster_nb_en_forme(&r), ==, 0);
    roster_ajouter(&r, &vivant);
    roster_ajouter(&r, &mort);
    roster_ajouter(&r, &vivant);
    V_CMP(roster_nb_en_forme(&r), ==, 2);
    r.data[0].pv = 0;
    r.data[2].pv = 0;
    V_CMP(roster_nb_en_forme(&r), ==, 0);
    roster_liberer(&r);
}

void tests_roster(void)
{
    LANCER(test_roster_init);
    LANCER(test_roster_ajouter_croissance);
    LANCER(test_roster_ajouter_copie);
    LANCER(test_roster_liberer);
    LANCER(test_roster_valide);
    LANCER(test_roster_trouver_cas);
    LANCER(test_roster_trouver_nom_prioritaire);
    LANCER(test_roster_nb_en_forme);
}
