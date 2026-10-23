/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef COMPOSITION_CONSOLE_H
    #define COMPOSITION_CONSOLE_H

    #include "bataille.h"
    #include "roster.h"
    #include "saisie.h"

/**
 * Demande jusqu'à TAILLE_EQUIPE_MAX noms (une ligne vide termine) et remplit
 * `equipe`. Refuse un nom introuvable, un personnage K.O. ou déjà engagé dans
 * cette équipe ou dans `autre` (NULL si aucune).
 * `q` annule la composition.
 * @return la taille de l'équipe, SAISIE_ANNULEE (q) ou SAISIE_FIN (Ctrl+D)
 */
int choisir_equipe(const roster_t *r, const char *titre,
    personnage_t equipe[TAILLE_EQUIPE_MAX], const personnage_t *autre, int nb_autre);

#endif /* COMPOSITION_CONSOLE_H */
