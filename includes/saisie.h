/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef SAISIE_H
    #define SAISIE_H

    #include <stddef.h>

/** Jette le reste de la ligne tapée, '\n' compris. */
void viderBuffer(void);

/** Affiche un message d'erreur en rouge. */
void afficherErreur(const char *message);

/** Lit une ligne (sans le '\n'). Renvoie -1 sur Ctrl+D. */
int lireLigne(char *buf, size_t taille);

/** Redemande jusqu'à un entier dans [min, max]. Renvoie -1 sur Ctrl+D. */
int lireEntier(const char *question, int min, int max, int *resultat);

#endif /* SAISIE_H */
