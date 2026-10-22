/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * Types de base de l'interface : énumérations, entrées, composants réutilisables.
 */

#ifndef GUI_TYPES_H
    #define GUI_TYPES_H

    #include <SFML/Graphics.h>
    #include "personnage.h"
    #include "gui_config.h"

/* app_t est défini dans gui_app.h : ici seulement son nom, pour les modules de dessin. */
typedef struct app_s app_t;

typedef enum {
    POLICE_GRAS,
    POLICE_NORMALE,
    POLICE_SYMBOLES,
    NB_POLICES
} police_t;

typedef enum {
    ALIGNE_GAUCHE,
    ALIGNE_CENTRE,
    ALIGNE_DROITE
} alignement_t;

typedef enum {
    SCENE_MENU,
    SCENE_CREATION,
    SCENE_LISTE,
    SCENE_SELECTION,
    SCENE_ARENE,
    SCENE_AU_REVOIR
} scene_t;

typedef enum {
    CURSEUR_FLECHE,
    CURSEUR_MAIN,
    CURSEUR_TEXTE,
    NB_CURSEURS
} curseur_t;

/** Entrées clavier / souris de la frame courante. */
typedef struct {
    sfVector2f souris;
    int bas;                    /**< Bouton gauche maintenu. */
    int clic;                   /**< Bouton gauche pressé cette frame. */
    int dehors;                 /**< Souris hors de la fenêtre. */
    int touches[sfKeyCount];    /**< Touches pressées cette frame. */
    int shift;
    sfUint32 texte[MAX_SAISIE]; /**< Caractères tapés cette frame. */
    int nb_texte;
} entree_t;

typedef struct {
    sfVector2f pos;
    sfVector2f vit;
    float vie;
    float vie_max;
    float taille;
    float gravite;
    sfColor couleur;
} particule_t;

typedef struct {
    float x;
    float y;
    float vitesse;
    float phase;
    float taille;
} poussiere_t;

typedef struct {
    char message[TAILLE_TOAST];
    char icone[TAILLE_ICONE];
    sfColor couleur;
    float t;
    float y;
    int actif;
} toast_t;

/** Ressort amorti pour les mouvements organiques. */
typedef struct {
    float x;
    float v;
} ressort_t;

typedef struct {
    float survol;
    float appui;
} bouton_t;

typedef struct {
    char texte[NOM_MAX];    /**< Même taille qu'un nom de personnage. */
    int focus;
    float focus_anim;
    float caret_x;
    float tape;         /**< Temps depuis le dernier caractère tapé. */
    float secousse;
    float marge;        /**< Marge gauche du texte. */
} champ_t;

typedef struct {
    float survol;
    float affiche;      /**< Valeur affichée (animée). */
    int glisse;
} glissiere_t;

typedef struct {
    int id;
    sfColor couleur;
    const char *badge;
} marque_t;

typedef struct {
    ressort_t lift[MAX_PERSONNAGES];
    float survol[MAX_PERSONNAGES];
    sfVector2f pos[MAX_PERSONNAGES];    /**< Position animée de chaque carte. */
    int place[MAX_PERSONNAGES];
} grille_t;

#endif /* GUI_TYPES_H */
