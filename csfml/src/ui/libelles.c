/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Noms, abréviations et couleurs des classes et des statistiques.
 */

#include "personnage.h"
#include "my_string.h"
#include "gui_carte.h"
#include "gui_config.h"

/** Nom de la classe, pris dans la table classes[] partagée avec la console. */
const char *nom_classe(classe_t c)
{
    return classes[c].nom;
}

/**
 * Majuscules UTF-8 : ASCII, plus les lettres accentuées du français codées
 * sur deux octets (à → À, é → É, ï → Ï…), pour « ÉLÉMENTALISTE » ou « PRÊTRE ».
 */
static void majuscules_utf8(char *s)
{
    unsigned char *u = (unsigned char *)s;

    for (int i = 0; u[i] != '\0'; i++) {
        if (u[i] >= 'a' && u[i] <= 'z')
            u[i] -= 32;
        else if (u[i] == 0xC3 && u[i + 1] >= 0xA0 && u[i + 1] <= 0xBE
            && u[i + 1] != 0xB7)
            u[++i] -= 0x20;
    }
}

/** Nom en majuscules, calculé une fois par classe depuis classes[]. */
const char *nom_classe_maj(classe_t c)
{
    static char maj[NB_CLASSES][NOM_MAX];

    if (maj[c][0] == '\0') {
        my_strncpy(maj[c], classes[c].nom, NOM_MAX - 1);
        majuscules_utf8(maj[c]);
    }
    return maj[c];
}

/** Couleur, nom et abréviation de chaque statistique (mêmes partout). */
sfColor couleur_stat(stat_t s)
{
    const sfColor couleurs[NB_STATS] = {C_VERT, C_ROUGE, C_BLEU, C_SOIN};

    return couleurs[s];
}

const char *nom_stat(stat_t s)
{
    static const char *noms[NB_STATS] = {"POINTS DE VIE", "ATTAQUE", "DÉFENSE", "SOIN"};

    return noms[s];
}

const char *abrege_stat(stat_t s)
{
    static const char *noms[NB_STATS] = {"PV", "ATQ", "DEF", "SOI"};

    return noms[s];
}

/** Même couleur que dans le terminal : le RGB central de classes[]. */
sfColor couleur_classe(classe_t c)
{
    return (sfColor){classes[c].couleur.r, classes[c].couleur.g,
        classes[c].couleur.b, 255};
}
