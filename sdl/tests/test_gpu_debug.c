#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/gpu.h"
#include "engine/gpu_debug.h"
#include "engine/platform.h"
#include "engine/renderer.h"
#include "test_render_util.h"
#include "utest.h"

/* Débogage GPU (M1-19) : groupes et noms de ressources.
 *
 * SDL ne permet pas de relire ce qui part vers le pilote : les tests observent donc le module par
 * son ENREGISTREUR, qui voit chaque push, pop, repère et nom au moment où il est émis, dans
 * l'ordre. Ce que l'outil (RenderDoc) affiche à partir de là dépend du pilote : voir
 * docs/gpu-debug.md. */

#define TRACE_MAX 256U

typedef struct TraceEntry {
    EngGpuDebugEvent event;
    char kind[16];
    char name[ENG_GPU_DEBUG_NAME_MAX];
    uint32_t depth;
} TraceEntry;

typedef struct Trace {
    TraceEntry entries[TRACE_MAX];
    uint32_t count;
    uint32_t overflow;
} Trace;

static void record(void *user, EngGpuDebugEvent event, const char *kind, const char *name,
                   uint32_t depth)
{
    Trace *t = (Trace *)user;
    if (t->count >= TRACE_MAX) {
        t->overflow++;
        return;
    }
    TraceEntry *e = &t->entries[t->count++];
    e->event = event;
    e->depth = depth;
    (void)snprintf(e->kind, sizeof e->kind, "%s", kind != NULL ? kind : "");
    (void)snprintf(e->name, sizeof e->name, "%s", name);
}

static void trace_start(Trace *t)
{
    *t = (Trace){0};
    eng_gpu_debug_set_enabled(true);
    eng_gpu_debug_reset_stats();
    eng_gpu_debug_set_recorder(record, t);
}

static void trace_stop(void)
{
    eng_gpu_debug_set_recorder(NULL, NULL);
    eng_gpu_debug_set_enabled(true);
}

/* Séquence des groupes : « +Frame 1 +Scene -Scene -Frame 1 » (labels et noms ignorés). */
static void trace_groups(const Trace *t, char *out, size_t size)
{
    out[0] = '\0';
    size_t used = 0;
    for (uint32_t i = 0; i < t->count; i++) {
        const TraceEntry *e = &t->entries[i];
        if (e->event != ENG_GPU_DEBUG_PUSH && e->event != ENG_GPU_DEBUG_POP) {
            continue;
        }
        const int n = snprintf(out + used, size - used, "%s%c%s", used != 0 ? " " : "",
                               e->event == ENG_GPU_DEBUG_PUSH ? '+' : '-', e->name);
        if (n < 0 || (size_t)n >= size - used) {
            return;
        }
        used += (size_t)n;
    }
}

/* Nombre de noms de ressources `name` (de type `kind`) vus. */
static uint32_t trace_names(const Trace *t, const char *kind, const char *name)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < t->count; i++) {
        const TraceEntry *e = &t->entries[i];
        if (e->event == ENG_GPU_DEBUG_NAME && strcmp(e->kind, kind) == 0 &&
            strcmp(e->name, name) == 0) {
            n++;
        }
    }
    return n;
}

#define EXPECT_GROUPS(trace, expected)                                                             \
    do {                                                                                           \
        char groups_[1024];                                                                        \
        trace_groups(&(trace), groups_, sizeof groups_);                                           \
        EXPECT_STREQ(groups_, expected);                                                           \
    } while (0)

/* ── Module seul, avec un vrai tampon de commandes ── */

typedef struct Fixture {
    EngPlatform platform;
    EngGpu gpu;
    bool ready;
} Fixture;

static bool open_fixture(Fixture *f, bool debug)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    const EngWindowDesc window = {.title = "test gpu debug",
                                  .width = 320,
                                  .height = 200,
                                  .resizable = true,
                                  .no_software_surface = true};
    if (!eng_platform_init(&f->platform, &window)) {
        return false;
    }
    const EngGpuDesc desc = {.debug = debug, .present_mode = ENG_PRESENT_VSYNC};
    f->ready = eng_gpu_init(&f->gpu, f->platform.window, &desc);
    if (!f->ready) {
        eng_platform_shutdown(&f->platform);
    }
    return f->ready;
}

/* true : tout ce que le GPU avait créé a été rendu (eng_gpu_shutdown). */
static bool close_fixture(Fixture *f)
{
    const bool balanced = eng_gpu_shutdown(&f->gpu);
    eng_platform_shutdown(&f->platform);
    return balanced;
}

/* Tampon de commandes jetable : acquis puis soumis vide. */
static struct SDL_GPUCommandBuffer *scratch_commands(Fixture *f)
{
    return SDL_AcquireGPUCommandBuffer(f->gpu.device);
}

/* DOIT rester le premier test : il lit l'état avant que quiconque ait appelé set_enabled. */
UTEST(eng_gpu_debug, a_fresh_module_is_enabled)
{
    EXPECT_TRUE(eng_gpu_debug_enabled());
}

UTEST(eng_gpu_debug, push_and_pop_nest_and_are_counted)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    SDL_GPUCommandBuffer *c = scratch_commands(&f);
    ASSERT_TRUE(c != NULL);
    eng_gpu_debug_push(c, "A");
    eng_gpu_debug_push(c, "B");
    eng_gpu_debug_label(c, "point");
    eng_gpu_debug_pop(c);
    eng_gpu_debug_pop(c);
    EXPECT_TRUE(SDL_SubmitGPUCommandBuffer(c));
    EXPECT_GROUPS(t, "+A +B -B -A");
    EXPECT_EQ(t.entries[1].depth, 2U); /* « +B » : profondeur après l'événement */
    EXPECT_EQ(t.entries[3].depth, 1U);
    EXPECT_EQ(t.entries[4].depth, 0U);
    const EngGpuDebugStats s = eng_gpu_debug_stats();
    EXPECT_EQ(s.pushes, 2U);
    EXPECT_EQ(s.pops, 2U);
    EXPECT_EQ(s.labels, 1U);
    EXPECT_EQ(s.depth, 0U);
    EXPECT_EQ(s.max_depth, 2U);
    EXPECT_EQ(s.unbalanced_pops, 0U);
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, resetting_the_counters_keeps_the_groups_that_are_still_open)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    SDL_GPUCommandBuffer *c = scratch_commands(&f);
    ASSERT_TRUE(c != NULL);
    eng_gpu_debug_push(c, "ouvert");
    eng_gpu_debug_reset_stats();
    EXPECT_EQ(eng_gpu_debug_stats().depth, 1U);
    EXPECT_EQ(eng_gpu_debug_stats().max_depth, 1U);
    EXPECT_EQ(eng_gpu_debug_stats().pushes, 0U);
    eng_gpu_debug_pop(c); /* le groupe est toujours là : fermé normalement, pas ignoré */
    EXPECT_TRUE(SDL_SubmitGPUCommandBuffer(c));
    EXPECT_EQ(eng_gpu_debug_stats().pops, 1U);
    EXPECT_EQ(eng_gpu_debug_stats().unbalanced_pops, 0U);
    EXPECT_EQ(eng_gpu_debug_stats().depth, 0U);
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, a_pop_without_a_group_is_ignored_and_counted)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    SDL_GPUCommandBuffer *c = scratch_commands(&f);
    ASSERT_TRUE(c != NULL);
    eng_gpu_debug_pop(c); /* rien d'ouvert : le pilote ne doit PAS recevoir ce pop */
    eng_gpu_debug_pop(c);
    EXPECT_TRUE(SDL_SubmitGPUCommandBuffer(c));
    EXPECT_EQ(t.count, 0U);
    const EngGpuDebugStats s = eng_gpu_debug_stats();
    EXPECT_EQ(s.unbalanced_pops, 2U);
    EXPECT_EQ(s.pops, 0U);
    EXPECT_EQ(s.depth, 0U);
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, null_and_empty_arguments_are_harmless)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    eng_gpu_debug_push(NULL, "x"); /* pas de tampon : rien */
    eng_gpu_debug_pop(NULL);
    eng_gpu_debug_label(NULL, "x");
    SDL_GPUCommandBuffer *c = scratch_commands(&f);
    ASSERT_TRUE(c != NULL);
    eng_gpu_debug_push(c, NULL); /* nom absent : « ? » */
    eng_gpu_debug_push(c, "");
    eng_gpu_debug_pop(c);
    eng_gpu_debug_pop(c);
    EXPECT_TRUE(SDL_SubmitGPUCommandBuffer(c));
    EXPECT_GROUPS(t, "+? +? -? -?");
    eng_gpu_debug_name_texture(NULL, NULL, "x");
    eng_gpu_debug_name_buffer(f.gpu.device, NULL, "x");
    eng_gpu_debug_name_texture(f.gpu.device, NULL, NULL);
    EXPECT_EQ(eng_gpu_debug_stats().names, 0U);
    eng_gpu_debug_props_free(0U);
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, long_names_are_truncated_to_the_limit)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    SDL_GPUCommandBuffer *c = scratch_commands(&f);
    ASSERT_TRUE(c != NULL);
    char many[200];
    memset(many, 'x', sizeof many - 1U);
    many[sizeof many - 1U] = '\0';
    eng_gpu_debug_push_fmt(c, "%s", many);
    eng_gpu_debug_push_fmt(c, "Frame %llu", 123456789ULL);
    eng_gpu_debug_pop(c);
    eng_gpu_debug_pop(c);
    EXPECT_TRUE(SDL_SubmitGPUCommandBuffer(c));
    EXPECT_EQ(strlen(t.entries[0].name), (size_t)ENG_GPU_DEBUG_NAME_MAX - 1U);
    EXPECT_STREQ(t.entries[1].name, "Frame 123456789");
    EXPECT_EQ(strlen(t.entries[3].name),
              (size_t)ENG_GPU_DEBUG_NAME_MAX - 1U); /* le pop rend le nom */
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, nesting_deeper_than_the_name_stack_still_balances)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    SDL_GPUCommandBuffer *c = scratch_commands(&f);
    ASSERT_TRUE(c != NULL);
    for (int i = 0; i < 40; i++) {
        eng_gpu_debug_push(c, "deep");
    }
    for (int i = 0; i < 40; i++) {
        eng_gpu_debug_pop(c);
    }
    EXPECT_TRUE(SDL_SubmitGPUCommandBuffer(c));
    const EngGpuDebugStats s = eng_gpu_debug_stats();
    EXPECT_EQ(s.pushes, 40U);
    EXPECT_EQ(s.pops, 40U);
    EXPECT_EQ(s.max_depth, 40U);
    EXPECT_EQ(s.depth, 0U);
    /* Au-delà de 32 niveaux le nom n'est plus mémorisé : « ? » au pop, jamais un crash. */
    ASSERT_EQ(t.count, 80U);
    for (uint32_t k = 0; k < 40U; k++) { /* le pop k ferme le niveau 39 - k */
        EXPECT_STREQ(t.entries[40U + k].name, 39U - k >= 32U ? "?" : "deep");
    }
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, disabled_emits_nothing_but_open_groups_are_still_closed)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    SDL_GPUCommandBuffer *c = scratch_commands(&f);
    ASSERT_TRUE(c != NULL);
    eng_gpu_debug_push(c, "ouvert");
    eng_gpu_debug_set_enabled(false);
    EXPECT_FALSE(eng_gpu_debug_enabled());
    eng_gpu_debug_push(c, "ignoré");
    eng_gpu_debug_label(c, "ignoré");
    eng_gpu_debug_pop(c); /* ferme « ouvert » : un groupe ouvert n'est jamais laissé béant */
    EXPECT_TRUE(SDL_SubmitGPUCommandBuffer(c));
    EXPECT_GROUPS(t, "+ouvert -ouvert");
    EXPECT_EQ(eng_gpu_debug_stats().depth, 0U);
    EXPECT_EQ(eng_gpu_debug_stats().labels, 0U);
    EXPECT_EQ(
        eng_gpu_debug_props("pipeline", SDL_PROP_GPU_GRAPHICSPIPELINE_CREATE_NAME_STRING, "x"),
        0U); /* sans débogage, pas de propriétés */
    trace_stop();
    EXPECT_TRUE(eng_gpu_debug_enabled()); /* trace_stop() réactive */
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, the_props_helper_carries_the_name_for_sdl)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    const uint32_t props =
        eng_gpu_debug_props("sampler", SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, "Mon sampler");
    ASSERT_TRUE(props != 0U);
    EXPECT_STREQ(SDL_GetStringProperty((SDL_PropertiesID)props,
                                       SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, "absent"),
                 "Mon sampler");
    EXPECT_EQ(trace_names(&t, "sampler", "Mon sampler"), 1U);
    eng_gpu_debug_props_free(props);
    /* Détruites : SDL ne connaît plus cet identifiant, la lecture rend la valeur par défaut. */
    EXPECT_STREQ(SDL_GetStringProperty((SDL_PropertiesID)props,
                                       SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, "détruite"),
                 "détruite");
    EXPECT_EQ(eng_gpu_debug_props("sampler", SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, NULL), 0U);
    EXPECT_EQ(eng_gpu_debug_props("sampler", SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, ""), 0U);
    EXPECT_EQ(eng_gpu_debug_props("sampler", NULL, "x"), 0U);
    EXPECT_EQ(eng_gpu_debug_stats().names, 1U);
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

/* ── Ce que SDL reçoit vraiment ──
 *
 * L'enregistreur voit ce que le module DÉCIDE d'envoyer ; pour voir ce qui arrive à SDL, l'éditeur
 * de liens remplace les fonctions de SDL par les nôtres (`-Wl,--wrap=...`, tests/CMakeLists.txt) :
 * chaque appel est noté puis transmis à la vraie fonction. Sans ce mécanisme (macOS, MSVC), ces
 * tests ne sont pas compilés. */

#ifdef OW_GPU_DEBUG_WRAP

#define SPY_MAX 256U

typedef struct SpyCall {
    char
        op; /* '+' push, '-' pop, 'L' repère, 'T'/'B' nom de texture/buffer, 'S'/'P'/'H' création */
    void *commands;
    void *device;
    void *object;
    char text[ENG_GPU_DEBUG_NAME_MAX];
} SpyCall;

typedef struct Spy {
    SpyCall calls[SPY_MAX];
    uint32_t count;
} Spy;

static Spy *g_spy; /* NULL : rien n'est noté */

static SpyCall *spy_add(char op, const char *text)
{
    if (g_spy == NULL || g_spy->count >= SPY_MAX) {
        return NULL;
    }
    SpyCall *c = &g_spy->calls[g_spy->count++];
    *c = (SpyCall){0};
    c->op = op;
    (void)snprintf(c->text, sizeof c->text, "%s", text != NULL ? text : "");
    return c;
}

/* NOLINTBEGIN(bugprone-reserved-identifier,readability-identifier-naming) : noms imposés par
 * --wrap */
void __real_SDL_PushGPUDebugGroup(SDL_GPUCommandBuffer *commands, const char *name);
void __real_SDL_PopGPUDebugGroup(SDL_GPUCommandBuffer *commands);
void __real_SDL_InsertGPUDebugLabel(SDL_GPUCommandBuffer *commands, const char *text);
void __real_SDL_SetGPUTextureName(SDL_GPUDevice *device, SDL_GPUTexture *texture, const char *text);
void __real_SDL_SetGPUBufferName(SDL_GPUDevice *device, SDL_GPUBuffer *buffer, const char *text);
SDL_GPUSampler *__real_SDL_CreateGPUSampler(SDL_GPUDevice *device,
                                            const SDL_GPUSamplerCreateInfo *info);
SDL_GPUGraphicsPipeline *
__real_SDL_CreateGPUGraphicsPipeline(SDL_GPUDevice *device,
                                     const SDL_GPUGraphicsPipelineCreateInfo *info);
SDL_GPUShader *__real_SDL_CreateGPUShader(SDL_GPUDevice *device,
                                          const SDL_GPUShaderCreateInfo *info);

void __wrap_SDL_PushGPUDebugGroup(SDL_GPUCommandBuffer *commands, const char *name)
{
    SpyCall *c = spy_add('+', name);
    if (c != NULL) {
        c->commands = commands;
    }
    __real_SDL_PushGPUDebugGroup(commands, name);
}

void __wrap_SDL_PopGPUDebugGroup(SDL_GPUCommandBuffer *commands)
{
    SpyCall *c = spy_add('-', NULL);
    if (c != NULL) {
        c->commands = commands;
    }
    __real_SDL_PopGPUDebugGroup(commands);
}

void __wrap_SDL_InsertGPUDebugLabel(SDL_GPUCommandBuffer *commands, const char *text)
{
    SpyCall *c = spy_add('L', text);
    if (c != NULL) {
        c->commands = commands;
    }
    __real_SDL_InsertGPUDebugLabel(commands, text);
}

void __wrap_SDL_SetGPUTextureName(SDL_GPUDevice *device, SDL_GPUTexture *texture, const char *text)
{
    SpyCall *c = spy_add('T', text);
    if (c != NULL) {
        c->device = device;
        c->object = texture;
    }
    __real_SDL_SetGPUTextureName(device, texture, text);
}

void __wrap_SDL_SetGPUBufferName(SDL_GPUDevice *device, SDL_GPUBuffer *buffer, const char *text)
{
    SpyCall *c = spy_add('B', text);
    if (c != NULL) {
        c->device = device;
        c->object = buffer;
    }
    __real_SDL_SetGPUBufferName(device, buffer, text);
}

/* Les créations notent le NOM porté par `info->props` au moment où SDL le reçoit. */
SDL_GPUSampler *__wrap_SDL_CreateGPUSampler(SDL_GPUDevice *device,
                                            const SDL_GPUSamplerCreateInfo *info)
{
    (void)spy_add('S', SDL_GetStringProperty(info->props, SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING,
                                             "(sans nom)"));
    return __real_SDL_CreateGPUSampler(device, info);
}

SDL_GPUGraphicsPipeline *
__wrap_SDL_CreateGPUGraphicsPipeline(SDL_GPUDevice *device,
                                     const SDL_GPUGraphicsPipelineCreateInfo *info)
{
    (void)spy_add('P', SDL_GetStringProperty(info->props,
                                             SDL_PROP_GPU_GRAPHICSPIPELINE_CREATE_NAME_STRING,
                                             "(sans nom)"));
    return __real_SDL_CreateGPUGraphicsPipeline(device, info);
}

SDL_GPUShader *__wrap_SDL_CreateGPUShader(SDL_GPUDevice *device,
                                          const SDL_GPUShaderCreateInfo *info)
{
    (void)spy_add('H', SDL_GetStringProperty(info->props, SDL_PROP_GPU_SHADER_CREATE_NAME_STRING,
                                             "(sans nom)"));
    return __real_SDL_CreateGPUShader(device, info);
}
/* NOLINTEND(bugprone-reserved-identifier,readability-identifier-naming) */

static void spy_install(Spy *s)
{
    *s = (Spy){0};
    g_spy = s;
}

static void spy_remove(void)
{
    g_spy = NULL;
}

#ifdef OW_SHADER_DIR /* utilisée par les tests du renderer seulement */
static uint32_t spy_count(const Spy *s, char op, const char *text)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < s->count; i++) {
        if (s->calls[i].op == op && strcmp(s->calls[i].text, text) == 0) {
            n++;
        }
    }
    return n;
}
#endif

/* « +Frame 1 - ... » : groupes reçus par SDL (les pop n'ont pas de nom côté SDL). */
static void spy_groups(const Spy *s, char *out, size_t size)
{
    out[0] = '\0';
    size_t used = 0;
    for (uint32_t i = 0; i < s->count; i++) {
        const SpyCall *c = &s->calls[i];
        if (c->op != '+' && c->op != '-') {
            continue;
        }
        const int n = snprintf(out + used, size - used, "%s%c%s", used != 0 ? " " : "", c->op,
                               c->op == '+' ? c->text : "");
        if (n < 0 || (size_t)n >= size - used) {
            return;
        }
        used += (size_t)n;
    }
}

UTEST(eng_gpu_debug, sdl_receives_every_group_on_the_frame_command_buffer)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Spy spy;
    spy_install(&spy);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    SDL_GPUCommandBuffer *frame_commands = f.gpu.command_buffer;
    ASSERT_TRUE(eng_gpu_begin_render_pass(&f.gpu) != NULL);
    ASSERT_TRUE(eng_gpu_begin_overlay_pass(&f.gpu) != NULL);
    eng_gpu_end_frame(&f.gpu);
    char groups[512];
    spy_groups(&spy, groups, sizeof groups);
    EXPECT_STREQ(groups, "+Frame 1 +Scene - +Overlay - -");
    ASSERT_EQ(spy.count, 6U);
    for (uint32_t i = 0; i < spy.count; i++) {
        EXPECT_TRUE(spy.calls[i].commands == (void *)frame_commands); /* un seul tampon, le bon */
    }
    spy_remove();
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, labels_and_resource_names_reach_sdl_with_the_right_objects)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Spy spy;
    spy_install(&spy);
    eng_gpu_debug_reset_stats();
    SDL_GPUCommandBuffer *c = scratch_commands(&f);
    ASSERT_TRUE(c != NULL);
    eng_gpu_debug_label(c, "repère");
    EXPECT_TRUE(SDL_SubmitGPUCommandBuffer(c));

    SDL_GPUTextureCreateInfo ti;
    SDL_zero(ti);
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width = 4;
    ti.height = 4;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(f.gpu.device, &ti);
    const SDL_GPUBufferCreateInfo bi = {.usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = 64};
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(f.gpu.device, &bi);
    ASSERT_TRUE(texture != NULL && buffer != NULL);
    eng_gpu_debug_name_texture(f.gpu.device, texture, "ma texture");
    eng_gpu_debug_name_buffer(f.gpu.device, buffer, "mon buffer");
    /* Refusés avant SDL : nom vide, périphérique ou objet absent. */
    eng_gpu_debug_name_texture(f.gpu.device, texture, "");
    eng_gpu_debug_name_buffer(f.gpu.device, buffer, "");
    eng_gpu_debug_name_texture(NULL, texture, "x");
    eng_gpu_debug_name_buffer(NULL, buffer, "x");
    eng_gpu_debug_name_texture(f.gpu.device, NULL, "x");
    eng_gpu_debug_name_buffer(f.gpu.device, NULL, "x");
    ASSERT_EQ(spy.count, 3U);
    EXPECT_EQ(eng_gpu_debug_stats().names, 2U);
    EXPECT_EQ(spy.calls[0].op, 'L');
    EXPECT_STREQ(spy.calls[0].text, "repère");
    EXPECT_TRUE(spy.calls[0].commands == (void *)c);
    EXPECT_EQ(spy.calls[1].op, 'T');
    EXPECT_STREQ(spy.calls[1].text, "ma texture");
    EXPECT_TRUE(spy.calls[1].device == (void *)f.gpu.device);
    EXPECT_TRUE(spy.calls[1].object == (void *)texture);
    EXPECT_EQ(spy.calls[2].op, 'B');
    EXPECT_STREQ(spy.calls[2].text, "mon buffer");
    EXPECT_TRUE(spy.calls[2].device == (void *)f.gpu.device);
    EXPECT_TRUE(spy.calls[2].object == (void *)buffer);
    SDL_ReleaseGPUTexture(f.gpu.device, texture);
    SDL_ReleaseGPUBuffer(f.gpu.device, buffer);
    spy_remove();
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, the_depth_and_hdr_targets_are_the_objects_that_get_named)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Spy spy;
    spy_install(&spy);
    ASSERT_TRUE(eng_gpu_enable_depth(&f.gpu));
    const bool hdr = eng_gpu_enable_hdr(&f.gpu);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    eng_gpu_end_frame(&f.gpu);
    bool depth_ok = false;
    bool hdr_ok = false;
    for (uint32_t i = 0; i < spy.count; i++) {
        const SpyCall *c = &spy.calls[i];
        if (c->op == 'T' && strcmp(c->text, "Depth") == 0) {
            depth_ok =
                c->object == (void *)f.gpu.depth_texture && c->device == (void *)f.gpu.device;
        }
        if (c->op == 'T' && strcmp(c->text, "HDR scene color") == 0) {
            hdr_ok = c->object == (void *)f.gpu.hdr_texture && c->device == (void *)f.gpu.device;
        }
    }
    EXPECT_TRUE(depth_ok);
    EXPECT_EQ(hdr_ok, hdr);
    spy_remove();
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, a_disabled_module_sends_nothing_to_sdl)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Spy spy;
    spy_install(&spy);
    eng_gpu_debug_set_enabled(false);
    (void)eng_gpu_enable_depth(&f.gpu);
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
        eng_gpu_end_frame(&f.gpu);
    }
    SDL_GPUCommandBuffer *c = scratch_commands(&f);
    ASSERT_TRUE(c != NULL);
    eng_gpu_debug_label(c, "x");
    EXPECT_TRUE(SDL_SubmitGPUCommandBuffer(c));
    const SDL_GPUBufferCreateInfo bi = {.usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = 64};
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(f.gpu.device, &bi);
    ASSERT_TRUE(buffer != NULL);
    eng_gpu_debug_name_buffer(f.gpu.device, buffer, "x");
    SDL_ReleaseGPUBuffer(f.gpu.device, buffer);
    EXPECT_EQ(spy.count, 0U);
    spy_remove();
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

#endif /* OW_GPU_DEBUG_WRAP */

/* ── Passes de l'image (EngGpu seul) ── */

static void resolve_nothing(void *user, struct SDL_GPUCommandBuffer *commands,
                            struct SDL_GPURenderPass *pass, struct SDL_GPUTexture *hdr)
{
    (void)user;
    (void)commands;
    (void)pass;
    (void)hdr;
}

UTEST(eng_gpu_debug, an_empty_frame_is_one_frame_group_with_one_scene_pass)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    eng_gpu_end_frame(&f.gpu);
    EXPECT_GROUPS(t, "+Frame 1 +Scene -Scene -Frame 1");
    EXPECT_EQ(eng_gpu_debug_stats().depth, 0U);
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, frames_are_numbered_in_order)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
        eng_gpu_end_frame(&f.gpu);
    }
    EXPECT_GROUPS(t, "+Frame 1 +Scene -Scene -Frame 1 +Frame 2 +Scene -Scene -Frame 2 "
                     "+Frame 3 +Scene -Scene -Frame 3");
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, scene_and_overlay_are_siblings_inside_the_frame)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    ASSERT_TRUE(eng_gpu_begin_render_pass(&f.gpu) != NULL);
    ASSERT_TRUE(eng_gpu_begin_overlay_pass(&f.gpu) != NULL);
    eng_gpu_end_frame(&f.gpu);
    EXPECT_GROUPS(t, "+Frame 1 +Scene -Scene +Overlay -Overlay -Frame 1");
    EXPECT_EQ(t.entries[1].depth, 2U); /* « Scene » est DANS « Frame 1 » */
    EXPECT_EQ(t.entries[3].depth, 2U); /* « Overlay » aussi : frère de « Scene », pas enfant */
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, an_overlay_without_a_scene_still_clears_the_scene_first)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    ASSERT_TRUE(eng_gpu_begin_overlay_pass(&f.gpu) != NULL);
    eng_gpu_end_frame(&f.gpu);
    EXPECT_GROUPS(t, "+Frame 1 +Scene -Scene +Overlay -Overlay -Frame 1");
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, hdr_adds_a_tonemap_pass_between_scene_and_overlay)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    if (!eng_gpu_enable_hdr(&f.gpu)) {
        EXPECT_TRUE(close_fixture(&f));
        return;
    }
    eng_gpu_set_resolve(&f.gpu, resolve_nothing, NULL);
    Trace t;
    trace_start(&t);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    eng_gpu_end_frame(&f.gpu); /* sans overlay : la principale finit à end_frame */
    EXPECT_GROUPS(t, "+Frame 1 +Scene -Scene +Tonemap -Tonemap -Frame 1");
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    ASSERT_TRUE(eng_gpu_begin_overlay_pass(&f.gpu) != NULL);
    eng_gpu_end_frame(&f.gpu);
    char groups[1024];
    trace_groups(&t, groups, sizeof groups);
    EXPECT_TRUE(
        strstr(groups, "+Frame 2 +Scene -Scene +Tonemap -Tonemap +Overlay -Overlay -Frame 2") !=
        NULL);
    EXPECT_EQ(eng_gpu_debug_stats().depth, 0U);
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, depth_and_hdr_targets_are_named_and_named_again_after_a_resize)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    ASSERT_TRUE(eng_gpu_enable_depth(&f.gpu));
    const bool hdr = eng_gpu_enable_hdr(&f.gpu);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    eng_gpu_end_frame(&f.gpu);
    EXPECT_EQ(trace_names(&t, "texture", "Depth"), 1U);
    EXPECT_EQ(trace_names(&t, "texture", "HDR scene color"), hdr ? 1U : 0U);
    EXPECT_TRUE(eng_platform_set_window_size(&f.platform, 200, 120));
    (void)eng_platform_pump(&f.platform);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    eng_gpu_end_frame(&f.gpu);
    EXPECT_EQ(trace_names(&t, "texture", "Depth"), 2U); /* recréée à la nouvelle taille */
    EXPECT_EQ(trace_names(&t, "texture", "HDR scene color"), hdr ? 2U : 0U);
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, groups_stay_balanced_across_resizes_overlays_and_hdr)
{
    Fixture f = {0};
    if (!open_fixture(&f, true)) { /* validation SDL active */
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    (void)eng_gpu_enable_depth(&f.gpu);
    if (eng_gpu_enable_hdr(&f.gpu)) {
        eng_gpu_set_resolve(&f.gpu, resolve_nothing, NULL);
    }
    Trace t;
    trace_start(&t);
    for (int i = 0; i < 90; i++) {
        (void)eng_platform_set_window_size(&f.platform, 160 + (i * 37) % 300, 120 + (i * 53) % 200);
        (void)eng_platform_pump(&f.platform);
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
        if (i % 3 != 0) {
            (void)eng_gpu_begin_render_pass(&f.gpu);
        }
        if (i % 2 == 0) {
            (void)eng_gpu_begin_overlay_pass(&f.gpu);
        }
        eng_gpu_end_frame(&f.gpu);
        ASSERT_EQ(eng_gpu_debug_stats().depth, 0U);
    }
    const EngGpuDebugStats s = eng_gpu_debug_stats();
    EXPECT_EQ(s.pushes, s.pops);
    EXPECT_EQ(s.unbalanced_pops, 0U);
    EXPECT_EQ(s.max_depth, 2U);
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

UTEST(eng_gpu_debug, disabling_between_frames_removes_the_groups_and_frames_still_run)
{
    Fixture f = {0};
    if (!open_fixture(&f, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace t;
    trace_start(&t);
    eng_gpu_debug_set_enabled(false);
    for (int i = 0; i < 5; i++) {
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
        eng_gpu_end_frame(&f.gpu);
    }
    EXPECT_EQ(t.count, 0U);
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)5);
    EXPECT_EQ(eng_gpu_debug_stats().unbalanced_pops, 0U);
    eng_gpu_debug_set_enabled(true);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    eng_gpu_end_frame(&f.gpu);
    EXPECT_GROUPS(t, "+Frame 6 +Scene -Scene -Frame 6");
    trace_stop();
    EXPECT_TRUE(close_fixture(&f));
}

/* ── Renderer ── */

#ifdef OW_SHADER_DIR

typedef struct TestRenderer {
    RenderFixture f;
    EngArena permanent;
    EngRenderer r;
} TestRenderer;

static bool open_renderer(TestRenderer *t, const EngRendererDesc *base)
{
    if (!open_render_fixture_with(&t->f, false)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "debug-test", (size_t)4 << 20)) {
        close_render_fixture(&t->f);
        return false;
    }
    EngRendererDesc desc = *base;
    desc.offline_dir = OW_SHADER_DIR;
    if (desc.max_commands == 0U) {
        desc.max_commands = 16;
    }
    if (!eng_renderer_init(&t->r, &t->f.gpu, &t->permanent, &t->f.scratch, &desc)) {
        eng_arena_shutdown(&t->permanent);
        close_render_fixture(&t->f);
        return false;
    }
    return true;
}

static void close_renderer(TestRenderer *t)
{
    (void)eng_renderer_shutdown(&t->r);
    eng_arena_shutdown(&t->permanent);
    close_render_fixture(&t->f);
}

static const float TRI[3 * 6] = {0.0F, 0.5F, 0.0F, 1.0F, 0.0F,  0.0F, -0.5F, -0.5F, 0.0F,
                                 0.0F, 1.0F, 0.0F, 0.5F, -0.5F, 0.0F, 0.0F,  0.0F,  1.0F};

UTEST(eng_gpu_debug_renderer, buffers_textures_and_samplers_carry_their_name)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, &(EngRendererDesc){0})) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace tr;
    trace_start(&tr);
    const EngBufferDesc named = {
        .name = "Mon buffer", .usage = ENG_BUFFER_VERTEX, .size = sizeof TRI, .data = TRI};
    const EngBufferDesc anonymous = {.usage = ENG_BUFFER_INDEX, .size = sizeof TRI, .data = TRI};
    const EngBufferDesc anonymous_vertex = {
        .usage = ENG_BUFFER_VERTEX, .size = sizeof TRI, .data = TRI};
    EXPECT_TRUE(eng_renderer_create_buffer(&t.r, &named).handle.generation != 0U);
    EXPECT_TRUE(eng_renderer_create_buffer(&t.r, &anonymous).handle.generation != 0U);
    EXPECT_TRUE(eng_renderer_create_buffer(&t.r, &anonymous_vertex).handle.generation != 0U);
    EXPECT_TRUE(eng_renderer_create_stream_buffer(&t.r, ENG_BUFFER_VERTEX, 256).handle.generation !=
                0U);
    EXPECT_TRUE(eng_renderer_create_stream_buffer(&t.r, ENG_BUFFER_INDEX, 256).handle.generation !=
                0U);
    EXPECT_EQ(trace_names(&tr, "buffer", "Mon buffer"), 1U);
    EXPECT_EQ(trace_names(&tr, "buffer", "Index buffer"), 1U); /* nom par défaut selon l'usage */
    EXPECT_EQ(trace_names(&tr, "buffer", "Vertex buffer"), 1U);
    EXPECT_EQ(trace_names(&tr, "buffer", "Stream vertex buffer"), 1U);
    EXPECT_EQ(trace_names(&tr, "buffer", "Stream index buffer"), 1U);

    const uint8_t pixels[4 * 4 * 4] = {0};
    const EngTextureDesc texture = {.name = "Ma texture",
                                    .width = 4,
                                    .height = 4,
                                    .format = ENG_TEXTURE_RGBA8,
                                    .pixels = pixels,
                                    .mip_levels = 1};
    EXPECT_TRUE(eng_renderer_create_texture(&t.r, &texture).handle.generation != 0U);
    const uint16_t half[4 * 4 * 6 * 4] = {0};
    const EngTextureDataDesc cube = {.name = "Mon cube",
                                     .width = 4,
                                     .height = 4,
                                     .layers = 6,
                                     .mip_levels = 1,
                                     .format = ENG_TEXTURE_RGBA16F,
                                     .data = half};
    EXPECT_TRUE(eng_renderer_create_texture_data(&t.r, &cube).handle.generation != 0U);
    EXPECT_EQ(trace_names(&tr, "texture", "Ma texture"), 1U);
    EXPECT_EQ(trace_names(&tr, "texture", "Mon cube"), 1U);

    const EngSamplerDesc sampler = {.name = "Mon sampler"};
    EXPECT_TRUE(eng_renderer_create_sampler(&t.r, &sampler).handle.generation != 0U);
    EXPECT_EQ(trace_names(&tr, "sampler", "Mon sampler"), 1U);
    const EngSamplerDesc no_name = {0};
    EXPECT_TRUE(eng_renderer_create_sampler(&t.r, &no_name).handle.generation != 0U);
    EXPECT_EQ(eng_gpu_debug_stats().names, 8U); /* le sampler sans nom n'en ajoute pas */
    EXPECT_EQ(eng_gpu_debug_stats().depth, 0U);
    trace_stop();
    close_renderer(&t);
}

UTEST(eng_gpu_debug_renderer, uploads_are_groups_named_after_their_resource)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, &(EngRendererDesc){0})) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace tr;
    trace_start(&tr);
    const EngBufferDesc named = {
        .name = "Mon buffer", .usage = ENG_BUFFER_VERTEX, .size = sizeof TRI, .data = TRI};
    const EngBufferDesc anonymous = {.usage = ENG_BUFFER_VERTEX, .size = sizeof TRI, .data = TRI};
    (void)eng_renderer_create_buffer(&t.r, &named);
    (void)eng_renderer_create_buffer(&t.r, &anonymous);
    const uint8_t pixels[2 * 2 * 4] = {0};
    const EngTextureDesc texture = {.name = "Ma texture",
                                    .width = 2,
                                    .height = 2,
                                    .format = ENG_TEXTURE_RGBA8,
                                    .pixels = pixels,
                                    .mip_levels = 1};
    (void)eng_renderer_create_texture(&t.r, &texture);
    const uint16_t half[2 * 2 * 4] = {0};
    const EngTextureDataDesc data = {.name = "Ma LUT",
                                     .width = 2,
                                     .height = 2,
                                     .layers = 1,
                                     .mip_levels = 1,
                                     .format = ENG_TEXTURE_RGBA16F,
                                     .data = half};
    (void)eng_renderer_create_texture_data(&t.r, &data);
    EXPECT_GROUPS(tr, "+Upload buffer Mon buffer -Upload buffer Mon buffer "
                      "+Upload buffer (unnamed) -Upload buffer (unnamed) "
                      "+Upload texture Ma texture -Upload texture Ma texture "
                      "+Upload texture Ma LUT -Upload texture Ma LUT");
    trace_stop();
    close_renderer(&t);
}

UTEST(eng_gpu_debug_renderer, pipelines_and_their_shaders_are_named)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, &(EngRendererDesc){0})) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Trace tr;
    trace_start(&tr);
    EngPipelineDesc d = {.name = "triangle",
                         .vertex_shader = "triangle.vert",
                         .fragment_shader = "triangle.frag",
                         .vertex_stride = 6 * sizeof(float),
                         .attribute_count = 2,
                         .color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM};
    d.attributes[0] = (EngVertexAttribute){.location = 0, .format = ENG_VERTEX_FLOAT3, .offset = 0};
    d.attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT3, .offset = 3 * sizeof(float)};
    EXPECT_TRUE(eng_renderer_create_pipeline(&t.r, &d).handle.generation != 0U);
    EXPECT_EQ(trace_names(&tr, "pipeline", "triangle"), 1U);
    EXPECT_TRUE(trace_names(&tr, "shader", "triangle.vert") >= 1U);
    EXPECT_TRUE(trace_names(&tr, "shader", "triangle.frag") >= 1U);
    trace_stop();
    close_renderer(&t);
}

#ifdef OW_GPU_DEBUG_WRAP
UTEST(eng_gpu_debug_renderer, sdl_receives_the_names_of_samplers_pipelines_and_shaders)
{
    TestRenderer t = {0};
    Spy spy;
    spy_install(&spy);
    const EngRendererDesc desc = {.hdr = true};
    if (!open_renderer(&t, &desc)) {
        spy_remove();
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngSamplerDesc sampler = {.name = "Mon sampler"};
    const EngSamplerDesc anonymous = {0};
    EXPECT_TRUE(eng_renderer_create_sampler(&t.r, &sampler).handle.generation != 0U);
    EXPECT_TRUE(eng_renderer_create_sampler(&t.r, &anonymous).handle.generation != 0U);
    EngPipelineDesc d = {.name = "triangle",
                         .vertex_shader = "triangle.vert",
                         .fragment_shader = "triangle.frag",
                         .vertex_stride = 6 * sizeof(float),
                         .attribute_count = 2,
                         .color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM};
    d.attributes[0] = (EngVertexAttribute){.location = 0, .format = ENG_VERTEX_FLOAT3, .offset = 0};
    d.attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT3, .offset = 3 * sizeof(float)};
    EXPECT_TRUE(eng_renderer_create_pipeline(&t.r, &d).handle.generation != 0U);
    EXPECT_EQ(spy_count(&spy, 'S', "Mon sampler"), 1U);
    EXPECT_EQ(spy_count(&spy, 'S', "(sans nom)"), 1U); /* le sampler sans nom reste sans nom */
    EXPECT_EQ(spy_count(&spy, 'P', "triangle"), 1U);
    EXPECT_EQ(spy_count(&spy, 'H', "triangle.vert"), 1U);
    EXPECT_EQ(spy_count(&spy, 'H', "triangle.frag"), 1U);
    if (eng_renderer_hdr_enabled(&t.r)) { /* créés à l'initialisation : l'espion était déjà là */
        EXPECT_EQ(spy_count(&spy, 'S', "HDR scene sampler"), 1U);
        EXPECT_EQ(spy_count(&spy, 'P', "tonemap"), 1U);
    }
    spy_remove();
    close_renderer(&t);
}

UTEST(eng_gpu_debug_renderer, sdl_receives_buffer_and_texture_names_and_upload_groups)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, &(EngRendererDesc){0})) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Spy spy;
    spy_install(&spy);
    const EngBufferDesc named = {
        .name = "Mon buffer", .usage = ENG_BUFFER_VERTEX, .size = sizeof TRI, .data = TRI};
    (void)eng_renderer_create_buffer(&t.r, &named);
    const uint8_t pixels[2 * 2 * 4] = {0};
    const EngTextureDesc texture = {.name = "Ma texture",
                                    .width = 2,
                                    .height = 2,
                                    .format = ENG_TEXTURE_RGBA8,
                                    .pixels = pixels,
                                    .mip_levels = 1};
    (void)eng_renderer_create_texture(&t.r, &texture);
    EXPECT_EQ(spy_count(&spy, 'B', "Mon buffer"), 1U);
    EXPECT_EQ(spy_count(&spy, 'T', "Ma texture"), 1U);
    EXPECT_EQ(spy_count(&spy, '+', "Upload buffer Mon buffer"), 1U);
    EXPECT_EQ(spy_count(&spy, '+', "Upload texture Ma texture"), 1U);
    char groups[512];
    spy_groups(&spy, groups, sizeof groups);
    EXPECT_STREQ(groups, "+Upload buffer Mon buffer - +Upload texture Ma texture -");
    spy_remove();
    close_renderer(&t);
}
#endif /* OW_GPU_DEBUG_WRAP */

typedef struct OverlayProbe {
    uint32_t prepared;
    uint32_t drawn;
} OverlayProbe;

static bool overlay_prepare(void *user, struct SDL_GPUCommandBuffer *commands)
{
    (void)commands;
    ((OverlayProbe *)user)->prepared++;
    return true;
}

static void overlay_draw(void *user, struct SDL_GPUCommandBuffer *commands,
                         struct SDL_GPURenderPass *pass)
{
    (void)commands;
    (void)pass;
    ((OverlayProbe *)user)->drawn++;
}

UTEST(eng_gpu_debug_renderer, a_frame_with_streams_shadow_and_overlay_lists_every_pass_in_order)
{
    TestRenderer t = {0};
    const EngRendererDesc desc = {.max_shadow_commands = 8, .depth_buffer = true};
    if (!open_renderer(&t, &desc)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferHandle stream = eng_renderer_create_stream_buffer(&t.r, ENG_BUFFER_VERTEX, 256);
    const EngTextureHandle map = eng_renderer_create_shadow_map(&t.r, 64);
    if (map.handle.generation == 0U) { /* GPU sans D32F lisible : rien à vérifier ici */
        close_renderer(&t);
        return;
    }
    eng_renderer_set_shadow_map(&t.r, map);
    OverlayProbe probe = {0};
    eng_renderer_set_overlay(&t.r, overlay_prepare, overlay_draw, &probe);

    Trace tr;
    trace_start(&tr);
    (void)eng_renderer_begin_frame(&t.r);
    (void)eng_renderer_begin_shadow_queue(&t.r);
    EXPECT_TRUE(eng_renderer_stream_write(&t.r, stream, 0, TRI, sizeof TRI));
    eng_renderer_end_frame(&t.r);
    EXPECT_EQ(probe.prepared, 1U);
    EXPECT_EQ(probe.drawn, 1U);
    EXPECT_GROUPS(tr, "+Frame 1 +Upload streams -Upload streams +Shadow map -Shadow map "
                      "+Overlay upload -Overlay upload +Scene -Scene +Overlay -Overlay -Frame 1");
    EXPECT_EQ(eng_gpu_debug_stats().depth, 0U);

    /* Seconde image sans écriture : plus d'envoi de flux. */
    tr.count = 0;
    (void)eng_renderer_begin_frame(&t.r);
    (void)eng_renderer_begin_shadow_queue(&t.r);
    eng_renderer_end_frame(&t.r);
    EXPECT_GROUPS(tr, "+Frame 2 +Shadow map -Shadow map +Overlay upload -Overlay upload +Scene "
                      "-Scene +Overlay -Overlay -Frame 2");
    trace_stop();
    close_renderer(&t);
}

UTEST(eng_gpu_debug_renderer, the_shadow_map_and_the_hdr_resources_are_named)
{
    TestRenderer t = {0};
    const EngRendererDesc desc = {.max_shadow_commands = 8, .hdr = true};
    Trace tr;
    trace_start(&tr); /* avant l'ouverture : le sampler HDR est créé à l'init */
    if (!open_renderer(&t, &desc)) {
        trace_stop();
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngTextureHandle map = eng_renderer_create_shadow_map(&t.r, 64);
    if (eng_renderer_hdr_enabled(&t.r)) {
        EXPECT_EQ(trace_names(&tr, "sampler", "HDR scene sampler"), 1U);
        EXPECT_EQ(trace_names(&tr, "pipeline", "tonemap"), 1U);
    }
    if (map.handle.generation != 0U) {
        EXPECT_EQ(trace_names(&tr, "texture", "Shadow map depth"), 1U);
    }
    trace_stop();
    close_renderer(&t);
}

UTEST(eng_gpu_debug_renderer, a_hdr_frame_ends_with_the_tonemap_pass)
{
    TestRenderer t = {0};
    const EngRendererDesc desc = {.hdr = true};
    if (!open_renderer(&t, &desc)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    if (!eng_renderer_hdr_enabled(&t.r)) {
        close_renderer(&t);
        return;
    }
    Trace tr;
    trace_start(&tr);
    (void)eng_renderer_begin_frame(&t.r);
    eng_renderer_end_frame(&t.r);
    EXPECT_GROUPS(tr, "+Frame 1 +Scene -Scene +Tonemap -Tonemap -Frame 1");
    trace_stop();
    close_renderer(&t);
}

#endif /* OW_SHADER_DIR */
