/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ saisie.c ───────────────────────────────────────────────────────────
** │ But         Lire le clavier sans risque : lignes de taille bornée,
** │             entiers vérifiés, message d'erreur uniforme.
** │ Contenu     vider_buffer, afficher_erreur, lire_ligne, lire_entier,
** │             lire_entier_menu, lire_nom, confirmer
** │ Utilisé par tous les menus, creer_personnage.c et bataille_console.c
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "couleurs.h"
#include "my_string.h"
#include "personnage.h"
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

#define LIGNE_NOM 64    /* assez grand pour détecter un nom trop long */

static void afficher_question(const char *question, int min, int max,
    int annulable)
{
    printf("  " GRAS "%s" RESET GRIS " (%d-%d%s)" RESET CYAN " › " RESET,
        question, min, max, annulable ? " · " MOT_ANNULER " annule" : "");
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
** ╭─ lire_entier_interne ────────────────────────────────────────────────
** │ Lit un entier compris entre min et max ; redemande tant que la saisie
** │ est mauvaise.
** │
** │ 1. lire_ligne() lit toute la ligne
** │ 2. si le joueur a tapé q (et que la saisie est annulable) : on sort
** │    tout de suite, sans rien modifier
** │ 3. strtol() convertit et dit où la conversion s'est arrêtée
** │ 4. accepté seulement si tout le texte est un nombre (pas de "12abc"),
** │    sans dépassement, et si min <= valeur <= max
** │ Renvoie 0 si OK, SAISIE_FIN (Ctrl+D) ou SAISIE_ANNULEE (q).
** ╰──────────────────────────────────────────────────────────────────────
*/
static int lire_entier_interne(const char *question, int min, int max,
    int *resultat, int annulable)
{
    char buf[32];
    char *fin = NULL;
    long valeur = 0;

    while (1) {
        afficher_question(question, min, max, annulable);
        if (lire_ligne(buf, sizeof(buf)) == -1)
            return SAISIE_FIN;
        if (annulable && my_strcmp(buf, MOT_ANNULER) == 0)
            return SAISIE_ANNULEE;
        errno = 0;
        valeur = strtol(buf, &fin, 10);
        if (fin != buf && *fin == '\0' && errno == 0 && valeur >= min && valeur <= max) {
            *resultat = (int)valeur;
            return 0;
        }
        afficher_erreur("Valeur invalide");
    }
}

int lire_entier(const char *question, int min, int max, int *resultat)
{
    return lire_entier_interne(question, min, max, resultat, 1);
}

int lire_entier_menu(const char *question, int min, int max, int *resultat)
{
    return lire_entier_interne(question, min, max, resultat, 0);
}

static int contient_espace(const char *s)
{
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] == ' ' || s[i] == '\t')
            return 1;
    }
    return 0;
}

/*
** ╭─ lire_nom ───────────────────────────────────────────────────────────
** │ Lit un nom de personnage, et le redemande tant qu'il est refusé.
** │
** │ 1. lire_ligne() lit toute la ligne (tampon borné) : plus de scanf, donc
** │    plus de nom coupé en silence à 29 caractères
** │ 2. q annule ; une ligne vide est ignorée
** │ 3. refusé s'il fait plus de NOM_LONGUEUR caractères ou contient une
** │    espace (le nom sert d'identifiant dans le fichier de sauvegarde)
** │ Renvoie 0, SAISIE_FIN (Ctrl+D) ou SAISIE_ANNULEE (q).
** ╰──────────────────────────────────────────────────────────────────────
*/
int lire_nom(const char *question, char *nom, size_t taille)
{
    char buf[LIGNE_NOM];

    if (taille < NOM_MAX)
        return SAISIE_FIN;
    while (1) {
        printf("  " GRAS "%s" RESET GRIS " (" MOT_ANNULER " annule)" RESET CYAN " › " RESET,
            question);
        if (lire_ligne(buf, sizeof(buf)) == -1)
            return SAISIE_FIN;
        if (my_strcmp(buf, MOT_ANNULER) == 0)
            return SAISIE_ANNULEE;
        if (buf[0] == '\0')
            continue;
        if (my_strlen(buf) > NOM_LONGUEUR)
            afficher_erreur("Nom trop long : " STR(NOM_LONGUEUR) " caractères au plus");
        else if (contient_espace(buf))
            afficher_erreur("Le nom ne peut pas contenir d'espace");
        else
            break;
    }
    snprintf(nom, taille, "%s", buf);
    return 0;
}

/*
** ╭─ confirmer ──────────────────────────────────────────────────────────
** │ Pose une question oui/non avant une action qui ne se rattrape pas.
** │
** │ La réponse est comparée en minuscules : o, oui = 1 ; n, non, q, ligne
** │ vide ou Ctrl+D = 0. Tout le reste est redemandé. Le choix « par
** │ défaut » est toujours non : appuyer sur Entrée par réflexe ne détruit
** │ rien.
** ╰──────────────────────────────────────────────────────────────────────
*/
int confirmer(const char *question)
{
    char buf[16];

    while (1) {
        printf("  " GRAS "%s" RESET GRIS " (o/n)" RESET CYAN " › " RESET, question);
        if (lire_ligne(buf, sizeof(buf)) == -1)
            return 0;
        my_strlowcase(buf);
        if (my_strcmp(buf, "o") == 0 || my_strcmp(buf, "oui") == 0)
            return 1;
        if (buf[0] == '\0' || my_strcmp(buf, "n") == 0 || my_strcmp(buf, "non") == 0
            || my_strcmp(buf, MOT_ANNULER) == 0)
            return 0;
        afficher_erreur("Réponds par o (oui) ou n (non)");
    }
}
