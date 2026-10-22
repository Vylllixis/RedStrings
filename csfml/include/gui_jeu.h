/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Petites fonctions utilitaires de l'interface.
 */

#ifndef GUI_JEU_H
    #define GUI_JEU_H

    #include "gui_types.h"

    /* Seul argument reconnu : il active le mode Yu-Gi-Oh! (easter egg). */
    #define ARGUMENT_YUGIOH "Yu-Gi-Oh!"

int contient_nom(const char *nom, const char *recherche);
int mode_depuis_arguments(int argc, char **argv, mode_visuel_t *mode);

#endif /* GUI_JEU_H */
