/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Moteur de combat en équipe, au tour par tour. Il ne dépend d'aucune
 * interface : la console et la version CSFML appellent les mêmes
 * fonctions, puis représentent les événements qu'il renvoie.
 *
 * Le combat travaille sur des COPIES des personnages : les PV perdus et
 * les effets temporaires disparaissent à la fin, le roster n'est jamais
 * modifié.
 */

#ifndef BATAILLE_H
    #define BATAILLE_H

    #include "personnage.h"

    #define NB_EQUIPES 2
    #define TAILLE_EQUIPE_MAX 4         /* combattants par équipe */
    #define MAX_EFFETS 4                /* effets temporaires par combattant */
    #define MAX_EVENEMENTS 16           /* événements produits par une action */
    #define JOURNAL_MAX 8               /* lignes de journal conservées */
    #define JOURNAL_LIGNE 96
    #define GARDE_REDUCTION 50          /* % de dégâts absorbés en garde */
    #define GARDE_DUREE 1               /* expire au début du tour suivant */

typedef enum {
    ACTION_ATTAQUER,
    ACTION_SOIGNER,
    ACTION_DEFENDRE,
    NB_ACTIONS
} action_t;

/** Effets temporaires : un seul pour l'instant, prévu pour en accueillir d'autres. */
typedef enum {
    EFFET_GARDE,
    NB_TYPES_EFFET
} type_effet_t;

typedef struct {
    type_effet_t type;
    int tours;          /**< Débuts de tour du porteur avant expiration. */
} effet_t;

typedef struct {
    personnage_t perso;     /**< Copie : nom, classe, statistiques. */
    int pv;                 /**< PV actuels pendant le combat. */
    int pv_max;             /**< PV au début du combat (statistique PV). */
    effet_t effets[MAX_EFFETS];
    int nb_effets;
} combattant_t;

typedef struct {
    combattant_t membres[TAILLE_EQUIPE_MAX];
    int taille;
} equipe_t;

/** Désigne un combattant : son équipe (0 = joueur, 1 = adversaire) et son rang. */
typedef struct {
    int equipe;
    int index;
} ref_t;

typedef enum {
    EV_ATTAQUE,         /**< valeur = dégâts infligés. */
    EV_SOIN,            /**< valeur = PV réellement rendus. */
    EV_GARDE,           /**< l'acteur se met en garde. */
    EV_GARDE_FIN,       /**< la garde de l'acteur expire. */
    EV_KO,              /**< la cible tombe à 0 PV. */
    EV_VICTOIRE         /**< valeur = équipe gagnante. */
} type_evenement_t;

/** Ce qui s'est passé, dans l'ordre : l'interface ne fait que le représenter. */
typedef struct {
    type_evenement_t type;
    ref_t acteur;
    ref_t cible;
    int valeur;
    int pv_apres;       /**< PV de la cible après l'événement. */
    int garde;          /**< 1 si la garde de la cible a réduit les dégâts. */
} evenement_t;

typedef struct {
    equipe_t equipes[NB_EQUIPES];
    ref_t ordre[NB_EQUIPES * TAILLE_EQUIPE_MAX];    /**< Ordre des tours. */
    int nb_ordre;
    int tour;           /**< Indice dans ordre[] du combattant actif. */
    int manche;         /**< Nombre de tours complets écoulés. */
    int gagnant;        /**< Équipe gagnante, -1 tant que le combat continue. */
    char journal[JOURNAL_MAX][JOURNAL_LIGNE];
    int nb_journal;     /**< Lignes écrites depuis le début (tampon circulaire). */
} bataille_t;

/** Prépare le combat. Renvoie -1 si une équipe est vide, trop grande ou
 *  contient un personnage K.O. */
int bataille_init(bataille_t *b, const personnage_t *joueurs, int nb_joueurs,
    const personnage_t *adversaires, int nb_adversaires);

/** Combattant dont c'est le tour. */
ref_t bataille_actif(const bataille_t *b);
combattant_t *bataille_combattant(bataille_t *b, ref_t r);
const combattant_t *bataille_lire(const bataille_t *b, ref_t r);

int combattant_vivant(const combattant_t *c);
int combattant_en_garde(const combattant_t *c);

/** Dégâts qu'infligerait l'attaquant à la cible (garde comprise), au moins 1. */
int bataille_degats(const combattant_t *attaquant, const combattant_t *cible);
/** PV qu'un soin rendrait réellement à la cible (plafonné aux PV max). */
int bataille_soin(const combattant_t *soigneur, const combattant_t *cible);

/** 1 si le combattant actif peut faire cette action sur cette cible. */
int bataille_cible_valide(const bataille_t *b, action_t a, ref_t cible);
/** Remplit `cibles` avec les cibles valides de l'action ; renvoie leur nombre. */
int bataille_cibles(const bataille_t *b, action_t a,
    ref_t cibles[NB_EQUIPES * TAILLE_EQUIPE_MAX]);
/** 1 si l'action a au moins une cible valide pour le combattant actif. */
int bataille_action_possible(const bataille_t *b, action_t a);

/**
 * Résout l'action du combattant actif, puis passe au suivant vivant (et
 * fait expirer ses effets). Écrit les événements dans `ev` et renvoie leur
 * nombre, ou -1 si l'action est refusée (combat fini, cible invalide…).
 */
int bataille_agir(bataille_t *b, action_t a, ref_t cible,
    evenement_t ev[MAX_EVENEMENTS]);

/** Choix déterministe de l'ordinateur pour le combattant actif. */
void bataille_ia(const bataille_t *b, action_t *a, ref_t *cible);

/** Ligne de journal, de la plus récente (0) à la plus ancienne ; NULL au-delà. */
const char *bataille_journal(const bataille_t *b, int i);

#endif /* BATAILLE_H */
