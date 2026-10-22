/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * L'application : fenêtre, ressources, entrées, roster et état des scènes.
 */

#ifndef GUI_APP_H
    #define GUI_APP_H

    #include <SFML/Graphics.h>
    #include "roster.h"
    #include "gui_config.h"
    #include "gui_types.h"
    #include "gui_etat.h"

struct app_s {
    sfRenderWindow *fenetre;
    sfView *vue;
    sfFont *polices[NB_POLICES];
    sfText *texte;
    sfConvexShape *forme;
    sfCircleShape *cercle;
    sfTexture *tex_lueur;
    sfTexture *tex_vignette;
    sfSprite *sprite;
    sfCursor *curseurs[NB_CURSEURS];
    curseur_t curseur_actuel;
    curseur_t curseur_voulu;
    sfClock *horloge;
    sfRenderStates additif;
    float temps;
    float dt;
    float dt_jeu;        /**< dt ralenti pendant le "hitstop". */
    float hitstop;
    float secousse;
    float alpha;        /**< Opacité globale appliquée aux dessins. */
    float espacement;   /**< Espacement des lettres du prochain texte. */
    float contour;      /**< Contour du prochain texte. */
    float rotation;     /**< Rotation (degrés) du prochain texte. */
    int souris_bloquee;
    entree_t in;
    roster_t equipe;    /**< Même roster que la console (roster.c). */
    scene_t scene;
    scene_t scene_suivante;
    float t_scene;
    float t_transition;  /**< -1 si aucune transition. */
    particule_t particules[MAX_PARTICULES];
    poussiere_t poussieres[NB_POUSSIERES];
    poussiere_t etoiles[NB_ETOILES];   /**< Plan le plus lointain du fond. */
    toast_t toasts[MAX_TOASTS];
    menu_t menu;
    creation_t creation;
    liste_t liste;
    selection_t selection;
    arene_t arene;
    int ouvert;
};

/* app.c */
int creer_app(app_t *app);
void detruire_app(app_t *app);
void evenements(app_t *app);
void maj_app(app_t *app, float dt);
void changer_scene(app_t *app, scene_t scene);

#endif /* GUI_APP_H */
