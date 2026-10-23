/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * L'application : fenêtre, ressources, entrées, roster et état des scènes.
 */

#ifndef GUI_APP_H
    #define GUI_APP_H

    #include <stddef.h>
    #include <SFML/Graphics.h>
    #include "roster.h"
    #include "gui_config.h"
    #include "gui_types.h"
    #include "gui_etat.h"
    #include "gui_audio.h"

struct app_s {
    mode_visuel_t mode;     /**< Fixé au lancement, jamais modifié ensuite. */
    yugioh_t yugioh;
    sfRenderTexture *cible; /**< Si non NULL, le dessin va dans cette texture. */
    float desature;         /**< 0..1 : portraits grisés (personnage K.O.). */
    sfRenderWindow *fenetre;
    affichage_t affichage;      /**< Mode de fenêtre courant. */
    affichage_t plein_ecran;    /**< Plein écran retrouvé par F11 (sans bordure au départ). */
    int affichage_demande;      /**< Changement à appliquer après les événements, -1 sinon. */
    sfVector2u taille_fenetre;  /**< Taille du mode fenêtré, retrouvée en sortant du plein écran. */
    float echelle;              /**< Pixels de l'écran par unité de l'interface. */
    sfFloatRect ecran;          /**< Zone visible, dans le repère des scènes (1280 × 720 centré). */
    sfView *vue;                /**< Vue des scènes, avec la secousse de l'écran. */
    sfView *vue_fixe;           /**< Même cadrage, sans secousse : fond, notifications, souris. */
    sfView *vue_zoom;           /**< Vue d'une zone mise à l'échelle (zoom_debut / zoom_fin). */
    sfVector2f souris_hors_zoom;
    float zoom;                 /**< Échelle de la zone en cours, 1 hors zoom. */
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
    audio_t audio;      /**< Sons, musique et réglages audio (src/audio/). */
    int ouvert;
};

/* app.c */
int creer_app(app_t *app, mode_visuel_t mode);
int chemin_ressource(const char *relatif, char *chemin, size_t taille);

/* affichage.c : fenêtre, modes d'affichage, échelle et vues */
int ouvrir_fenetre(app_t *app, affichage_t mode);
void appliquer_taille(app_t *app, unsigned int largeur, unsigned int hauteur);
void demander_affichage(app_t *app, affichage_t mode);
void appliquer_demande_affichage(app_t *app);
void basculer_plein_ecran(app_t *app);
void changer_type_plein_ecran(app_t *app);
void detruire_app(app_t *app);
void evenements(app_t *app);
void maj_app(app_t *app, float dt);
void changer_scene(app_t *app, scene_t scene);

#endif /* GUI_APP_H */
