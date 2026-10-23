/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef SAISIE_H
    #define SAISIE_H

    #include <stddef.h>

/** Jette le reste de la ligne tapée, '\n' compris. */
void vider_buffer(void);

/** Affiche un message d'erreur en rouge. */
void afficher_erreur(const char *message);

/** Mot à taper à n'importe quelle étape pour l'annuler (rien n'est enregistré). */
#define MOT_ANNULER "q"

/** Codes de retour des lectures : 0 si la saisie est valide, sinon... */
#define SAISIE_FIN (-1)         /**< l'entrée est fermée (Ctrl+D) */
#define SAISIE_ANNULEE (-2)     /**< le joueur a tapé MOT_ANNULER */

/** Lit une ligne (sans le '\n'). Renvoie -1 sur Ctrl+D. */
int lire_ligne(char *buf, size_t taille);

/**
 * Redemande jusqu'à un entier dans [min, max] ; la question rappelle que `q`
 * annule. Renvoie 0, SAISIE_FIN ou SAISIE_ANNULEE.
 */
int lire_entier(const char *question, int min, int max, int *resultat);

/** Comme lire_entier(), pour un menu : `q` n'y est pas accepté (jamais SAISIE_ANNULEE). */
int lire_entier_menu(const char *question, int min, int max, int *resultat);

/**
 * Lit un nom de personnage : non vide, sans espace, 29 caractères au plus.
 * Un nom refusé est redemandé avec la raison. `nom` doit faire NOM_MAX octets
 * au moins. Renvoie 0, SAISIE_FIN ou SAISIE_ANNULEE (`q` n'est donc pas un nom).
 */
int lire_nom(const char *question, char *nom, size_t taille);

/**
 * Pose une question oui/non (o, oui, n, non ; en majuscules aussi).
 * Une ligne vide, `q` ou Ctrl+D valent « non » : le choix sûr.
 * @return 1 pour oui, 0 pour non
 */
int confirmer(const char *question);

#endif /* SAISIE_H */
