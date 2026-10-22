/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Géométrie de l'écran pour les scènes : zone visible, ancrage aux bords
 * quand la fenêtre est plus étroite que le repère 1280 × 720, et zones
 * dessinées à une autre échelle (la souris suit la même conversion).
 */

#ifndef GUI_ECRAN_H
    #define GUI_ECRAN_H

    #include <SFML/Graphics.h>
    #include "gui_types.h"

/** Zone visible, dans le repère des scènes. */
sfFloatRect ecran_visible(const app_t *app);
/** Zone visible élargie : pour les voiles qui doivent couvrir tout l'écran. */
sfFloatRect ecran_couvrant(const app_t *app);
/** Décalage à appliquer à un élément ancré à gauche (positif si l'écran est étroit). */
float decal_gauche(const app_t *app);
/** Décalage à appliquer à un élément ancré à droite (négatif si l'écran est étroit). */
float decal_droite(const app_t *app);
/** Point du repère 1280 × 720 étiré sur toute la zone visible (décor, particules). */
sfVector2f vers_ecran(const app_t *app, float x, float y);

/**
 * Dessine la suite à l'échelle `k` autour de `pivot` (uniforme : rien n'est
 * déformé). La souris est convertie dans le même repère jusqu'à zoom_fin.
 */
void zoom_debut(app_t *app, sfVector2f pivot, float k);
void zoom_fin(app_t *app);

#endif /* GUI_ECRAN_H */
