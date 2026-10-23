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
/**
 * Carte compacte du personnage `p` dans le rectangle `r`. `survol` (0..1)
 * intensifie le halo de la couleur de classe, le bord, l'ombre et les
 * crochets ; `marque` signale l'équipe de la carte (composition), ou NULL.
 */
void mini_carte(app_t *app, const personnage_t *p, sfFloatRect r,
    float survol, const marque_t *marque);
/** Position à l'écran d'un point de la grille (la grille est peut-être réduite). */
sfVector2f grille_vers_ecran(const grille_t *g, sfVector2f p);
/**
 * Grille de cartes animée et centrée, adaptée à la place visible.
 *
 * Survol : la carte se soulève, grossit de 7 % et s'incline légèrement vers
 * la souris ; elle est dessinée en dernier, donc toujours au-dessus de ses
 * voisines, et sa zone de clic ne change pas (pas de clignotement sur ses bords).
 * `ids` contient des indices dans app->equipe ; ceux hors de l'équipe sont ignorés.
 * @return l'indice de la carte cliquée, ou -1
 */
int grille_cartes(app_t *app, grille_t *g, const int *ids, int n, float y0,
    const marque_t *marques, int nb_marques);

#endif /* GUI_CARTE_H */
