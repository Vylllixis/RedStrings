#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "engine/camera.h"
#include "engine/daynight.h"
#include "engine/game_menu.h"
#include "engine/input.h"
#include "engine/menu_motion.h"
#include "engine/ui.h"
#include "play_mode.h"

/* Déroulé d'une partie (M8-04/05) autour du mode combat : écran-titre au lancement, pause (Échap,
 * P, Start), sauvegarde, reprise, options et remappage enregistrés. Fichiers dans le dossier de
 * préférences de l'utilisateur (SDL_GetPrefPath : ~/.local/share/RedStrings/game/ sous Linux) :
 * options.txt et TROIS emplacements de sauvegarde, partie1.rssv à partie3.rssv.
 *
 * Gestion des parties (2026-11-22, comme la liste des mondes de Minecraft) :
 *   - écran-titre : « Continuer » reprend la partie enregistrée le plus récemment ; « Parties »
 *     ouvre les trois emplacements ;
 *   - un emplacement libre crée une partie, à laquelle on donne un NOM et un MODE DE JEU
 *     (aventure ou créatif : invincible et vol libre) ; elle est enregistrée tout de suite. Le
 *     mode est enregistré avec la partie et ne se change pas en jeu ;
 *   - un emplacement occupé propose : Jouer, Renommer, Dupliquer (dans le premier emplacement
 *     libre), Supprimer (avec confirmation ; game_slots.h) ;
 *   - en jeu : « Sauvegarder » écrit dans l'emplacement de la partie en cours ; « Sauvegarder et
 *     quitter » enregistre puis revient à l'écran-titre ;
 *   - une ancienne sauvegarde unique (savegame.rssv) est COPIÉE dans l'emplacement 1 s'il est
 *     libre (l'ancien fichier reste en place).
 *
 * Fondus (game_flow_set_transitions, lancement à la main) : le titre s'éteint avant qu'une partie
 * commence, le jeu apparaît en fondu, et inversement pour revenir au titre ; l'action attend la
 * fin du fondu de sortie. Sans eux (tests), tout est immédiat. */

/* Monde ouvert (origine flottante) : les coordonnées LOCALES du joueur changent à chaque
 * recentrage de l'origine et d'un lancement à l'autre. Avec ces fonctions (game_flow_set_world),
 * la sauvegarde garde la position MONDE du joueur, et reprendre ou recommencer une partie le
 * replace au bon endroit, relief chargé autour de lui. Sans elles (arène, terrain) : les
 * coordonnées locales sont celles du monde. */
typedef struct GameFlowWorld {
    void *user;
    void (*to_world)(void *user, const float local[3], double world[3]);
    /* charge le relief autour de `world` (en bloquant) et y pose le joueur ; false : impossible */
    bool (*place)(void *user, const double world[3], float yaw);
    /* nom du lieu d'un point de la carte (« Plaine »), pour la carte de la partie ; peut être nul
     */
    void (*place_name)(void *user, const double world[3], char *out, size_t capacity);
    uint32_t boss_count; /* boss de la carte (0 : aucun affiché) */
} GameFlowWorld;

typedef struct GameFlow {
    EngMenu menu;
    EngMenuMotion motion; /* animation du menu, avancée à chaque image dessinée */
    EngGameOptions options;
    EngInput *input;
    GameFlowWorld world;  /* monde ouvert : position monde et replacement ; nulles sinon */
    char save_path[1024]; /* fichier de l'emplacement de la partie en cours */
    char slot_path[ENG_MENU_SLOTS][1024];
    char options_path[1024];
    char name[ENG_MENU_NAME_MAX]; /* nom de la partie en cours */
    bool creative; /* mode de la partie en cours : créatif (choisi à la création, fixé ensuite) */
    int32_t slot;          /* emplacement de la partie en cours (0 à 2) */
    uint64_t fade_last_ms; /* fondu d'entrée : instant de l'image précédente ; 0 : première */
    float fade;            /* fondu d'entrée : 1 (noir) à 0 (fini) ; game_flow_fade_in */
    float mouse[2]; /* dernière position de la souris : seul un mouvement choisit une ligne */
    double play_time;
    EngDayNight daynight; /* M8-07 : avance pendant la partie, figé dans les menus, sauvegardé */
    /* fondu de sortie : l'action `pending` s'exécute quand `cover_ticks` atteint `cover_total` */
    EngMenuAction pending;
    uint32_t cover_ticks;
    uint32_t cover_total;
    bool transitions; /* fondus entre le titre et le jeu */
    bool in_title;    /* écran-titre : aucune partie à l'écran derrière le menu */
    bool title_scene; /* la scène animée du titre est dessinée derrière le menu (lanceur) */
    bool quit;
    bool started;        /* une partie est en cours (nouvelle ou reprise) */
    bool graphics_dirty; /* options graphiques à appliquer au renderer (par le lanceur) */
} GameFlow;

/* Le plein écran enregistré dans les options (dossier de préférences), lu AVANT d'ouvrir la
 * fenêtre : elle naît alors en plein écran, au lieu d'y basculer au milieu du chargement.
 * `fallback` sans fichier d'options, ou si le fichier ne le dit pas. */
bool game_flow_saved_fullscreen(bool fallback);

/* `dir` : dossier des fichiers (NULL : dossier de préférences). Ouvre l'écran-titre. Lit et
 * applique les options et les liaisons ; `fullscreen_default` : plein écran quand les options
 * enregistrées n'en disent rien (vrai pour un lancement à la main). */
bool game_flow_init(GameFlow *f, PlayMode *play, EngInput *input, const char *dir,
                    bool fullscreen_default);

/* Monde ouvert : voir GameFlowWorld. `world` NULL : coordonnées locales. */
void game_flow_set_world(GameFlow *f, const GameFlowWorld *world);
/* Fondus entre le titre et le jeu ; `title_scene` : la scène du titre est dessinée par le lanceur
 * derrière le menu (sinon le menu assombrit le jeu et écrit le titre lui-même). */
void game_flow_set_transitions(GameFlow *f, bool transitions, bool title_scene);

/* Enregistre la partie en cours dans son emplacement, ou reprend celle de l'emplacement `slot`
 * (0 à 2), sans passer par le menu (autotest, raccourcis). false en cas d'échec. */
bool game_flow_save_now(GameFlow *f, PlayMode *play);
bool game_flow_load_slot(GameFlow *f, PlayMode *play, int32_t slot);

/* Lance directement une nouvelle partie (menu fermé) : option --new-game ; `creative` : en mode
 * créatif (option --creative). */
void game_flow_start_new(GameFlow *f, PlayMode *play, bool creative);

/* Un tick. true : la partie avance ce tick (sinon un menu est ouvert et le jeu est figé). */
bool game_flow_tick(GameFlow *f, PlayMode *play, const EngInputState *in, float dt);

/* Texte tapé au clavier depuis le dernier appel ('\b' : effacement) : le nom d'une partie. */
void game_flow_text(GameFlow *f, const char *utf8);
/* L'écran ouvert attend du texte (le lanceur active alors la saisie de la fenêtre). */
bool game_flow_wants_text(const GameFlow *f);

/* Le menu apparaît en fondu depuis le noir. */
void game_flow_fade_in(GameFlow *f);
/* L'écran-titre est affiché : le lanceur dessine la scène du titre et pas le jeu. */
bool game_flow_in_title(const GameFlow *f);
/* Progression (0 à 1) du fondu de sortie de l'écran-titre : la scène s'éteint. */
float game_flow_title_outro(const GameFlow *f);

/* Dessine le HUD ou le menu, et les fondus. `dt` : secondes depuis l'image précédente (animation
 * du menu). */
void game_flow_draw(GameFlow *f, EngUi *ui, const PlayMode *play, const EngCamera *camera,
                    float aspect, int width, int height, float dt);

/* Autotest sans fenêtre (--game-selftest) : création d'une partie nommée, combat, sauvegarde,
 * relance, Continuer ; puis les trois emplacements : renommer, dupliquer, supprimer. */
bool game_flow_selftest(void);
