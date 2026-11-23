#ifndef ENG_MENU_MOTION_H
#define ENG_MENU_MOTION_H

/* Animation des menus (2026-11-22) : état PUR, sans rendu ni GPU, avancé une fois par image par
 * eng_menu_motion_update. Le dessin (ui_menu) et la scène du titre (lanceur) ne font que le lire.
 *
 * Principe : rien n'apparaît ni ne change d'un coup. Chaque grandeur visible est soit un ressort
 * amorti qui poursuit sa cible (curseur, cartes, cadrage de la scène), soit une courbe d'entrée
 * décalée d'une ligne à l'autre (cascade), soit une impulsion qui s'éteint (corde pincée,
 * validation, refus). Les cibles se déduisent de l'écran ouvert et de la ligne choisie.
 *
 * Trois familles d'écrans :
 *   barre   menu principal : les lignes sont posées sur le fil rouge, en bas de l'écran ;
 *   cartes  Parties, Partie, Nom, Suppression : trois cartes suspendues au fil ; la carte visée
 *           vient à gauche et le panneau de ses actions s'ouvre à droite ;
 *   liste   Pause, Options, Contrôles : une colonne de lignes, qui défile si elle déborde.
 *
 * Unités : les positions sont en HAUTEURS d'écran, origine au centre, x vers la droite et y vers
 * le BAS (comme les pixels) ; les temps en secondes ; les angles en radians. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/game_menu.h"

typedef struct EngSpring {
    float x; /* valeur */
    float v; /* vitesse */
} EngSpring;

/* Courbes d'entrée : t hors de [0, 1] est ramené dans l'intervalle. */
float eng_ease_out_cubic(float t);
float eng_ease_in_out_cubic(float t);
/* dépasse 1 (d'environ 10 %) avant d'y revenir : l'élément « se pose » */
float eng_ease_out_back(float t);

/* Un pas de ressort amorti vers `target`. `hz` : fréquence propre ; `damping` : 1 = critique (sans
 * dépassement), < 1 : oscille. Stable quel que soit `dt` (découpé en sous-pas). */
void eng_spring_step(EngSpring *s, float target, float hz, float damping, float dt);

/* Entrée en cascade : progression (0 à 1) de l'élément `index`, qui commence `delay` secondes
 * après le précédent et dure `duration`. */
float eng_stagger(float time, uint32_t index, float delay, float duration);

typedef enum EngMenuFamily {
    ENG_MENU_FAMILY_NONE = 0,
    ENG_MENU_FAMILY_RAIL,
    ENG_MENU_FAMILY_CARDS,
    ENG_MENU_FAMILY_LIST
} EngMenuFamily;

EngMenuFamily eng_menu_family(EngMenuScreen screen);

/* Géométrie des cartes, en hauteurs d'écran (avant l'ajustement `fit` aux fenêtres étroites). */
#define ENG_MOTION_CARD_W 0.30F
#define ENG_MOTION_CARD_H 0.42F
#define ENG_MOTION_CARD_GAP 0.045F
#define ENG_MOTION_THREAD_Y (-0.23F) /* le fil auquel les cartes sont suspendues */
#define ENG_MOTION_OUT_TIME 0.16F    /* durée de sortie de l'écran quitté */

typedef struct EngMotionCard {
    EngSpring x; /* centre de la carte */
    EngSpring y;
    EngSpring scale;
    EngSpring alpha;
    EngSpring angle; /* balancement autour du nœud, cible 0 */
    float born;      /* secondes depuis que la carte est apparue ou a changé de contenu */
} EngMotionCard;

typedef struct EngMenuMotion {
    bool started;
    float clock;          /* secondes depuis le début (mouvements de repos) */
    EngMenuScreen screen; /* écran affiché */
    EngMenuScreen from;   /* écran quitté (dessiné pendant ENG_MOTION_OUT_TIME) */
    int32_t from_selected;
    float enter; /* secondes depuis l'entrée dans `screen` */
    float leave; /* secondes depuis la sortie de `from` */
    float open;  /* 0 (menu fermé) à 1 (ouvert) : voile et panneau de la pause */
    int32_t selected;
    float since_select;                 /* secondes depuis le dernier changement de ligne */
    EngSpring cursor;                   /* ligne choisie, en continu (curseur, perle du fil) */
    float emphasis[ENG_MENU_MAX_ITEMS]; /* 0 à 1 : mise en avant de chaque ligne */
    float pluck;        /* secondes depuis que le fil a été pincé (changement de ligne) */
    float confirm;      /* 1 à la validation, s'éteint */
    float deny;         /* 1 au refus, s'éteint (secousse) */
    float adjust;       /* 1 quand une valeur change, s'éteint */
    int32_t adjust_dir; /* -1 ou +1 */
    float typed;        /* 1 à chaque frappe, s'éteint */
    EngSpring scroll;   /* liste : première ligne visible, en continu */
    float scroll_first; /* ... sa cible */
    bool scroll_snap;   /* écran tout juste ouvert : la liste se place sans défiler */
    /* écrits par le dessin (ui_menu), qui connaît la taille de l'écran : lignes de la liste et
     * nombre de lignes visibles ; lus à l'image suivante pour le défilement */
    uint32_t list_count;
    uint32_t list_visible;
    float status;         /* 0 à 1 : visibilité du message */
    char status_text[64]; /* le dernier message (il reste lisible pendant qu'il s'efface) */
    float thread;         /* 0 à 1 : le fil des cartes est tendu de gauche à droite */
    float fit;            /* ajustement des cartes à la largeur de la fenêtre (<= 1) */
    EngMotionCard cards[ENG_MENU_SLOTS];
    /* la carte d'une partie supprimée tombe pendant ENG_MOTION_GHOST_TIME */
    EngMenuSlot ghost;
    float ghost_time; /* secondes depuis la suppression ; négatif : pas de fantôme */
    float ghost_x;
    float ghost_y;
    float ghost_scale;
    int32_t ghost_index;
    /* la scène du titre : mise en page du titre, cadrage, assombrissement */
    float title_menu;  /* 0 : écran de chargement ; 1 : menu (la corde de chargement s'efface) */
    EngSpring title_y; /* centre du titre, y vers le HAUT comme dans le shader */
    EngSpring title_w; /* demi-largeur du titre */
    EngSpring cam_x;   /* cadrage : déplacement du premier plan */
    EngSpring cam_y;
    EngSpring cam_zoom;
    EngSpring dim; /* assombrissement de la scène derrière les cartes et les listes */
    /* ce que le menu a déjà signalé */
    uint32_t seen_confirm;
    uint32_t seen_deny;
    uint32_t seen_adjust;
    uint32_t seen_text;
    uint32_t seen_status;
    bool seen_used[ENG_MENU_SLOTS];
    EngMenuSlot seen_slots[ENG_MENU_SLOTS];
} EngMenuMotion;

#define ENG_MOTION_GHOST_TIME 0.7F

void eng_menu_motion_init(EngMenuMotion *a);

/* Une image : `dt` secondes écoulées (borné à 50 ms : un à-coup ne fait pas sauter l'animation),
 * `aspect` = largeur / hauteur de l'écran. */
void eng_menu_motion_update(EngMenuMotion *a, const EngMenu *m, float dt, float aspect);

/* L'état au repos de l'écran courant, sans animation (tests, captures). */
void eng_menu_motion_settle(EngMenuMotion *a, const EngMenu *m, float aspect);

/* Centre visé de la carte `index` pour l'écran et la ligne donnés (hauteurs d'écran). */
void eng_menu_motion_card_target(const EngMenu *m, uint32_t index, float aspect, float fit,
                                 float out[2], float *scale, float *alpha);

/* Première ligne visible d'une liste de `count` lignes dont `visible` tiennent à l'écran, pour que
 * la ligne `selected` soit affichée avec une ligne de marge quand c'est possible. */
float eng_menu_motion_scroll_target(float current, int32_t selected, uint32_t count,
                                    uint32_t visible);

#endif
