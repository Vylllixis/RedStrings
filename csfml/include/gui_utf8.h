/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Texte UTF-8 : décodage et encodage stricts (séquences tronquées, trop
 * longues, substituts et points hors Unicode refusés), sans dépendre de SFML.
 */

#ifndef GUI_UTF8_H
    #define GUI_UTF8_H

    #include <stdint.h>

/** Écrit `point` en UTF-8 dans `sortie` (4 octets au plus). Renvoie le nombre
 *  d'octets écrits, ou 0 si le point de code n'est pas valide. */
int utf8_encoder(uint32_t point, char sortie[4]);

/** Décode `s` dans `dst`, terminé par 0. Au plus `max - 1` points de code ;
 *  un octet invalide est ignoré. Renvoie le nombre de points de code. */
int utf8_decoder(const char *s, uint32_t *dst, int max);

/** Nombre de caractères (et non d'octets). */
int utf8_nb_caracteres(const char *s);

/** Retire le dernier caractère, même s'il occupe plusieurs octets. */
void utf8_supprimer_dernier(char *s);

/** Majuscules : ASCII et lettres accentuées du français (à → À, é → É…). */
void utf8_majuscules(char *s);

#endif /* GUI_UTF8_H */
