/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Primitives de dessin : formes, halos, texte, fond animé.
 */

#ifndef GUI_DESSIN_H
    #define GUI_DESSIN_H

    #include <SFML/Graphics.h>
    #include "gui_types.h"

void rect_arrondi(app_t *app, sfFloatRect r, float rayon, sfColor fond);
void rect_arrondi_bord(app_t *app, sfFloatRect r, float rayon, sfColor fond,
    sfColor bord);
void ombre(app_t *app, sfFloatRect r, float rayon, float force);
void lueur(app_t *app, sfVector2f centre, sfVector2f rayon, sfColor c);
void cercle(app_t *app, sfVector2f centre, float r, sfColor fond,
    sfColor bord, float epaisseur);
void polygone(app_t *app, const float *pts, int n, sfVector2f centre,
    float taille, float angle, sfColor c);
void ligne_epaisse(app_t *app, const sfVector2f *pts, int n, float epaisseur,
    sfColor c, int additif);
float texte(app_t *app, police_t p, const char *s, unsigned int taille,
    sfVector2f pos, sfColor c, alignement_t a);
float texte_echelle(app_t *app, police_t p, const char *s, unsigned int taille,
    sfVector2f pos, sfColor c, alignement_t a, float echelle);
float largeur_texte(app_t *app, police_t p, const char *s, unsigned int taille);
float position_caractere(app_t *app, police_t p, const char *s,
    unsigned int taille, int index);
void tronquer(app_t *app, police_t p, unsigned int taille, const char *src,
    float max, char *dst);
float etiquette(app_t *app, const char *s, sfVector2f pos, sfColor c,
    alignement_t a);
void dessiner_fond(app_t *app);

#endif /* GUI_DESSIN_H */
