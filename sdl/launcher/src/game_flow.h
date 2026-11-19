#pragma once

#include <stdbool.h>

#include "engine/camera.h"
#include "engine/daynight.h"
#include "engine/game_menu.h"
#include "engine/input.h"
#include "engine/ui.h"
#include "play_mode.h"

/* Déroulé d'une partie (M8-04/05) autour du mode combat : menu principal au lancement (Continuer,
 * Nouvelle partie, Options, Quitter), pause (P / Start), sauvegarde, reprise, options et
 * remappage enregistrés. Fichiers dans le dossier de préférences de l'utilisateur
 * (SDL_GetPrefPath : ~/.local/share/RedStrings/game/ sous Linux) : savegame.rssv, options.txt. */

typedef struct GameFlow {
    EngMenu menu;
    EngGameOptions options;
    EngInput *input;
    char save_path[1024];
    char options_path[1024];
    double play_time;
    EngDayNight daynight; /* M8-07 : avance pendant la partie, figé dans les menus, sauvegardé */
    bool quit;
    bool started;        /* une partie est en cours (nouvelle ou reprise) */
    bool graphics_dirty; /* options graphiques à appliquer au renderer (par le lanceur) */
} GameFlow;

/* `dir` : dossier des fichiers (NULL : dossier de préférences). Ouvre le menu principal. Lit et
 * applique les options et les liaisons. */
bool game_flow_init(GameFlow *f, PlayMode *play, EngInput *input, const char *dir);

/* Lance directement une nouvelle partie (menu fermé) : option --new-game. */
void game_flow_start_new(GameFlow *f, PlayMode *play);

/* Un tick. true : la partie avance ce tick (sinon un menu est ouvert et le jeu est figé). */
bool game_flow_tick(GameFlow *f, PlayMode *play, const EngInputState *in, float dt);

void game_flow_draw(const GameFlow *f, EngUi *ui, const PlayMode *play, const EngCamera *camera,
                    float aspect, int width, int height);

/* Autotest sans fenêtre (--game-selftest) : nouvelle partie, combat, sauvegarde, menu principal,
 * « quitter » (tout est détruit), relance, Continuer : l'état revient à l'identique. */
bool game_flow_selftest(void);
