/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#include "couleurs.h"
#include "my_string.h"
#include "personnage.h"

const info_classe_t classes[NB_CLASSES] = {
    [GUERRIER] = { "Guerrier", ROUGE   },
    [MAGE]     = { "Mage",     MAGENTA },
    [ARCHER]   = { "Archer",   VERT    },
};

int trouver_personnage(const personnage_t *tab, int size, const char *nom)
{
    for (int i = 0; i < size; i++) {
        if (my_strcmp(tab[i].nom, nom) == 0)
            return i;
    }
    return -1;
}
