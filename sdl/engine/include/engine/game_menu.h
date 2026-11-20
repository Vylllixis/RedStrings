#ifndef ENG_GAME_MENU_H
#define ENG_GAME_MENU_H

/* Menus du jeu (M8-04) et options (M8-10), LOGIQUE seulement : le rendu (ui_game) affiche l'état,
 * ce module décide. Navigation complète sans souris : stick / ZQSD haut-bas pour choisir,
 * gauche-droite pour régler une valeur, Saut (Espace / A) ou Attaque (clic / X) pour valider,
 * Esquive (Ctrl / B) pour revenir, Pause (P / Start) pour ouvrir ou fermer la pause.
 *
 * Écrans : Principal (Continuer, Nouvelle partie, Options, Quitter), Pause (Reprendre,
 * Sauvegarder, Options, Menu principal), Options (volumes par bus, sensibilité, inversion Y,
 * taille du texte, contraste élevé, anticrénelage, bloom, Contrôles...), Contrôles (une ligne par
 * action ; valider attend la prochaine touche pressée ; « Par défaut »). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/input.h"
#include "engine/input_state.h"
#include "engine/mixer.h"

typedef enum EngMenuScreen {
    ENG_MENU_CLOSED = 0,
    ENG_MENU_MAIN,
    ENG_MENU_PAUSE,
    ENG_MENU_OPTIONS,
    ENG_MENU_CONTROLS,
    ENG_MENU_SCREEN_COUNT
} EngMenuScreen;

typedef enum EngMenuAction {
    ENG_MENU_ACT_NONE = 0,
    ENG_MENU_ACT_RESUME, /* menu fermé : la partie reprend */
    ENG_MENU_ACT_NEW_GAME,
    ENG_MENU_ACT_CONTINUE, /* charger la sauvegarde */
    ENG_MENU_ACT_SAVE,
    ENG_MENU_ACT_TO_MAIN,
    ENG_MENU_ACT_QUIT,
    ENG_MENU_ACT_OPTIONS_CHANGED, /* une option a changé : l'appliquer (volumes, caméra...) */
    ENG_MENU_ACT_REMAPPED         /* une action a été reliée à une nouvelle touche */
} EngMenuAction;

typedef struct EngGameOptions {
    float master;
    float volume[ENG_BUS_COUNT];
    float sensitivity; /* ]0 ; 4] */
    bool invert_y;
    float text_scale; /* 0,75 à 2 */
    bool high_contrast;
    bool fxaa;               /* graphismes (M8-06) */
    float bloom;             /* 0 à 1 */
    bool vsync;              /* synchronisation verticale */
    bool fullscreen;         /* plein écran */
    uint32_t shadow_quality; /* 0 bas (sans filtre), 1 moyen, 2 haut */
    uint32_t view_distance;  /* 0 proche, 1 moyenne, 2 loin (monde : 1, 2 ou 3 cellules) */
} EngGameOptions;

#define ENG_MENU_MAX_ITEMS 20U
#define ENG_MENU_LABEL 48U

typedef struct EngMenuItem {
    char label[ENG_MENU_LABEL];
    char value[ENG_MENU_LABEL]; /* vide : pas de valeur affichée */
    bool enabled;
} EngMenuItem;

typedef struct EngMenu {
    EngMenuScreen screen;
    EngMenuScreen options_return; /* écran d'où l'on est entré dans Options */
    int32_t selected;
    bool has_save; /* « Continuer » actif ? */
    int32_t remap; /* action en attente d'une touche, -1 sinon */
    uint32_t remap_serial;
    uint32_t nav_cooldown; /* ticks avant la prochaine répétition du stick */
    float nav_prev_y;
    float nav_prev_x;
    EngGameOptions *options;
    EngInput *input; /* pour remapper (peut être NULL : pas de remappage) */
    char status[64]; /* message bref (« Partie sauvegardée ») */
    uint32_t status_ticks;
} EngMenu;

void eng_options_default(EngGameOptions *o);
bool eng_options_valid(const EngGameOptions *o);

/* Texte « clé = valeur » + lignes « bind ACTION GENRE CODE » (liaisons de `input`, peut être
 * NULL). Retourne les octets écrits, 0 si la place manque. */
size_t eng_options_save(const EngGameOptions *o, const EngInput *input, char *out, size_t capacity);
/* Valeurs invalides ou inconnues : ignorées une par une (journalisées) ; les liaisons lues
 * remplacent celles de l'action concernée. */
bool eng_options_load(EngGameOptions *o, EngInput *input, const char *text);

void eng_menu_init(EngMenu *m, EngGameOptions *options, EngInput *input);
void eng_menu_open(EngMenu *m, EngMenuScreen screen);
bool eng_menu_is_open(const EngMenu *m);

/* Un tick. `in` est l'état d'entrée du tick ; pendant un menu ouvert, l'appelant ne doit PAS
 * passer ces entrées au jeu. Retourne l'action à exécuter par l'appelant. */
EngMenuAction eng_menu_update(EngMenu *m, const EngInputState *in);

/* Lignes de l'écran courant (pour le rendu et les tests). */
uint32_t eng_menu_items(const EngMenu *m, EngMenuItem *out, uint32_t max);
const char *eng_menu_title(const EngMenu *m);
const char *eng_action_label(EngAction a);
void eng_menu_set_status(EngMenu *m, const char *text);

#endif
