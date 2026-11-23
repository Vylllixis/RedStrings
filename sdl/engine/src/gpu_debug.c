#include "engine/gpu_debug.h"

#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define NAME_STACK 32U /* noms mémorisés pour l'enregistreur ; au-delà, « ? » */

static struct {
    bool enabled;
    bool initialized;
    EngGpuDebugRecorder recorder;
    void *recorder_user;
    EngGpuDebugStats stats;
    char names[NAME_STACK][ENG_GPU_DEBUG_NAME_MAX];
} g_debug;

/* Le zéro de l'initialisation statique voudrait dire « désactivé » : on part de « actif ». */
static void ensure_init(void)
{
    if (!g_debug.initialized) {
        g_debug.initialized = true;
        g_debug.enabled = true;
    }
}

static void emit(EngGpuDebugEvent event, const char *kind, const char *name)
{
    if (g_debug.recorder != NULL) {
        g_debug.recorder(g_debug.recorder_user, event, kind, name, g_debug.stats.depth);
    }
}

static const char *safe_name(const char *name)
{
    return name != NULL && name[0] != '\0' ? name : "?";
}

void eng_gpu_debug_set_enabled(bool enabled)
{
    ensure_init();
    g_debug.enabled = enabled;
}

bool eng_gpu_debug_enabled(void)
{
    ensure_init();
    return g_debug.enabled;
}

void eng_gpu_debug_set_recorder(EngGpuDebugRecorder fn, void *user)
{
    g_debug.recorder = fn;
    g_debug.recorder_user = user;
}

EngGpuDebugStats eng_gpu_debug_stats(void)
{
    return g_debug.stats;
}

void eng_gpu_debug_reset_stats(void)
{
    const uint32_t depth = g_debug.stats.depth;
    g_debug.stats = (EngGpuDebugStats){0};
    g_debug.stats.depth = depth;
    g_debug.stats.max_depth = depth;
}

void eng_gpu_debug_push(struct SDL_GPUCommandBuffer *commands, const char *name)
{
    ensure_init();
    if (commands == NULL || !g_debug.enabled) {
        return;
    }
    name = safe_name(name);
    if (g_debug.stats.depth < NAME_STACK) {
        (void)snprintf(g_debug.names[g_debug.stats.depth], ENG_GPU_DEBUG_NAME_MAX, "%s", name);
    }
    SDL_PushGPUDebugGroup(commands, name);
    g_debug.stats.pushes++;
    g_debug.stats.depth++;
    if (g_debug.stats.depth > g_debug.stats.max_depth) {
        g_debug.stats.max_depth = g_debug.stats.depth;
    }
    emit(ENG_GPU_DEBUG_PUSH, NULL, name);
}

void eng_gpu_debug_push_fmt(struct SDL_GPUCommandBuffer *commands, const char *format, ...)
{
    char name[ENG_GPU_DEBUG_NAME_MAX];
    va_list args;
    va_start(args, format);
    /* Même faux positif de clang-tidy 18 que dans log.c : le va_list est initialisé juste
     * au-dessus. */
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    (void)vsnprintf(name, sizeof name, format, args);
    va_end(args);
    eng_gpu_debug_push(commands, name);
}

void eng_gpu_debug_pop(struct SDL_GPUCommandBuffer *commands)
{
    ensure_init();
    if (commands == NULL) {
        return;
    }
    if (g_debug.stats.depth == 0) {
        /* Un pop de trop planterait la couche de validation : jamais transmis. Désactivé, les pop
         * qui répondent à des push ignorés sont normaux : on ne les compte pas. */
        if (g_debug.enabled) {
            g_debug.stats.unbalanced_pops++;
        }
        return;
    }
    const uint32_t index = g_debug.stats.depth - 1U;
    const char *name = index < NAME_STACK ? g_debug.names[index] : "?";
    SDL_PopGPUDebugGroup(commands);
    g_debug.stats.pops++;
    g_debug.stats.depth--;
    emit(ENG_GPU_DEBUG_POP, NULL, name);
}

void eng_gpu_debug_label(struct SDL_GPUCommandBuffer *commands, const char *text)
{
    ensure_init();
    if (commands == NULL || !g_debug.enabled) {
        return;
    }
    text = safe_name(text);
    SDL_InsertGPUDebugLabel(commands, text);
    g_debug.stats.labels++;
    emit(ENG_GPU_DEBUG_LABEL, NULL, text);
}

void eng_gpu_debug_name_texture(struct SDL_GPUDevice *device, struct SDL_GPUTexture *texture,
                                const char *name)
{
    ensure_init();
    if (device == NULL || texture == NULL || name == NULL || name[0] == '\0' || !g_debug.enabled) {
        return;
    }
    SDL_SetGPUTextureName(device, texture, name);
    g_debug.stats.names++;
    emit(ENG_GPU_DEBUG_NAME, "texture", name);
}

void eng_gpu_debug_name_buffer(struct SDL_GPUDevice *device, struct SDL_GPUBuffer *buffer,
                               const char *name)
{
    ensure_init();
    if (device == NULL || buffer == NULL || name == NULL || name[0] == '\0' || !g_debug.enabled) {
        return;
    }
    SDL_SetGPUBufferName(device, buffer, name);
    g_debug.stats.names++;
    emit(ENG_GPU_DEBUG_NAME, "buffer", name);
}

uint32_t eng_gpu_debug_props(const char *kind, const char *key, const char *name)
{
    ensure_init();
    if (key == NULL || name == NULL || name[0] == '\0' || !g_debug.enabled) {
        return 0;
    }
    const SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0) {
        return 0;
    }
    if (!SDL_SetStringProperty(props, key, name)) {
        SDL_DestroyProperties(props);
        return 0;
    }
    g_debug.stats.names++;
    emit(ENG_GPU_DEBUG_NAME, kind != NULL ? kind : "?", name);
    return (uint32_t)props;
}

void eng_gpu_debug_props_free(uint32_t props)
{
    if (props != 0) {
        SDL_DestroyProperties((SDL_PropertiesID)props);
    }
}
