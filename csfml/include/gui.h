/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

#ifndef GUI_H
    #define GUI_H

    #include <SFML/Graphics.h>
    #include "my_string.h"
#include "personnage.h"
    #include "personnage.h"

    #define LARGEUR 1280
    #define HAUTEUR 720
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
    int nbTexte;
} entree_t;

typedef struct {
    sfVector2f pos;
    sfVector2f vit;
    float vie;
    float vieMax;
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
    float focusAnim;
    float caretX;
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
    float survolEquipe[MAX_PERSONNAGES];
} menu_t;

typedef struct {
    champ_t nom;
    glissiere_t stats[3];
    int valeurs[3];
    Classe_t classe;
    bouton_t tuiles[3];
    ressort_t lift[3];
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
    float tModal;
    float segments[3];
} liste_t;

typedef struct {
    grille_t grille;
    bouton_t retour;
    int etape;          /**< 0 attaquant, 1 défenseur. */
    int idAtk;
    int idDef;
    float validation;
    float etapeAnim;
} selection_t;

typedef struct {
    int ids[2];         /**< 0 : attaquant (gauche), 1 : défenseur (droite). */
    float pvAffiche[MAX_PERSONNAGES];
    float pvFantome[MAX_PERSONNAGES];
    float delaiFantome[MAX_PERSONNAGES];
    float pvMax[MAX_PERSONNAGES];
    float flash[MAX_PERSONNAGES];
    float recul[MAX_PERSONNAGES];
    float ko[MAX_PERSONNAGES];  /**< Temps depuis le K.O., -1 si vivant. */
    float tAttaque;     /**< -1 si aucune attaque en cours. */
    int impact;
    float echange;      /**< -1 si aucune inversion en cours. */
    int tour;
    int aAttaque;
    float tResultat;
    int atk;
    int def;
    int degats;
    int pvRestants;
    int dernierAtk;
    int dernierDef;
    float tPop;
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
    sfTexture *texLueur;
    sfTexture *texVignette;
    sfSprite *sprite;
    sfCursor *curseurs[NB_CURSEURS];
    curseur_t curseurActuel;
    curseur_t curseurVoulu;
    sfClock *horloge;
    sfRenderStates additif;
    float temps;
    float dt;
    float dtJeu;        /**< dt ralenti pendant le "hitstop". */
    float hitstop;
    float secousse;
    float alpha;        /**< Opacité globale appliquée aux dessins. */
    float espacement;   /**< Espacement des lettres du prochain texte. */
    float contour;      /**< Contour du prochain texte. */
    float rotation;     /**< Rotation (degrés) du prochain texte. */
    int sourisBloquee;
    entree_t in;
    personnage_t persos[MAX_PERSONNAGES];
    int nbPersos;
    scene_t scene;
    scene_t sceneSuivante;
    float tScene;
    float tTransition;  /**< -1 si aucune transition. */
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
int creerApp(app_t *app);
void detruireApp(app_t *app);
void evenements(app_t *app);
void majApp(app_t *app, float dt);
void changerScene(app_t *app, scene_t scene);

/* anim.c */
float bornerf(float v, float min, float max);
float lerpf(float a, float b, float t);
float progression(float t, float debut, float duree);
float sortieCubique(float t);
float entreeCubique(float t);
float entreeSortieCubique(float t);
float sortieRebond(float t);
float approcher(float actuel, float cible, float vitesse, float dt);
void majRessort(ressort_t *r, float cible, float raideur, float amorti,
    float dt);
float aleatoire(float min, float max);
sfColor avecAlpha(sfColor c, float a);
sfColor melanger(sfColor a, sfColor b, float t);
sfColor griser(sfColor c, float t);

/* dessin.c */
void rectArrondi(app_t *app, sfFloatRect r, float rayon, sfColor fond);
void rectArrondiBord(app_t *app, sfFloatRect r, float rayon, sfColor fond,
    sfColor bord);
void ombre(app_t *app, sfFloatRect r, float rayon, float force);
void lueur(app_t *app, sfVector2f centre, sfVector2f rayon, sfColor c);
void cercle(app_t *app, sfVector2f centre, float r, sfColor fond,
    sfColor bord, float epaisseur);
void polygone(app_t *app, const float *pts, int n, sfVector2f centre,
    float taille, float angle, sfColor c);
void ligneEpaisse(app_t *app, const sfVector2f *pts, int n, float epaisseur,
    sfColor c, int additif);
float texte(app_t *app, police_t p, const char *s, unsigned int taille,
    sfVector2f pos, sfColor c, alignement_t a);
float texteEchelle(app_t *app, police_t p, const char *s, unsigned int taille,
    sfVector2f pos, sfColor c, alignement_t a, float echelle);
float largeurTexte(app_t *app, police_t p, const char *s, unsigned int taille);
float positionCaractere(app_t *app, police_t p, const char *s,
    unsigned int taille, int index);
void tronquer(app_t *app, police_t p, unsigned int taille, const char *src,
    float max, char *dst);
float etiquette(app_t *app, const char *s, sfVector2f pos, sfColor c,
    alignement_t a);
void dessinerFond(app_t *app);

/* particules.c */
void emettre(app_t *app, sfVector2f pos, int n, sfColor c, float vitesse,
    float taille);
void majParticules(app_t *app);
void dessinerParticules(app_t *app);

/* ui.c */
int survole(app_t *app, sfFloatRect r);
int bouton(app_t *app, bouton_t *b, sfFloatRect r, const char *label,
    const char *touche, const char *info, sfColor accent, int actif);
int boutonPlein(app_t *app, bouton_t *b, sfFloatRect r, const char *label,
    const char *touche, sfColor accent, int actif);
int boutonRetour(app_t *app, bouton_t *b, sfVector2f pos);
int boutonRond(app_t *app, bouton_t *b, sfVector2f centre, const char *sym);
int champTexte(app_t *app, champ_t *c, sfFloatRect r, const char *aide,
    int max);
int glissiere(app_t *app, glissiere_t *g, sfFloatRect r, const char *label,
    int *valeur, sfColor couleur, int focus);
float pastille(app_t *app, const char *s, sfVector2f pos, sfColor c,
    int plein);
void notifier(app_t *app, const char *icone, const char *message,
    sfColor c);
void dessinerToasts(app_t *app);
void enTete(app_t *app, bouton_t *retour, const char *titre,
    const char *sousTitre);

/* carte.c */
const char *nomClasse(Classe_t c);
const char *nomClasseMaj(Classe_t c);
sfColor couleurClasse(Classe_t c);
void embleme(app_t *app, Classe_t c, sfVector2f centre, float taille,
    sfColor couleur);
void avatar(app_t *app, const personnage_t *p, sfVector2f centre, float r,
    float mort);
void barreSegments(app_t *app, sfFloatRect r, float segments, sfColor c);
void fiche(app_t *app, const personnage_t *p, sfFloatRect r,
    const float segments[3]);
void miniCarte(app_t *app, const personnage_t *p, sfFloatRect r,
    float survol, const marque_t *marque);
int grilleCartes(app_t *app, grille_t *g, const int *ids, int n, float y0,
    const marque_t *marques, int nbMarques);

/* jeu.c */
int trouverPersonnage(personnage_t *tab, int nb, char *nom);
int appliquerAttaque(personnage_t *attaquant, personnage_t *cible);
int contientNom(const char *nom, const char *recherche);

/* scènes */
void entrerMenu(app_t *app);
void sceneMenu(app_t *app);
void entrerCreation(app_t *app);
void sceneCreation(app_t *app);
void entrerListe(app_t *app);
void sceneListe(app_t *app);
void entrerSelection(app_t *app);
void sceneSelection(app_t *app);
void entrerArene(app_t *app);
void sceneArene(app_t *app);
void sceneAuRevoir(app_t *app);

#endif /* GUI_H */
