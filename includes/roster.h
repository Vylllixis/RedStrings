/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef ROSTER_H
    #define ROSTER_H

    #include "personnage.h"

    /* Capacité allouée au premier ajout, puis doublée à chaque fois. */
    #define ROSTER_CAPACITE_INITIALE 4

typedef struct {
    personnage_t *data;     /**< Tableau alloué avec realloc. */
    int len;                /**< Nombre de personnages. */
    int capacite;           /**< Nombre de places allouées. */
} roster_t;

/** Prépare un roster vide, sans allocation. */
void roster_init(roster_t *r);

/** Copie p à la fin du roster. Renvoie -1 si l'allocation échoue
 *  (le roster reste alors intact). */
int roster_ajouter(roster_t *r, const personnage_t *p);

/** Libère la mémoire et remet le roster à vide. */
void roster_liberer(roster_t *r);

/**
 * Indice du personnage désigné par `saisie` : son nom exact, ou son numéro
 * (1 à r->len, comme dans la liste affichée). Le nom a priorité sur le numéro.
 * @return l'indice dans r->data, ou -1 si rien ne correspond
 */
int roster_trouver(const roster_t *r, const char *saisie);

/** Nombre de personnages qui ne sont pas K.O. (donc engageables en combat). */
int roster_nb_en_forme(const roster_t *r);

#endif /* ROSTER_H */
