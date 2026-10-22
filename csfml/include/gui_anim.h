/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Fonctions mathématiques d'animation : courbes, ressorts, couleurs.
 */

#ifndef GUI_ANIM_H
    #define GUI_ANIM_H

    #include <SFML/Graphics.h>
    #include "gui_types.h"

float bornerf(float v, float min, float max);
float lerpf(float a, float b, float t);
float progression(float t, float debut, float duree);
float sortie_cubique(float t);
float entree_cubique(float t);
float entree_sortie_cubique(float t);
float sortie_rebond(float t);
float approcher(float actuel, float cible, float vitesse, float dt);
void maj_ressort(ressort_t *r, float cible, float raideur, float amorti,
    float dt);
float aleatoire(float min, float max);
sfColor avec_alpha(sfColor c, float a);
sfColor melanger(sfColor a, sfColor b, float t);
sfColor griser(sfColor c, float t);

#endif /* GUI_ANIM_H */
