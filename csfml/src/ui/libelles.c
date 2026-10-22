/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
** ╭─ libelles.c ─────────────────────────────────────────────────────────
** │ But         Noms, abréviations et couleurs des classes et des
** │             statistiques. Tout vient de la table classes[] partagée
** │             avec la console.
** │ Contenu     classe_info, nom_classe, nom_classe_maj, couleur_stat,
** │             nom_stat, abrege_stat, couleur_classe
** ╰──────────────────────────────────────────────────────────────────────
*/

#include <stdio.h>
#include "personnage.h"
#include "gui_carte.h"
#include "gui_config.h"
#include "gui_utf8.h"

/**
 * Entrée de la table classes[] (partagée avec la console). Un identifiant hors
 * table, venu d'un état corrompu, retombe sur la première classe au lieu de
 * lire hors du tableau.
 */
const info_classe_t *classe_info(classe_t c)
{
    return &classes[(unsigned)c < NB_CLASSES ? c : 0];
}

const char *nom_classe(classe_t c)
{
    return classe_info(c)->nom;
}

/** Nom en majuscules, calculé une fois par classe depuis classes[]. */
const char *nom_classe_maj(classe_t c)
{
    static char maj[NB_CLASSES][NOM_MAX];
    int i = (unsigned)c < NB_CLASSES ? (int)c : 0;

    if (maj[i][0] == '\0') {
        snprintf(maj[i], sizeof(maj[i]), "%s", classes[i].nom);
        utf8_majuscules(maj[i]);
    }
    return maj[i];
}

/** Couleur, nom et abréviation de chaque statistique (mêmes partout). */
sfColor couleur_stat(stat_t s)
{
    const sfColor couleurs[NB_STATS] = {C_VERT, C_ROUGE, C_BLEU, C_SOIN};

    return couleurs[(unsigned)s < NB_STATS ? s : 0];
}

const char *nom_stat(stat_t s)
{
    static const char *noms[NB_STATS] = {"POINTS DE VIE", "ATTAQUE", "DÉFENSE", "SOIN"};

    return noms[(unsigned)s < NB_STATS ? s : 0];
}

const char *abrege_stat(stat_t s)
{
    static const char *noms[NB_STATS] = {"PV", "ATQ", "DEF", "SOI"};

    return noms[(unsigned)s < NB_STATS ? s : 0];
}

/** Même couleur que dans le terminal : le RGB central de classes[]. */
sfColor couleur_classe(classe_t c)
{
    const info_classe_t *info = classe_info(c);

    return (sfColor){info->couleur.r, info->couleur.g, info->couleur.b, 255};
}
