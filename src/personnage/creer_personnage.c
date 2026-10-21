/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <stdio.h>
#include "boite.h"
#include "couleurs.h"
#include "personnage.h"
#include "saisie.h"

static int saisie_interrompue(void)
{
    afficher_erreur("Saisie interrompue");
    return -1;
}

static int lire_stat(const char *libelle, int *stat)
{
    char libelle_aligne[64];

    aligner_texte(libelle_aligne, sizeof(libelle_aligne), libelle,
        LARGEUR_LIBELLE);
    return lire_entier(libelle_aligne, STAT_MIN, STAT_MAX, stat);
}

int creer_personnage(personnage_t *p, personnage_t *tab, int len)
{
    int choix = 1;

    //en-tête
    printf("\n");
    boite_haut(CYAN);
    boite_ligne(CYAN, "  " GRAS "✦ Création d'un personnage" RESET);
    boite_bas(CYAN);
    printf("\n");

    //choix du nom
    printf("  " GRAS "Nom" RESET CYAN " › " RESET);
    if (scanf("%" STR(NOM_LONGUEUR) "s", p->nom) != 1)
        return saisie_interrompue();
    vider_buffer();
    if (trouver_personnage(tab, len, p->nom) != -1) {
        afficher_erreur("Ce nom est déjà pris");
        return -1;
    }
    printf("\n");

    //choix des statistiques
    printf("  " GRIS "── Statistiques ──" RESET "\n");
    if (lire_stat("Points de vie", &p->pv) == -1
        || lire_stat("Attaque", &p->atq) == -1
        || lire_stat("Défense", &p->def) == -1)
        return saisie_interrompue();
    printf("\n");

    //choix de la classe (1-3 saisi, l'enum commence à 0)
    printf("  " GRIS "── Classe ──" RESET "\n");
    for (int i = 0; i < NB_CLASSES; i++)
        printf("    " GRAS "%d" RESET "  %s%s" RESET "\n",
            i + 1, classes[i].couleur, classes[i].nom);
    if (lire_entier("Choix", 1, NB_CLASSES, &choix) == -1)
        return saisie_interrompue();
    p->classe = (classe_t)(choix - 1);

    printf("\n  " VERT "✔ %s a été créé !" RESET "\n", p->nom);
    return 0;
}
