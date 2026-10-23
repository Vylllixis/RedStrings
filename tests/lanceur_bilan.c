/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ lanceur_bilan.c ────────────────────────────────────────────────────
** │ But         Le bilan de fin de tests : barre de progression, totaux,
** │             nombre de vérifications, temps, et la liste des tests à
** │             corriger avec leur emplacement.
** ╰──────────────────────────────────────────────────────────────────────
*/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include "lanceur_interne.h"

#define ROUGE "\033[1;31m"
#define VERT "\033[1;32m"
#define GRIS "\033[2m"
#define GRAS "\033[1m"
#define RESET "\033[0m"
#define LARGEUR_BARRE 40

static void ligne_titre(const char *titre)
{
    printf("\n  %s┏━ %s ", rapport_couleur(GRAS), titre);
    for (int i = (int)strlen(titre); i < 56; i++)
        printf("━");
    printf("%s\n", rapport_couleur(RESET));
}

static void barre(int reussis, int total, int echecs)
{
    int pleins = total > 0 ? reussis * LARGEUR_BARRE / total : 0;

    printf("  ┃  %s", rapport_couleur(echecs == 0 ? VERT : ROUGE));
    for (int i = 0; i < LARGEUR_BARRE; i++)
        printf("%s", i < pleins ? "█" : "░");
    printf("%s  %3d %%\n", rapport_couleur(RESET), total > 0 ? reussis * 100 / total : 0);
}

void rapport_bilan(int total, int reussis, int verifs, double ms, const resume_echec_t *liste,
    int nb_liste)
{
    int echecs = total - reussis;

    ligne_titre("Bilan des tests");
    barre(reussis, total, echecs);
    printf("  ┃  %s✔ %d réussi%s%s    %s✘ %d échoué%s%s    %d test%s\n", rapport_couleur(VERT),
        reussis, reussis > 1 ? "s" : "", rapport_couleur(RESET), rapport_couleur(echecs ? ROUGE : GRIS),
        echecs, echecs > 1 ? "s" : "", rapport_couleur(RESET), total, total > 1 ? "s" : "");
    printf("  ┃  %s%d vérification%s · %.2f s%s\n", rapport_couleur(GRIS), verifs, verifs > 1 ? "s" : "",
        ms / 1000.0, rapport_couleur(RESET));
    printf("  ┗%s\n", "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    if (echecs == 0) {
        printf("\n  %sTout est vert : aucun test à corriger.%s\n\n", rapport_couleur(VERT), rapport_couleur(RESET));
        return;
    }
    printf("\n  %sÀ corriger (%d) :%s\n", rapport_couleur(ROUGE), echecs, rapport_couleur(RESET));
    for (int i = 0; i < nb_liste; i++) {
        printf("   %s%2d.%s %s%s%s  %s%s%s\n", rapport_couleur(GRIS), i + 1, rapport_couleur(RESET),
            rapport_couleur(GRAS), liste[i].nom, rapport_couleur(RESET), rapport_couleur(GRIS),
            liste[i].lieu, rapport_couleur(RESET));
        printf("       %s%s%s\n", rapport_couleur(ROUGE), liste[i].resume, rapport_couleur(RESET));
    }
    if (echecs > nb_liste)
        printf("   … et %d autres\n", echecs - nb_liste);
    printf("\n");
}
