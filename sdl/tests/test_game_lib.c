/* Faux jeu pour tester l'hôte de rechargement à chaud. Compilé trois fois :
 *   TEST_VARIANT 1 : « A », pas de simulation = +1
 *   TEST_VARIANT 2 : « B », pas de simulation = +100 (le « code modifié »)
 *   TEST_VARIANT 3 : « gros », comme A mais avec un état plus grand (changement de disposition)
 */
#include <string.h>

#include "bridge.h"
#include "engine/assert.h"
#include "engine/game_api.h"
#include "engine/log.h"

#if TEST_VARIANT == 2
#define STEP 100
#else
#define STEP 1
#endif

typedef struct TestState {
    long accumulated;
    int init_calls;
    int reload_calls;
    int unload_calls;
    void *engine_memory; /* bloc pris dans l'arène du moteur : doit survivre au rechargement */
#if TEST_VARIANT == 3
    char extra[64];
#endif
} TestState;

static void t_init(void *state, const EngServices *services)
{
    TestState *s = state;
    gm_bind_services(services);
    s->init_calls++;
    s->engine_memory = services->alloc(services->userdata, ENG_MEM_PERMANENT, 128, 16);
    ENG_LOG_INFO("init variante %d", TEST_VARIANT);
}

static void t_reload(void *state, const EngServices *services)
{
    TestState *s = state;
    gm_bind_services(services);
    s->reload_calls++;
}

static void t_unload(void *state)
{
    TestState *s = state;
    s->unload_calls++;
}

static void t_update(void *state, const EngInputState *input, double dt)
{
    TestState *s = state;
    (void)input;
    ENG_ASSERT(dt >= 0.0);
    s->accumulated += STEP;
}

static void t_render(const void *state, double alpha, EngDebugSquare *out)
{
    const TestState *s = state;
    out->x = (double)s->accumulated;
    out->size = alpha;
    out->r = TEST_VARIANT;
    out->g = 0;
    out->b = 0;
}

static void t_shutdown(void *state)
{
    (void)state;
}

static const EngGameApi API = {
    .api_version = ENG_GAME_API_VERSION,
    .state_size = sizeof(TestState),
    .init = t_init,
    .reload = t_reload,
    .unload = t_unload,
    .update = t_update,
    .render = t_render,
    .shutdown = t_shutdown,
};

ENG_GAME_EXPORT const EngGameApi *eng_game_get_api(void)
{
    return &API;
}
