/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef OUTILS_TESTS_H
    #define OUTILS_TESTS_H

    #include <stddef.h>

/* Outils communs aux fichiers de tests. */

/** Les écritures sur stdout (printf, write) vont dans un tampon jusqu'à capture_fin(). */
void capture_debut(void);
/** Rétablit stdout et copie ce qui a été écrit dans buf (terminé par '\0', tronqué à taille). */
void capture_fin(char *buf, size_t taille);
/** Le prochain fgets / getchar sur stdin lira `texte` ; un texte vide simule Ctrl+D. */
void entree_simulee(const char *texte);
/** stdout vers /dev/null jusqu'à muet_fin(sauve). */
int muet_commencer(void);
void muet_finir(int sauve);
/** Écrit un fichier texte (le test échoue s'il est impossible à écrire). */
void ecrire_fichier_test(const char *chemin, const char *contenu);
/** 1 si `texte` contient `morceau`. */
int contient(const char *texte, const char *morceau);

#endif /* OUTILS_TESTS_H */
