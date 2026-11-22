#pragma once

#include <stdbool.h>

#include "engine/camera.h"
#include "engine/daynight.h"
#include "engine/game_menu.h"
#include "engine/input.h"
#include "engine/ui.h"
#include "play_mode.h"

/* Déroulé d'une partie (M8-04/05) autour du mode combat : menu principal au lancement (Continuer,
 * Nouvelle partie, Charger une partie, Options, Quitter), pause (P / Start), sauvegarde, reprise,
 * options et remappage enregistrés. Fichiers dans le dossier de préférences de l'utilisateur
 * (SDL_GetPrefPath : ~/.local/share/RedStrings/game/ sous Linux) : options.txt et TROIS
 * emplacements de sauvegarde, partie1.rssv à partie3.rssv (2026-11-22).
 *
 *   - « Continuer » reprend la partie enregistrée le plus récemment ;
 *   - « Nouvelle partie » choisie au menu est enregistrée tout de suite dans son emplacement
 *     (après confirmation s'il était occupé) ; « Sauvegarder » (pause) écrit dans l'emplacement
 *     de la partie en cours ;
 *   - une ancienne sauvegarde unique (savegame.rssv) est COPIÉE dans l'emplacement 1 s'il est
 *     libre (l'ancien fichier reste en place). */

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
} GameFlowWorld;

typedef struct GameFlow {
    EngMenu menu;
    EngGameOptions options;
    EngInput *input;
    GameFlowWorld world;  /* monde ouvert : position monde et replacement ; nulles sinon */
    char save_path[1024]; /* fichier de l'emplacement de la partie en cours */
    char slot_path[ENG_MENU_SLOTS][1024];
    char options_path[1024];
    int32_t slot;          /* emplacement de la partie en cours (0 à 2) */
    uint64_t fade_last_ms; /* fondu d'entrée : instant de l'image précédente ; 0 : première */
    float fade;            /* fondu d'entrée : 1 (noir) à 0 (fini) ; game_flow_fade_in */
    float mouse[2]; /* dernière position de la souris : seul un mouvement choisit une ligne */
    double play_time;
    EngDayNight daynight; /* M8-07 : avance pendant la partie, figé dans les menus, sauvegardé */
    bool quit;
    bool started;        /* une partie est en cours (nouvelle ou reprise) */
    bool graphics_dirty; /* options graphiques à appliquer au renderer (par le lanceur) */
} GameFlow;

/* Le plein écran enregistré dans les options (dossier de préférences), lu AVANT d'ouvrir la
 * fenêtre : elle naît alors en plein écran, au lieu d'y basculer au milieu du chargement. false
 * sans fichier d'options. */
bool game_flow_saved_fullscreen(void);

/* `dir` : dossier des fichiers (NULL : dossier de préférences). Ouvre le menu principal. Lit et
 * applique les options et les liaisons. */
bool game_flow_init(GameFlow *f, PlayMode *play, EngInput *input, const char *dir);

/* Monde ouvert : voir GameFlowWorld. `world` NULL : coordonnées locales. */
void game_flow_set_world(GameFlow *f, const GameFlowWorld *world);

/* Enregistre la partie en cours dans son emplacement, ou reprend celle de l'emplacement `slot`
 * (0 à 2), sans passer par le menu (autotest, raccourcis). false en cas d'échec. */
bool game_flow_save_now(GameFlow *f, PlayMode *play);
bool game_flow_load_slot(GameFlow *f, PlayMode *play, int32_t slot);

/* Lance directement une nouvelle partie (menu fermé) : option --new-game. */
void game_flow_start_new(GameFlow *f, PlayMode *play);

/* Un tick. true : la partie avance ce tick (sinon un menu est ouvert et le jeu est figé). */
bool game_flow_tick(GameFlow *f, PlayMode *play, const EngInputState *in, float dt);

/* Le menu apparaît en fondu depuis le noir (après l'écran de chargement, qui finit au noir). */
void game_flow_fade_in(GameFlow *f);

void game_flow_draw(GameFlow *f, EngUi *ui, const PlayMode *play, const EngCamera *camera,
                    float aspect, int width, int height);

/* Autotest sans fenêtre (--game-selftest) : nouvelle partie, combat, sauvegarde, menu principal,
 * « quitter » (tout est détruit), relance, Continuer : l'état revient à l'identique. */
bool game_flow_selftest(void);
