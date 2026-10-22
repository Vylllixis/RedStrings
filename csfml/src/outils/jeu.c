/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include <ctype.h>
#include "my_string.h"
#include "gui_jeu.h"
#include "gui_types.h"

/** Recherche insensible à la casse de `recherche` dans `nom`. */
int contient_nom(const char *nom, const char *recherche)
{
    int j = 0;

    if (recherche[0] == '\0')
        return 1;
    for (int i = 0; nom[i] != '\0'; i++) {
        for (j = 0; recherche[j] != '\0' && nom[i + j] != '\0'; j++) {
            if (tolower((unsigned char)nom[i + j])
                != tolower((unsigned char)recherche[j]))
                break;
        }
        if (recherche[j] == '\0')
            return 1;
    }
    return 0;
}

/**
 * Mode visuel demandé au lancement : aucun argument → normal, exactement
 * « Yu-Gi-Oh! » → cartes. Renvoie -1 pour tout autre argument.
 */
int mode_depuis_arguments(int argc, char **argv, mode_visuel_t *mode)
{
    *mode = MODE_NORMAL;
    if (argc <= 1)
        return 0;
    if (argc == 2 && my_strcmp(argv[1], ARGUMENT_YUGIOH) == 0) {
        *mode = MODE_YUGIOH;
        return 0;
    }
    return -1;
}
