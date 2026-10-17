/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef PERSONNAGE_H
    #define PERSONNAGE_H

typedef enum Classe {
    GUERRIER,   /**< Affiché en rouge. */
    MAGE,       /**< Affiché en magenta. */
    ARCHER      /**< Affiché en vert. */
} Classe_t;

typedef struct {
    char name[30];      /**< Nom, sans espace, sert d'identifiant. */
    int pv;             /**< Points de vie, mort à 0. */
    int atk;            /**< Dégâts bruts infligés. */
    int def;            /**< Pourcentage de réduction des dégâts reçus. */
    Classe_t classe;    /**< Classe du personnage. */
} personnage_t;

/** Demande le nom, les stats et la classe au joueur. */
int creerPersonnage(personnage_t *p);

/** Affiche la fiche du personnage avec ses barres de stats. */
int afficherPersonnage(const personnage_t *p);

/** Retire à la cible les dégâts de l'attaquant, réduits par sa défense. */
void attaquer(personnage_t *attaquant, personnage_t *cible);

#endif /* PERSONNAGE_H */
