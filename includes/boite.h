/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/**
 * @file boite.h
 * @brief Cadres arrondis dessinés dans le terminal.
 *
 * ╭────╮  boiteHaut
 * │ .. │  boiteLigne
 * ├────┤  boiteSeparateur
 * ╰────╯  boiteBas
 */

#ifndef BOITE_H
    #define BOITE_H

    /* LARGEUR_BOITE doit être égal au nombre de "─" dans LIGNE. */
    #define LARGEUR_BOITE 36
    #define LIGNE "────────────────────────────────────"

void boiteHaut(const char *couleur);
void boiteSeparateur(const char *couleur);
void boiteBas(const char *couleur);

/** Affiche une ligne au format printf, bordure droite alignée. */
void boiteLigne(const char *couleur, const char *format, ...);

#endif /* BOITE_H */
