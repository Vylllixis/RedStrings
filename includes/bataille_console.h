/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef BATAILLE_CONSOLE_H
    #define BATAILLE_CONSOLE_H

    #include "roster.h"

/** Compose les deux équipes puis joue le combat dans le terminal.
 *  Le roster n'est pas modifié : le combat travaille sur des copies. */
void menu_bataille(const roster_t *r);

#endif /* BATAILLE_CONSOLE_H */
