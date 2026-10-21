/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "couleurs.h"
#include "my_string.h"
#include "saisie.h"

void vider_buffer(void)
{
    int c = getchar();

    while (c != '\n' && c != EOF)
        c = getchar();
}

void afficher_erreur(const char *message)
{
    printf("  " ROUGE "✗ %s" RESET "\n\n", message);
}

static void afficher_question(const char *question, int min, int max)
{
    printf("  " GRAS "%s" RESET GRIS " (%d-%d)" RESET CYAN " › " RESET,
        question, min, max);
}

int lire_ligne(char *buf, size_t taille)
{
    size_t len = 0;

    if (fgets(buf, (int)taille, stdin) == NULL)
        return -1;
    len = my_strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';         // on retire le '\n'
    else
        vider_buffer();               // ligne trop longue : on jette la fin
    return 0;
}

int lire_entier(const char *question, int min, int max, int *resultat)
{
    char buf[32];
    char *fin = NULL;
    long valeur = 0;

    while (1) {
        afficher_question(question, min, max);
        if (lire_ligne(buf, sizeof(buf)) == -1)
            return -1;
        errno = 0;
        valeur = strtol(buf, &fin, 10);
        if (fin != buf && *fin == '\0' && errno == 0
            && valeur >= min && valeur <= max) {
            *resultat = (int)valeur;
            return 0;
        }
        afficher_erreur("Valeur invalide");
    }
}
