/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Cartes de personnage, fiches, avatars et libellés des classes et statistiques.
 */

#ifndef GUI_CARTE_H
    #define GUI_CARTE_H

    #include <SFML/Graphics.h>
    #include "personnage.h"
    #include "gui_types.h"

const info_classe_t *classe_info(classe_t c);
const char *nom_classe(classe_t c);
const char *nom_classe_maj(classe_t c);
sfColor couleur_classe(classe_t c);
void avatar(app_t *app, const personnage_t *p, sfVector2f centre, float r,
    float mort);
void barre_segments(app_t *app, sfFloatRect r, float segments, sfColor c);
sfColor couleur_stat(stat_t s);
const char *nom_stat(stat_t s);
const char *abrege_stat(stat_t s);
void fiche(app_t *app, const personnage_t *p, sfFloatRect r,
    const float segments[NB_STATS]);
void mini_carte(app_t *app, const personnage_t *p, sfFloatRect r,
    float survol, const marque_t *marque);
sfVector2f grille_vers_ecran(const grille_t *g, sfVector2f p);
int grille_cartes(app_t *app, grille_t *g, const int *ids, int n, float y0,
    const marque_t *marques, int nb_marques);

#endif /* GUI_CARTE_H */
