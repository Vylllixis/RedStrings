/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef SAUVEGARDE_H
    #define SAUVEGARDE_H

    #include "roster.h"

    #define FICHIER_SAUVEGARDE "personnages.txt"

/** Codes de retour de sauvegarder() et charger(). */
typedef enum {
    SAUVEGARDE_OK = 0,
    SAUVEGARDE_ERR_OUVERTURE,   /**< fopen a échoué. */
    SAUVEGARDE_ERR_ECRITURE,    /**< fprintf ou fclose a échoué. */
    SAUVEGARDE_ERR_FORMAT,      /**< fichier corrompu ou valeur invalide. */
    SAUVEGARDE_ERR_MEMOIRE      /**< allocation impossible. */
} sauvegarde_t;

/** Écrit un personnage par ligne : « nom pv atq def classe soin ».
 *  Les lignes sans soin (anciennes sauvegardes) restent lisibles. */
sauvegarde_t sauvegarder(const roster_t *r, const char *chemin);

/** Remplace le contenu de r par celui du fichier. En cas d'erreur, r n'est
 *  pas modifié. */
sauvegarde_t charger(roster_t *r, const char *chemin);

/**
 * Ajoute à r les personnages du fichier `chemin`, sans toucher à ceux qui y
 * sont déjà. Un personnage dont le nom existe déjà dans r est ignoré (celui
 * de r est gardé). Tout ou rien : si le fichier est invalide ou si la mémoire
 * manque, r reste exactement comme avant. `ajoutes` et `ignores` reçoivent le
 * nombre de personnages ajoutés et ignorés (0 et 0 en cas d'erreur).
 */
sauvegarde_t fusionner(roster_t *r, const char *chemin, int *ajoutes, int *ignores);

/** Message en français pour un code de retour. */
const char *sauvegarde_message(sauvegarde_t code);

#endif /* SAUVEGARDE_H */
