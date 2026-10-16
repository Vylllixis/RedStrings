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

#endif /* PERSONNAGE_H */
