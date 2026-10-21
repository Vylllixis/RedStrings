/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef BOITE_H
    #define BOITE_H

    #include <stddef.h>

    /* LARGEUR_BOITE doit être égal au nombre de "─" dans LIGNE. */
    #define LARGEUR_BOITE 36
    /* Largeur des libellés alignés (« Points de vie », « Attaque »...). */
    #define LARGEUR_LIBELLE 13
    #define LIGNE "────────────────────────────────────"

void boite_haut(const char *couleur);
void boite_separateur(const char *couleur);
void boite_bas(const char *couleur);

/** Affiche une ligne au format printf, bordure droite alignée. */
void boite_ligne(const char *couleur, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

/** Nombre de colonnes occupées à l'écran (ignore les couleurs ANSI, un
 *  caractère UTF-8 compte pour 1). */
int largeur_visible(const char *texte);

/** Copie texte dans dst complété d'espaces jusqu'à largeur colonnes. */
void aligner_texte(char *dst, size_t taille, const char *texte, int largeur);

/** Ligne « libellé aligné + valeur au format printf » dans la boîte. */
void boite_champ(const char *couleur, const char *libelle,
    const char *format, ...) __attribute__((format(printf, 3, 4)));

#endif /* BOITE_H */
