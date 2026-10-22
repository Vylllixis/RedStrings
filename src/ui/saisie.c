/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ saisie.c ───────────────────────────────────────────────────────────
** │ But         Lire le clavier sans risque : lignes de taille bornée,
** │             entiers vérifiés, message d'erreur uniforme.
** │ Contenu     vider_buffer, afficher_erreur, lire_ligne, lire_entier
** │ Utilisé par tous les menus, creer_personnage.c et bataille_console.c
** ╰──────────────────────────────────────────────────────────────────────
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

/*
** ╭─ lire_ligne ─────────────────────────────────────────────────────────
** │ Lit une ligne dans buf, sans jamais dépasser sa taille.
** │
** │ 1. fgets() lit au plus taille - 1 caractères
** │ 2. si la ligne finit par '\n', on le retire
** │ 3. sinon la ligne était trop longue : vider_buffer() jette la fin,
** │    pour qu'elle ne pollue pas la question suivante
** │ Renvoie 0 si OK, -1 si l'entrée est fermée (Ctrl+D).
** ╰──────────────────────────────────────────────────────────────────────
*/
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

/*
** ╭─ lire_entier ────────────────────────────────────────────────────────
** │ Lit un entier compris entre min et max ; redemande tant que la saisie
** │ est mauvaise.
** │
** │ 1. lire_ligne() lit toute la ligne
** │ 2. strtol() convertit et dit où la conversion s'est arrêtée
** │ 3. accepté seulement si tout le texte est un nombre (pas de "12abc"),
** │    sans dépassement, et si min <= valeur <= max
** │ Renvoie 0 si OK, -1 si l'entrée est fermée (Ctrl+D).
** ╰──────────────────────────────────────────────────────────────────────
*/
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
