/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ outils_tests.c ─────────────────────────────────────────────────────
** │ But         Capturer l'affichage et simuler le clavier pour tester
** │             les fonctions qui écrivent sur stdout ou lisent stdin.
** ╰──────────────────────────────────────────────────────────────────────
*/

#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "outils_tests.h"
#include "lanceur.h"

#define FICHIER_CAPTURE "tests_capture_tmp.txt"
#define FICHIER_CLAVIER "tests_clavier_tmp.txt"

static int g_sauve = -1;

void capture_debut(void)
{
    int fd = 0;

    fflush(stdout);
    g_sauve = dup(STDOUT_FILENO);
    fd = open(FICHIER_CAPTURE, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    V_CMP(g_sauve, >=, 0);
    V_CMP(fd, >=, 0);
    dup2(fd, STDOUT_FILENO);
    close(fd);
}

void capture_fin(char *buf, size_t taille)
{
    FILE *f = NULL;
    size_t n = 0;

    fflush(stdout);
    dup2(g_sauve, STDOUT_FILENO);
    close(g_sauve);
    g_sauve = -1;
    f = fopen(FICHIER_CAPTURE, "r");
    V_CMP_BLOQUANT(f, !=, NULL);
    V_CMP(taille, >, 0);
    n = fread(buf, 1, taille - 1, f);
    buf[n] = '\0';
    fclose(f);
    remove(FICHIER_CAPTURE);
}

void entree_simulee(const char *texte)
{
    ecrire_fichier_test(FICHIER_CLAVIER, texte);
    V_CMP_BLOQUANT(freopen(FICHIER_CLAVIER, "r", stdin), !=, NULL);
}

int muet_commencer(void)
{
    int sauve = 0;
    int nul = open("/dev/null", O_WRONLY);

    fflush(stdout);
    sauve = dup(STDOUT_FILENO);
    dup2(nul, STDOUT_FILENO);
    close(nul);
    return sauve;
}

void muet_finir(int sauve)
{
    fflush(stdout);
    dup2(sauve, STDOUT_FILENO);
    close(sauve);
}

void ecrire_fichier_test(const char *chemin, const char *contenu)
{
    FILE *f = fopen(chemin, "w");

    V_CMP_BLOQUANT(f, !=, NULL);
    fputs(contenu, f);
    fclose(f);
}

int contient(const char *texte, const char *morceau)
{
    return strstr(texte, morceau) != NULL;
}
