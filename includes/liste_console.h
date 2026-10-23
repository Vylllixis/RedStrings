/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef LISTE_CONSOLE_H
    #define LISTE_CONSOLE_H

    #include "roster.h"

/** Un groupe de personnages à signaler dans la liste (une équipe en cours de composition). */
typedef struct {
    const personnage_t *membres;    /**< les personnages du groupe (peut être NULL si n = 0) */
    int n;                          /**< combien il y en a */
    const char *libelle;            /**< texte affiché à droite de leur ligne */
    const char *couleur;            /**< code couleur ANSI du libellé */
} groupe_liste_t;

/**
 * Affiche tous les personnages avec leur ID (leur position dans la liste, à
 * partir de 1, que roster_trouver() accepte à la place d'un nom), puis leur
 * nom, leur classe, leurs PV et leur état à droite.
 * Un personnage K.O. est marqué « K.O. » ; celui d'un groupe porte le
 * libellé de ce groupe. `groupes` peut être NULL (nb_groupes = 0).
 */
void afficher_liste(const roster_t *r, const groupe_liste_t *groupes, int nb_groupes);

#endif /* LISTE_CONSOLE_H */
