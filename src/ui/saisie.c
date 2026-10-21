/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "couleurs.h"
#include "saisie.h"

void viderBuffer(void)
{
    int c = getchar();

    while (c != '\n' && c != EOF)
        c = getchar();
}

void afficherErreur(const char *message)
{
    printf("  " ROUGE "✗ %s" RESET "\n\n", message);
}

static void afficherQuestion(const char *question, int min, int max)
{
    printf("  " GRAS "%s" RESET GRIS " (%d-%d)" RESET CYAN " › " RESET,
        question, min, max);
}

int lireLigne(char *buf, size_t taille)
{
    size_t len = 0;

    if (fgets(buf, (int)taille, stdin) == NULL)
        return -1;
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';         // on retire le '\n'
    else
        viderBuffer();               // ligne trop longue : on jette la fin
    return 0;
}

int lireEntier(const char *question, int min, int max, int *resultat)
{
    char buf[32];
    char *fin = NULL;
    long valeur = 0;

    while (1) {
        afficherQuestion(question, min, max);
        if (lireLigne(buf, sizeof(buf)) == -1)
            return -1;
        errno = 0;
        valeur = strtol(buf, &fin, 10);
        if (fin != buf && *fin == '\0' && errno == 0
            && valeur >= min && valeur <= max) {
            *resultat = (int)valeur;
            return 0;
        }
        afficherErreur("Valeur invalide");
    }
}
