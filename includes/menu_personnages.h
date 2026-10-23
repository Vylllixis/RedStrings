/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef MENU_PERSONNAGES_H
    #define MENU_PERSONNAGES_H

    #include "roster.h"

/** Numéros des entrées du menu principal. */
typedef enum {
    CHOIX_QUITTER = 0,
    CHOIX_AJOUTER,
    CHOIX_AFFICHER,
    CHOIX_COMBAT,
    CHOIX_SAUVEGARDER,
    CHOIX_CHARGER,
    CHOIX_FUSIONNER,
    NB_CHOIX
} choix_menu_t;

/**
 * Pourquoi l'entrée `choix` du menu n'est pas utilisable maintenant (par
 * exemple « Afficher » sans aucun personnage), ou NULL si elle l'est.
 */
const char *menu_indisponible(const roster_t *r, int choix);

/** Crée un personnage et l'ajoute ; `q` annule à chaque étape sans rien enregistrer. */
void menu_ajouter(roster_t *r);

/** Montre la liste puis la fiche du personnage choisi (par son nom ou son ID). */
void menu_afficher(const roster_t *r);

/** Écrit tous les personnages dans FICHIER_SAUVEGARDE. */
void menu_sauvegarder(const roster_t *r);

/** Remplace tous les personnages par ceux du fichier, après confirmation s'il y en a. */
void menu_charger(roster_t *r);

/** Ajoute les personnages du fichier à ceux qui existent, sans rien écraser. */
void menu_fusionner(roster_t *r);

#endif /* MENU_PERSONNAGES_H */
