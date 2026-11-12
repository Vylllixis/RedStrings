#include <string.h>

#include "bridge.h"
#include "engine/assert.h"
#include "engine/game_api.h"
#include "engine/log.h"

/* ── Constantes à modifier pour essayer le rechargement à chaud ──
 * Change une valeur, lance `cmake --build --preset debug` pendant que le jeu tourne : l'effet
 * apparaît en une fraction de seconde, sans relancer, et le carré reste où il est.
 *
 * Commandes : ZQSD/WASD ou stick gauche = déplacer ; Maj ou clic de stick = courir ;
 * Ctrl ou B/Cercle = esquive ; clic gauche ou X/Carré (maintenu) = carré clair ; P ou Start =
 * pause. */
#define MOVE_SPEED 0.45 /* largeurs d'écran par seconde */
#define SPRINT_FACTOR 2.0
#define DODGE_DISTANCE 0.12
#define SQUARE_SIZE 0.125 /* côté, en fraction de la hauteur de la fenêtre */
#define SQUARE_R 230
#define SQUARE_G 180
#define SQUARE_B 60

/* Tout l'état du jeu : alloué et conservé par le moteur (voir les règles dans game_api.h). */
typedef struct GameState {
    double previous_x;
    double previous_y;
    double current_x;
    double current_y;
    uint64_t ticks;
    uint32_t reloads;
    uint32_t attacks;
    uint32_t attack_held; /* 0/1 : copie de l'action, pour le rendu */
    uint32_t paused;
} GameState;

static double clamp01(double v)
{
    return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

static void game_init(void *state, const EngServices *services)
{
    GameState *game = state;
    gm_bind_services(services);
    game->current_x = 0.5;
    game->current_y = 0.5;
    game->previous_x = 0.5;
    game->previous_y = 0.5;
    ENG_LOG_INFO("jeu initialisé");
}

static void game_reload(void *state, const EngServices *services)
{
    GameState *game = state;
    gm_bind_services(services);
    game->reloads++;
    ENG_LOG_INFO("nouveau code actif : état conservé (%llu pas, x = %.3f, rechargement n°%u)",
                 (unsigned long long)game->ticks, game->current_x, (unsigned)game->reloads);
}

static void game_unload(void *state)
{
    (void)state;
}

static void game_update(void *state, const EngInputState *input, double dt)
{
    GameState *game = state;
    ENG_ASSERT(dt > 0.0);

    if (eng_action_pressed(input, ENG_ACTION_PAUSE)) {
        game->paused = !game->paused;
        ENG_LOG_INFO("%s", game->paused ? "pause" : "reprise");
    }
    game->previous_x = game->current_x;
    game->previous_y = game->current_y;
    game->attack_held = eng_action_held(input, ENG_ACTION_ATTACK) ? 1u : 0u;
    if (game->paused) {
        return;
    }
    game->ticks++;

    if (eng_action_pressed(input, ENG_ACTION_ATTACK)) {
        game->attacks++;
        ENG_LOG_INFO("attaque n°%u", (unsigned)game->attacks);
    }

    double speed = MOVE_SPEED * (eng_action_held(input, ENG_ACTION_SPRINT) ? SPRINT_FACTOR : 1.0);
    double move_x = (double)input->axis[ENG_AXIS_MOVE].x;
    double move_y = (double)input->axis[ENG_AXIS_MOVE].y;
    game->current_x += move_x * speed * dt;
    game->current_y += move_y * speed * dt;

    if (eng_action_pressed(input, ENG_ACTION_DODGE)) {
        game->current_x += move_x * DODGE_DISTANCE;
        game->current_y += move_y * DODGE_DISTANCE;
    }
    game->current_x = clamp01(game->current_x);
    game->current_y = clamp01(game->current_y);
}

static void game_render(const void *state, double alpha, EngDebugSquare *out)
{
    const GameState *game = state;
    out->x = game->previous_x + (game->current_x - game->previous_x) * alpha;
    /* Dans l'état, y croît vers le HAUT (comme l'axe de déplacement) ; l'écran, vers le bas. */
    out->y = 1.0 - (game->previous_y + (game->current_y - game->previous_y) * alpha);
    out->size = SQUARE_SIZE;
    out->r = game->attack_held != 0 ? 255 : SQUARE_R;
    out->g = game->attack_held != 0 ? 255 : SQUARE_G;
    out->b = game->attack_held != 0 ? 255 : SQUARE_B;
}

static void game_shutdown(void *state)
{
    const GameState *game = state;
    ENG_LOG_INFO("jeu arrêté après %llu pas", (unsigned long long)game->ticks);
}

static const EngGameApi API = {
    .api_version = ENG_GAME_API_VERSION,
    .state_size = sizeof(GameState),
    .init = game_init,
    .reload = game_reload,
    .unload = game_unload,
    .update = game_update,
    .render = game_render,
    .shutdown = game_shutdown,
};

ENG_GAME_EXPORT const EngGameApi *eng_game_get_api(void)
{
    return &API;
}
