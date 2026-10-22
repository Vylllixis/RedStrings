/*
** ESGI PROJECT, 2026
** PROJET FIL ROUGE C Avancé
*/

/*
 * État propre à chaque scène : menu, création, liste, composition, arène.
 */

#ifndef GUI_ETAT_H
    #define GUI_ETAT_H

    #include <SFML/Graphics.h>
    #include "personnage.h"
    #include "bataille.h"
    #include "gui_config.h"
    #include "gui_types.h"

typedef struct {
    bouton_t boutons[4];
    ressort_t decalage[4];      /**< Glissement de l'option sélectionnée. */
    int selection;              /**< Option choisie au clavier ou à la souris. */
    int souris_bouge;           /**< La souris a bougé depuis l'image précédente. */
    sfVector2f souris_prec;
    float survol_equipe[MAX_PERSONNAGES];
} menu_t;

typedef struct {
    champ_t nom;
    glissiere_t stats[NB_STATS];
    int valeurs[NB_STATS];
    classe_t classe;
    bouton_t onglets[NB_CATEGORIES];
    bouton_t tuiles[NB_CLASSES];
    ressort_t lift[NB_CLASSES];
    bouton_t creer;
    bouton_t retour;
    float segments[NB_STATS];
    float affiche[NB_STATS];   /**< Statistiques affichées dans l'aperçu (animées). */
    int focus;          /**< Nom, glissières, classe ou bouton (FOCUS_* de la création). */
    float sortie;       /**< Compte à rebours après création, -1 sinon. */
    float flash;
    classe_t classe_vue;    /**< Classe affichée à l'image précédente. */
    classe_t classe_prec;   /**< Classe affichée avant le dernier changement. */
    float t_classe;         /**< Temps depuis le dernier changement de classe. */
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
    float segments[NB_STATS];
} liste_t;

/** Composition des deux équipes avant un combat. */
typedef struct {
    grille_t grille;
    bouton_t retour;
    bouton_t valider;
    int etape;                      /**< 0 : ton équipe, 1 : équipe adverse. */
    int ids[NB_EQUIPES][TAILLE_EQUIPE_MAX];     /**< Indices dans app->equipe. */
    int nb[NB_EQUIPES];
    int curseur;                    /**< Carte choisie au clavier. */
    float etape_anim;
    float depart;                   /**< Compte à rebours avant l'arène, -1 sinon. */
} selection_t;

/** États de l'écran de combat : seuls CHOIX_* acceptent une commande. */
typedef enum {
    VUE_INTRO,
    VUE_CHOIX_ACTION,
    VUE_CHOIX_CIBLE,
    VUE_ANIMATION,
    VUE_ORDINATEUR,
    VUE_FIN
} etat_vue_t;

/**
 * Ce que l'écran MONTRE d'un combattant. Le moteur a déjà appliqué l'action ;
 * ces valeurs rattrapent son état au rythme des animations (au moment de
 * l'impact), sans jamais recalculer ni modifier les PV réels.
 */
typedef struct {
    int pv_vis;             /**< PV affichés (mis à jour à l'impact). */
    float pv_barre;         /**< Barre de PV animée. */
    float pv_fantome;       /**< Trace des PV perdus, qui suit avec retard. */
    float delai_fantome;
    int ko_vis;
    float t_ko;
    int garde_vis;
    float garde_anim;       /**< 0..1 : apparition du bouclier. */
    float garde_choc;       /**< Éclat du bouclier quand il absorbe un coup. */
    float flash;
    float secousse;
    float lueur_soin;
    sfVector2f elan;        /**< Décalage pendant une attaque. */
    sfVector2f pos;         /**< Centre du personnage (calculé à chaque image). */
    sfVector2f sol;         /**< Point au sol, sous le personnage. */
    float rayon;
} visuel_t;

    #define MAX_POPUPS 12

/** Nombre qui s'envole au-dessus d'un combattant (« -24 », « +30 »). */
typedef struct {
    char texte[24];
    sfColor couleur;
    sfVector2f pos;
    float t;
    int actif;
} popup_t;

typedef struct {
    bataille_t bataille;                /**< État réel du combat (moteur). */
    personnage_t equipes[NB_EQUIPES][TAILLE_EQUIPE_MAX];   /**< Pour rejouer. */
    int tailles[NB_EQUIPES];
    visuel_t vis[NB_EQUIPES][TAILLE_EQUIPE_MAX];
    etat_vue_t etat;
    float t_etat;
    int action;                         /**< Action en cours de choix. */
    ref_t cibles[NB_EQUIPES * TAILLE_EQUIPE_MAX];
    int nb_cibles;
    int cible;                          /**< Indice dans cibles[]. */
    evenement_t file[MAX_EVENEMENTS];   /**< Événements à jouer, dans l'ordre. */
    int nb_file;
    int i_file;
    float t_evt;
    int impact_fait;                    /**< L'impact de l'événement courant a eu lieu. */
    popup_t popups[MAX_POPUPS];
    float t_tour;
    int journal_vu;                     /**< Lignes du journal déjà révélées. */
    bouton_t boutons[NB_ACTIONS];
    bouton_t rejouer;
    bouton_t quitter;
} arene_t;

#endif /* GUI_ETAT_H */
