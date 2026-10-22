/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Éléments d'interface de style HUD : panneaux, hexagones, radar, touches.
 */

#ifndef GUI_HUD_H
    #define GUI_HUD_H

    #include <SFML/Graphics.h>
    #include "personnage.h"
    #include "gui_types.h"

void panneau(app_t *app, sfFloatRect r, float coupe, sfColor haut, sfColor bas,
    sfColor bord);
void ombre_biseau(app_t *app, sfFloatRect r, float coupe, float force);
void crochets(app_t *app, sfFloatRect r, float longueur, sfColor c);
void losange(app_t *app, sfVector2f centre, float r, sfColor c);
void hexagone(app_t *app, sfVector2f centre, float r, float rotation,
    sfColor fond, sfColor bord);
void titre_section(app_t *app, const char *num, const char *label,
    sfVector2f pos, float largeur, sfColor accent);
void arc_cercle(app_t *app, sfVector2f centre, float rayon, float epaisseur,
    sfVector2f angles, sfColor c, int additif);
void graduations(app_t *app, sfVector2f centre, float rayon, int n,
    float longueur, float rotation, sfColor c);
void ellipse(app_t *app, sfVector2f centre, sfVector2f rayon, float epaisseur,
    sfColor c);
void radar_stats(app_t *app, sfVector2f centre, float rayon,
    const float v[NB_STATS], sfColor c);
float largeur_touche(app_t *app, const char *s);
float touche_clavier(app_t *app, const char *s, sfVector2f centre, sfColor c,
    float allume);

#endif /* GUI_HUD_H */
