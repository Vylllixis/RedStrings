#ifndef ENG_GAME_MENU_H
#define ENG_GAME_MENU_H

/* Menus du jeu (M8-04) et options (M8-10), LOGIQUE seulement : le rendu (ui_menu) affiche l'état,
 * ce module décide. Tout se fait sans souris :
 *   - choisir : stick / ZQSD, flèches du clavier, croix de la manette ;
 *   - valider : Entrée, Saut (Espace / A) ou Attaque (clic / X) ;
 *   - revenir : Échap, Esquive (Ctrl / B) ou Pause (P / Start) ; à la racine de la pause, revenir
 *     reprend la partie ; à la racine du menu principal, revenir ne fait rien (Échap ne ferme
 *     jamais le jeu) ;
 *   - en jeu, Échap ou Pause ouvre la pause.
 *
 * Écrans :
 *   Principal   Continuer (ou « Nouvelle partie » sans aucune sauvegarde), Parties, Options,
 *               Quitter.
 *   Parties     les trois emplacements, puis Retour. Sur un emplacement occupé : ses actions
 *               (écran Partie). Sur un emplacement libre : créer une partie (écran Nom).
 *   Partie      Jouer, Renommer, Dupliquer (dans le premier emplacement libre), Supprimer, Retour.
 *   Nom         saisie du nom (création ou renommage) : le texte tapé arrive par
 *               eng_menu_text_input ; Entrée valide, Échap annule. Les touches du clavier liées
 *               à des actions (Espace, ZQSD, P...) y sont du texte, pas des commandes. À la
 *               création, la première ligne choisit le MODE DE JEU (Aventure ou Créatif, gauche /
 *               droite) : il est enregistré avec la partie et ne change plus ensuite.
 *   Suppression confirmation ; le choix par défaut garde la partie.
 *   Pause       Reprendre, Sauvegarder, Options, Sauvegarder et quitter (vers le menu principal).
 *   Options, Contrôles : réglages, remappage (une ligne par action ; valider attend la prochaine
 *               touche pressée, Échap annule ; « Par défaut »).
 *
 * L'action renvoyée vise toujours l'emplacement `slot` ; c'est l'appelant (game_flow) qui lit et
 * écrit les fichiers, puis décrit les emplacements avec eng_menu_set_slot. */

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
    ENG_MENU_SAVES,  /* les trois emplacements */
    ENG_MENU_SLOT,   /* les actions de l'emplacement `slot` */
    ENG_MENU_NAME,   /* saisie du nom de l'emplacement `slot` (création ou renommage) */
    ENG_MENU_DELETE, /* confirmer la suppression de l'emplacement `slot` */
    ENG_MENU_SCREEN_COUNT
} EngMenuScreen;

typedef enum EngMenuAction {
    ENG_MENU_ACT_NONE = 0,
    ENG_MENU_ACT_RESUME,   /* menu fermé : la partie reprend */
    ENG_MENU_ACT_NEW_GAME, /* nouvelle partie dans `slot`, nommée `text`, en mode `creative` */
    ENG_MENU_ACT_CONTINUE, /* charger la sauvegarde de l'emplacement `slot` */
    ENG_MENU_ACT_SAVE,     /* enregistrer dans l'emplacement de la partie en cours */
    ENG_MENU_ACT_TO_MAIN,  /* « Sauvegarder et quitter » : enregistrer puis menu principal */
    ENG_MENU_ACT_QUIT,
    ENG_MENU_ACT_OPTIONS_CHANGED, /* une option a changé : l'appliquer (volumes, caméra...) */
    ENG_MENU_ACT_REMAPPED,        /* une action a été reliée à une nouvelle touche */
    ENG_MENU_ACT_RENAME_SLOT,     /* l'emplacement `slot` prend le nom `text` */
    ENG_MENU_ACT_DELETE_SLOT,     /* supprimer la partie de l'emplacement `slot` */
    ENG_MENU_ACT_COPY_SLOT        /* copier l'emplacement `slot` dans `slot_target` (libre) */
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
    bool ssao;               /* occlusion ambiante (finition M8-06) */
    float render_scale;      /* échelle de résolution : 0,5, 0,75 ou 1 (finition M8-06) */
} EngGameOptions;

#define ENG_MENU_MAX_ITEMS 20U
#define ENG_MENU_LABEL 64U

typedef enum EngMenuItemKind {
    ENG_MENU_ITEM_PLAIN = 0, /* une ligne à valider */
    ENG_MENU_ITEM_SLIDER,    /* une valeur de 0 à 1 (`frac`) : gauche / droite */
    ENG_MENU_ITEM_TOGGLE,    /* oui / non (`frac` : 1 ou 0) */
    ENG_MENU_ITEM_CHOICE     /* une valeur parmi quelques-unes : gauche / droite */
} EngMenuItemKind;

typedef struct EngMenuItem {
    char label[ENG_MENU_LABEL];
    char value[ENG_MENU_LABEL]; /* vide : pas de valeur affichée */
    bool enabled;
    EngMenuItemKind kind;
    float frac; /* curseur : position de 0 à 1 ; bascule : 0 ou 1 */
} EngMenuItem;

#define ENG_MENU_SLOTS 3U
#define ENG_MENU_NAME_MAX 32U   /* octets du nom d'une partie, zéro final compris (UTF-8) */
#define ENG_MENU_NAME_CHARS 22U /* caractères au plus */

typedef struct EngMenuSlot {
    bool used;                    /* une partie y est enregistrée */
    bool unreadable;              /* fichier abîmé : ni jouable ni renommable, supprimable */
    bool creative;                /* partie en mode créatif */
    char name[ENG_MENU_NAME_MAX]; /* nom de la partie */
    char summary[ENG_MENU_LABEL]; /* « Niv. 3 · 12 min » */
    char place[32];               /* où le joueur a enregistré (« Plaine ») ; vide : inconnu */
    char date[40];                /* dernière sauvegarde (« 22 nov. 2026 · 10:31 ») */
    uint32_t level;
    uint32_t bosses;     /* bit i : boss de la zone i vaincu */
    uint32_t boss_count; /* boss à vaincre (0 : pas de grande carte, rien d'affiché) */
} EngMenuSlot;

typedef enum EngMenuNamePurpose {
    ENG_MENU_NAME_CREATE = 0,
    ENG_MENU_NAME_RENAME
} EngMenuNamePurpose;

typedef struct EngMenu {
    EngMenuScreen screen;
    EngMenuScreen options_return; /* écran d'où l'on est entré dans Options */
    int32_t selected;
    bool has_save; /* une partie au moins est enregistrée et lisible : « Continuer » */
    EngMenuSlot slots[ENG_MENU_SLOTS];
    int32_t slot; /* emplacement visé par l'action renvoyée, et celui de la partie en cours */
    int32_t slot_target; /* ENG_MENU_ACT_COPY_SLOT : emplacement libre qui reçoit la copie */
    int32_t last_slot;   /* partie la plus récente : celle de « Continuer » */
    int32_t remap;       /* action en attente d'une touche, -1 sinon */
    uint32_t remap_serial;
    uint32_t nav_cooldown; /* ticks avant la prochaine répétition */
    float nav_prev_y;
    float nav_prev_x;
    /* saisie du nom (écran Nom) */
    char text[ENG_MENU_NAME_MAX];
    EngMenuNamePurpose name_purpose;
    bool text_fresh;      /* nom proposé, pas encore modifié : la première frappe le remplace */
    bool creative;        /* création : mode de jeu choisi (faux : aventure) */
    uint32_t text_serial; /* augmente à chaque modification du texte (animation du curseur) */
    /* souris : ligne survolée à la dernière image (-1 : aucune) et côté de sa valeur (-1 gauche,
     * +1 droite, 0 ailleurs) ; un clic hors de toute ligne ne valide rien */
    int32_t hover_item;
    int32_t hover_dir;
    EngGameOptions *options;
    EngInput *input; /* pour remapper (peut être NULL : pas de remappage) */
    char status[64]; /* message bref (« Partie sauvegardée ») */
    uint32_t status_ticks;
    uint32_t status_serial; /* augmente à chaque nouveau message */
    /* compteurs pour l'animation (menu_motion) : ils augmentent, rien ne les lit ici */
    uint32_t confirm_serial; /* une ligne a été validée */
    uint32_t adjust_serial;  /* une valeur a changé */
    uint32_t deny_serial;    /* validation refusée (ligne grisée, nom vide...) */
    int32_t adjust_dir;      /* sens du dernier réglage : -1 ou +1 */
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

/* Écran Nom : `utf8` est le texte tapé depuis le dernier appel ('\b' : effacer le dernier
 * caractère). Les caractères de contrôle sont ignorés ; le nom est borné à ENG_MENU_NAME_CHARS
 * caractères. Sans effet sur un autre écran. */
void eng_menu_text_input(EngMenu *m, const char *utf8);

/* Lignes de l'écran courant (pour le rendu et les tests). */
uint32_t eng_menu_items(const EngMenu *m, EngMenuItem *out, uint32_t max);
const char *eng_menu_title(const EngMenu *m);
const char *eng_action_label(EngAction a);
/* Nom lisible d'une liaison (« Espace », « clic gauche », « manette A ») dans `out`. */
void eng_binding_name(EngBindKind kind, int code, char *out, size_t capacity);
void eng_menu_set_status(EngMenu *m, const char *text);
/* Décrit l'emplacement `index` (0 à ENG_MENU_SLOTS - 1) ; `slot` NULL : libre. */
void eng_menu_set_slot(EngMenu *m, uint32_t index, const EngMenuSlot *slot);
/* Nom proposé pour une nouvelle partie dans l'emplacement `index` : « Partie 2 ». */
void eng_menu_default_name(uint32_t index, char *out, size_t capacity);
/* La souris survole la ligne `item` (-1 : aucune), du côté `dir` de sa valeur. Si elle a bougé
 * (`moved`) et que la ligne existe et n'est pas grisée, elle devient la ligne choisie. Un clic
 * (Attaque) la valide ensuite, comme au clavier. */
void eng_menu_hover(EngMenu *m, int32_t item, int32_t dir, bool moved);

#endif
