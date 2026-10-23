/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef PERSONNAGE_H
    #define PERSONNAGE_H

/*
 * Catalogue des classes. L'identifiant numérique est écrit tel quel dans
 * personnages.txt : on n'insère JAMAIS une classe au milieu, on l'ajoute
 * juste avant NB_CLASSES. GUERRIER, MAGE et ARCHER gardent 0, 1 et 2.
 */
typedef enum classe {
    GUERRIER,
    MAGE,
    ARCHER,
    PALADIN,
    BERSERKER,
    CHEVALIER_NOIR,
    SAMOURAI,
    DUELLISTE,
    LANCIER,
    GLADIATEUR,
    ARCHIMAGE,
    ELEMENTALISTE,
    NECROMANCIEN,
    INVOCATEUR,
    DRUIDE,
    ILLUSIONNISTE,
    CHRONOMANCIEN,
    CHASSEUR,
    TIREUR_ELITE,
    ASSASSIN,
    OMBRELAME,
    CORSAIRE,
    PRETRE,
    BARDE,
    ALCHIMISTE,
    INGENIEUR,
    GARDIEN,
    MOINE,
    NB_CLASSES
} classe_t;

typedef enum {
    CAT_CORPS_A_CORPS,
    CAT_MAGIE,
    CAT_DISTANCE,
    CAT_SOUTIEN,
    NB_CATEGORIES
} categorie_t;

/** Indices des statistiques dans info_classe_t.mods. */
typedef enum {
    STAT_PV,
    STAT_ATQ,
    STAT_DEF,
    STAT_SOIN,
    NB_STATS
} stat_t;

/** Symbole graphique de la classe (dessiné par la version CSFML). */
typedef enum {
    EMB_EPEES,
    EMB_BOUCLIER,
    EMB_HACHE,
    EMB_LAME,
    EMB_LANCE,
    EMB_ETOILE,
    EMB_FLAMME,
    EMB_CRANE,
    EMB_PORTAIL,
    EMB_FEUILLE,
    EMB_LUNE,
    EMB_SABLIER,
    EMB_ARC,
    EMB_CIBLE,
    EMB_ANCRE,
    EMB_CROIX,
    EMB_NOTE,
    EMB_FIOLE,
    EMB_ENGRENAGE,
    EMB_ORBE,
    NB_EMBLEMES
} embleme_t;

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} rgb_t;

    /* Bornes des modificateurs et longueur des descriptions (une ligne de boîte). */
    #define MOD_MAX 20
    #define DESCRIPTION_MAX 32

/**
 * Tout ce qui définit une classe, au même endroit : la console, la version
 * graphique et la logique de jeu lisent cette seule table.
 */
typedef struct {
    const char *nom;
    const char *description;    /**< Une ligne, DESCRIPTION_MAX caractères max. */
    categorie_t categorie;
    rgb_t couleur;              /**< Couleur du terminal (ANSI 24 bits) et de CSFML. */
    int mods[NB_STATS];         /**< Bonus/malus de PV, ATQ, DEF, soin ; somme nulle. */
    embleme_t embleme;
} info_classe_t;

extern const info_classe_t classes[NB_CLASSES];

/* Soin donné aux personnages d'une sauvegarde écrite avant cette statistique,
 * avant le modificateur de leur classe. */
#define SOIN_MIGRATION 20

#define NOM_MAX   30
#define STAT_MIN  0
#define STAT_MAX  99

typedef struct {
    char nom[30];      /**< Nom, sans espace, sert d'identifiant. */
    int pv;             /**< Points de vie, mort à 0. */
    int atq;            /**< Dégâts bruts infligés. */
    int def;            /**< Pourcentage de réduction des dégâts reçus. */
    int soin;           /**< PV rendus par l'action Soigner. */
    classe_t classe;    /**< Classe du personnage. */
} personnage_t;

/** Nom affichable d'une catégorie (« Corps à corps »…). */
const char *nom_categorie(categorie_t c);

/** Séquence ANSI 24 bits de la couleur de la classe (à fermer par RESET). */
const char *couleur_ansi(classe_t c);

/** 1 si `id` désigne une classe du catalogue (lecture d'une sauvegarde…). */
int classe_valide(int id);

/** Remplit `ordre` avec toutes les classes rangées par catégorie. */
void ordre_classes(classe_t ordre[NB_CLASSES]);

/** Valeur de la statistique `s` d'un personnage (PV, ATQ, DEF ou soin). */
int valeur_stat(const personnage_t *p, stat_t s);

/** Statistique de base modifiée par la classe, bornée à STAT_MIN..STAT_MAX. */
int stat_finale(classe_t c, stat_t s, int base);

/** Applique les modificateurs de sa classe aux statistiques de `p`. */
void appliquer_classe(personnage_t *p);

/** Cherche un personnage par son nom, renvoie son indice ou -1. */
int trouver_personnage(const personnage_t *tab, int size, const char *nom);

/**
 * 1 si `nom` ne contient que des chiffres (« 3 », « 007 »), sinon 0. Un tel nom
 * est interdit : dans la console, un nombre tapé désigne l'ID d'un personnage
 * (sa position dans la liste affichée), il ne doit pas pouvoir aussi être un nom.
 */
int nom_est_un_id(const char *nom);

#define STR_(x) #x
#define STR(x)  STR_(x)
#define NOM_LONGUEUR 29   /**< NOM_MAX - 1 */

/** Demande le nom, les stats et la classe au joueur (nom unique dans tab).
 *  `q` annule à chaque question ; *p n'est alors jamais ajouté nulle part.
 *  Renvoie 0 si le personnage est créé, SAISIE_ANNULEE (q) ou SAISIE_FIN
 *  (Ctrl+D) sinon (codes de saisie.h). */
int creer_personnage(personnage_t *p, personnage_t *tab, int len);

/** Affiche la fiche du personnage avec ses barres de stats. */
void afficher_personnage(const personnage_t *p);

/** Dégâts bruts : attaque réduite par le pourcentage de défense de la cible.
 *  Base de bataille_degats(), qui ajoute la garde et le minimum de 1. */
int calculer_degats(const personnage_t *attaquant, const personnage_t *cible);

/** 1 si le personnage est K.O. (0 PV), 0 sinon. */
int est_ko(const personnage_t *p);


#endif /* PERSONNAGE_H */
