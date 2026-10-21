/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef PERSONNAGE_H
    #define PERSONNAGE_H

typedef enum classe {
    GUERRIER,   /**< Affiché en rouge. */
    MAGE,       /**< Affiché en magenta. */
    ARCHER,      /**< Affiché en vert. */
    NB_CLASSES
} classe_t;

typedef struct {
    const char *nom;
    const char *couleur;
} info_classe_t;

extern const info_classe_t classes[NB_CLASSES];

#define NOM_MAX   30
#define STAT_MIN  0
#define STAT_MAX  99

typedef struct {
    char nom[30];      /**< Nom, sans espace, sert d'identifiant. */
    int pv;             /**< Points de vie, mort à 0. */
    int atq;            /**< Dégâts bruts infligés. */
    int def;            /**< Pourcentage de réduction des dégâts reçus. */
    classe_t classe;    /**< Classe du personnage. */
} personnage_t;

/** Cherche un personnage par son nom, renvoie son indice ou -1. */
int trouver_personnage(const personnage_t *tab, int size, const char *nom);

#define STR_(x) #x
#define STR(x)  STR_(x)
#define NOM_LONGUEUR 29   /* NOM_MAX - 1 */

/** Demande le nom, les stats et la classe au joueur (nom unique dans tab).
 *  Renvoie 0 si le personnage est créé, -1 si la saisie a échoué. */
int creer_personnage(personnage_t *p, personnage_t *tab, int len);

/** Affiche la fiche du personnage avec ses barres de stats. */
void afficher_personnage(const personnage_t *p);

/** Dégâts que l'attaquant inflige à la cible, jamais négatifs. */
int calculer_degats(const personnage_t *attaquant, const personnage_t *cible);

/** Retire à la cible les dégâts de l'attaquant, réduits par sa défense. */
void attaquer(personnage_t *attaquant, personnage_t *cible);

#endif /* PERSONNAGE_H */
