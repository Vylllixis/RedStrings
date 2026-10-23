/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef COMPOSITION_CONSOLE_H
    #define COMPOSITION_CONSOLE_H

    #include "bataille.h"
    #include "roster.h"

/**
 * Demande jusqu'à TAILLE_EQUIPE_MAX noms (une ligne vide termine) et remplit
 * `equipe`. Refuse un nom introuvable, un personnage K.O. ou déjà engagé dans
 * cette équipe ou dans `autre` (NULL si aucune).
 * @return la taille de l'équipe, ou -1 sur Ctrl+D
 */
int choisir_equipe(const roster_t *r, const char *titre,
    personnage_t equipe[TAILLE_EQUIPE_MAX], const personnage_t *autre, int nb_autre);

#endif /* COMPOSITION_CONSOLE_H */
