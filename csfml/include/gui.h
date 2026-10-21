/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef GUI_H
    #define GUI_H

    #include <SFML/Graphics.h>
    #include "my_string.h"
    #include "personnage.h"
    #include "roster.h"
    #include "sauvegarde.h"

    #define LARGEUR 1280
    #define HAUTEUR 720
    /* Le roster est illimité, mais l'écran n'a de place que pour 10 cartes. */
    #define MAX_PERSONNAGES 10
    #define MAX_PARTICULES 900
    #define MAX_TOASTS 4
    #define NB_POUSSIERES 70
    #define PI 3.14159265f

/* Palette */
    #define C_FOND          ((sfColor){9, 11, 24, 255})
    #define C_SURFACE       ((sfColor){22, 26, 46, 255})
    #define C_SURFACE_HAUTE ((sfColor){33, 39, 68, 255})
    #define C_CHAMP         ((sfColor){14, 17, 33, 240})
    #define C_BORD          ((sfColor){255, 255, 255, 22})
    #define C_TEXTE         ((sfColor){236, 240, 255, 255})
    #define C_TEXTE_DOUX    ((sfColor){150, 160, 192, 255})
    #define C_TEXTE_FAIBLE  ((sfColor){92, 102, 136, 255})
    #define C_BLANC         ((sfColor){255, 255, 255, 255})
    #define C_NOIR          ((sfColor){0, 0, 0, 255})
    #define C_TRANSPARENT   ((sfColor){0, 0, 0, 0})
    #define C_CYAN          ((sfColor){56, 189, 248, 255})
    #define C_ROUGE         ((sfColor){244, 63, 94, 255})
    #define C_VERT          ((sfColor){52, 211, 153, 255})
    #define C_BLEU          ((sfColor){96, 165, 250, 255})
    #define C_OR            ((sfColor){251, 191, 36, 255})
    #define C_GUERRIER      ((sfColor){239, 68, 68, 255})
    #define C_MAGE          ((sfColor){217, 70, 239, 255})
    #define C_ARCHER        ((sfColor){34, 197, 94, 255})

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
    sfUint32 texte[32];         /**< Caractères tapés cette frame. */
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
    char message[96];
    char icone[8];
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
    char texte[30];
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

typedef struct {
    bouton_t boutons[4];
    float survol_equipe[MAX_PERSONNAGES];
} menu_t;

typedef struct {
    champ_t nom;
    glissiere_t stats[3];
    int valeurs[3];
    classe_t classe;
    bouton_t tuiles[NB_CLASSES];
    ressort_t lift[NB_CLASSES];
    bouton_t creer;
    bouton_t retour;
    float segments[3];
    int focus;          /**< 0 nom, 1-3 stats, 4 classe, 5 bouton. */
    float sortie;       /**< Compte à rebours après création, -1 sinon. */
    float flash;
} creation_t;

typedef struct {
    grille_t grille;
    champ_t recherche;
    bouton_t retour;
    bouton_t fermer;
    bouton_t precedent;
    bouton_t suivant;
    int ouvert;         /**< Personnage affiché, -1 si aucun. */
    int dernier;
    float modal;
    float t_modal;
    float segments[3];
} liste_t;

typedef struct {
    grille_t grille;
    bouton_t retour;
    int etape;          /**< 0 attaquant, 1 défenseur. */
    int id_atq;
    int id_def;
    float validation;
    float etape_anim;
} selection_t;

typedef struct {
    int ids[2];         /**< 0 : attaquant (gauche), 1 : défenseur (droite). */
    float pv_affiche[MAX_PERSONNAGES];
    float pv_fantome[MAX_PERSONNAGES];
    float delai_fantome[MAX_PERSONNAGES];
    float pv_max[MAX_PERSONNAGES];
    float flash[MAX_PERSONNAGES];
    float recul[MAX_PERSONNAGES];
    float ko[MAX_PERSONNAGES];  /**< Temps depuis le K.O., -1 si vivant. */
    float t_attaque;     /**< -1 si aucune attaque en cours. */
    int impact;
    float echange;      /**< -1 si aucune inversion en cours. */
    int tour;
    int a_attaque;
    float t_resultat;
    int atq;
    int def;
    int degats;
    int pv_restants;
    int dernier_atq;
    int dernier_def;
    float t_pop;
    float vs;
    bouton_t attaquer;
    bouton_t inverser;
    bouton_t retour;
} arene_t;

typedef struct {
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
    toast_t toasts[MAX_TOASTS];
    menu_t menu;
    creation_t creation;
    liste_t liste;
    selection_t selection;
    arene_t arene;
    int ouvert;
} app_t;

/* app.c */
int creer_app(app_t *app);
void detruire_app(app_t *app);
void evenements(app_t *app);
void maj_app(app_t *app, float dt);
void changer_scene(app_t *app, scene_t scene);

/* anim.c */
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

/* dessin.c */
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

/* particules.c */
void emettre(app_t *app, sfVector2f pos, int n, sfColor c, float vitesse,
    float taille);
void maj_particules(app_t *app);
void dessiner_particules(app_t *app);

/* ui.c */
int survole(app_t *app, sfFloatRect r);
int bouton(app_t *app, bouton_t *b, sfFloatRect r, const char *label,
    const char *touche, const char *info, sfColor accent, int actif);
int bouton_plein(app_t *app, bouton_t *b, sfFloatRect r, const char *label,
    const char *touche, sfColor accent, int actif);
int bouton_retour(app_t *app, bouton_t *b, sfVector2f pos);
int bouton_rond(app_t *app, bouton_t *b, sfVector2f centre, const char *sym);
int champ_texte(app_t *app, champ_t *c, sfFloatRect r, const char *aide,
    int max);
int glissiere(app_t *app, glissiere_t *g, sfFloatRect r, const char *label,
    int *valeur, sfColor couleur, int focus);
float pastille(app_t *app, const char *s, sfVector2f pos, sfColor c,
    int plein);
void notifier(app_t *app, const char *icone, const char *message,
    sfColor c);
void dessiner_toasts(app_t *app);
void en_tete(app_t *app, bouton_t *retour, const char *titre,
    const char *sous_titre);

/* carte.c */
const char *nom_classe(classe_t c);
const char *nom_classe_maj(classe_t c);
sfColor couleur_classe(classe_t c);
void embleme(app_t *app, classe_t c, sfVector2f centre, float taille,
    sfColor couleur);
void avatar(app_t *app, const personnage_t *p, sfVector2f centre, float r,
    float mort);
void barre_segments(app_t *app, sfFloatRect r, float segments, sfColor c);
void fiche(app_t *app, const personnage_t *p, sfFloatRect r,
    const float segments[3]);
void mini_carte(app_t *app, const personnage_t *p, sfFloatRect r,
    float survol, const marque_t *marque);
int grille_cartes(app_t *app, grille_t *g, const int *ids, int n, float y0,
    const marque_t *marques, int nb_marques);

/* jeu.c */
int contient_nom(const char *nom, const char *recherche);

/* scènes */
void entrer_menu(app_t *app);
void scene_menu(app_t *app);
void entrer_creation(app_t *app);
void scene_creation(app_t *app);
void entrer_liste(app_t *app);
void scene_liste(app_t *app);
void entrer_selection(app_t *app);
void scene_selection(app_t *app);
void entrer_arene(app_t *app);
void scene_arene(app_t *app);
void scene_au_revoir(app_t *app);

#endif /* GUI_H */
