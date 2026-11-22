#include "engine/shader_hot.h"

#include <SDL3/SDL.h>
#include <string.h>

#ifdef ENG_SHADER_HOT_RELOAD
#include <SDL3_shadercross/SDL_shadercross.h>
#endif

#include "engine/files.h"
#include "engine/log.h"

bool eng_shader_hot_available(void)
{
#ifdef ENG_SHADER_HOT_RELOAD
    return true;
#else
    return false;
#endif
}

#ifdef ENG_SHADER_HOT_RELOAD

/* SDL_ShaderCross_Init ne doit être appelé qu'une fois : on compte les utilisateurs. */
static int g_cross_users = 0;

static bool cross_acquire(void)
{
    if (g_cross_users == 0 && !SDL_ShaderCross_Init()) {
        ENG_LOG_ERROR("shaders : SDL_ShaderCross_Init a échoué : %s", SDL_GetError());
        return false;
    }
    g_cross_users++;
    return true;
}

static void cross_release(void)
{
    if (g_cross_users == 0) {
        return;
    }
    g_cross_users--;
    if (g_cross_users == 0) {
        SDL_ShaderCross_Quit();
    }
}

struct SDL_GPUShader *eng_shader_compile_hlsl(EngGpu *gpu, const char *hlsl,
                                              const char *include_dir, EngShaderStage stage,
                                              const char *debug_name)
{
    if (gpu == NULL || gpu->device == NULL || hlsl == NULL) {
        return NULL;
    }
    if (!cross_acquire()) {
        return NULL;
    }

    SDL_GPUShader *shader = NULL;
    void *spirv = NULL;
    SDL_ShaderCross_GraphicsShaderMetadata *metadata = NULL;
    const SDL_ShaderCross_ShaderStage cross_stage = stage == ENG_SHADER_VERTEX
                                                        ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX
                                                        : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;

    const SDL_ShaderCross_HLSL_Info info = {
        .source = hlsl,
        .entrypoint = "main",
        .include_dir = include_dir,
        .defines = NULL,
        .shader_stage = cross_stage,
        .props = 0,
    };
    size_t spirv_size = 0;
    spirv = SDL_ShaderCross_CompileSPIRVFromHLSL(&info, &spirv_size);
    if (spirv == NULL) {
        ENG_LOG_ERROR("shader « %s » : compilation HLSL échouée : %s", debug_name, SDL_GetError());
        goto done;
    }
    metadata = SDL_ShaderCross_ReflectGraphicsSPIRV((const Uint8 *)spirv, spirv_size, 0);
    if (metadata == NULL) {
        ENG_LOG_ERROR("shader « %s » : réflexion impossible : %s", debug_name, SDL_GetError());
        goto done;
    }
    const SDL_ShaderCross_SPIRV_Info spirv_info = {
        .bytecode = (const Uint8 *)spirv,
        .bytecode_size = spirv_size,
        .entrypoint = "main",
        .shader_stage = cross_stage,
        .props = 0,
    };
    /* Même chemin pour tous les pilotes : SPIR-V -> format du périphérique (SPIR-V, DXIL, MSL). */
    shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(gpu->device, &spirv_info,
                                                            &metadata->resource_info, 0);
    if (shader == NULL) {
        ENG_LOG_ERROR("shader « %s » : le pilote refuse le shader : %s", debug_name,
                      SDL_GetError());
    }

done:
    SDL_free(metadata);
    SDL_free(spirv);
    cross_release();
    return shader;
}

#else /* build sans rechargement */

struct SDL_GPUShader *eng_shader_compile_hlsl(EngGpu *gpu, const char *hlsl,
                                              const char *include_dir, EngShaderStage stage,
                                              const char *debug_name)
{
    (void)gpu;
    (void)hlsl;
    (void)include_dir;
    (void)stage;
    ENG_LOG_ERROR("shader « %s » : cette build ne compile pas les shaders à l'exécution "
                  "(OW_SHADER_HOT_RELOAD)",
                  debug_name);
    return NULL;
}

#endif

/* Dossier d'un chemin, sans séparateur final ; « . » s'il n'y en a pas. */
static void directory_of(const char *path, char *out, size_t out_size)
{
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    if (backslash != NULL && (slash == NULL || backslash > slash)) {
        slash = backslash;
    }
    if (slash == NULL) {
        SDL_strlcpy(out, ".", out_size);
        return;
    }
    const size_t length = (size_t)(slash - path);
    const size_t copy = length < out_size - 1 ? length : out_size - 1;
    memcpy(out, path, copy);
    out[copy] = '\0';
}

#ifdef ENG_SHADER_HOT_RELOAD
/* Compile le fichier source ; NULL (journalisé) si illisible ou s'il ne compile pas. */
static SDL_GPUShader *compile_file(EngShaderHot *hot, EngGpu *gpu, EngArena *scratch)
{
    const EngArenaMark mark = eng_arena_mark(scratch);
    EngFileData file;
    SDL_GPUShader *shader = NULL;
    if (!eng_file_read_all(scratch, hot->source, &file)) {
        ENG_LOG_ERROR("shader « %s » : lecture de %s impossible", hot->name, hot->source);
    } else {
        shader = eng_shader_compile_hlsl(gpu, (const char *)file.data, hot->include_dir, hot->stage,
                                         hot->name);
    }
    eng_arena_reset_to(scratch, mark);
    return shader;
}
#endif

bool eng_shader_hot_init(EngShaderHot *hot, EngGpu *gpu, EngArena *scratch, const char *source_path,
                         const char *offline_dir, const char *name, EngShaderStage stage)
{
    *hot = (EngShaderHot){0};
    hot->stage = stage;
    if ((source_path != NULL &&
         SDL_strlcpy(hot->source, source_path, sizeof hot->source) >= sizeof hot->source) ||
        SDL_strlcpy(hot->name, name, sizeof hot->name) >= sizeof hot->name ||
        SDL_strlcpy(hot->offline_dir, offline_dir != NULL ? offline_dir : "shaders",
                    sizeof hot->offline_dir) >= sizeof hot->offline_dir) {
        ENG_LOG_ERROR("shader « %s » : chemin trop long", name);
        return false;
    }
    directory_of(hot->source, hot->include_dir, sizeof hot->include_dir);

#ifdef ENG_SHADER_HOT_RELOAD
    if (source_path != NULL) {
        hot->hot = true;
    }
    if (hot->hot && !eng_file_watch_init(&hot->watch, hot->source, 0.0)) {
        return false;
    }
    if (hot->hot) {
        hot->shader = compile_file(hot, gpu, scratch);
        if (hot->shader == NULL) {
            ENG_LOG_WARN("shader « %s » : repli sur la version compilée hors ligne", hot->name);
        }
    }
#else
    (void)gpu;
#endif
    if (hot->shader == NULL) {
        hot->shader = eng_shader_load(gpu, scratch, hot->offline_dir, hot->name, stage);
    }
    if (hot->shader == NULL) {
        return false;
    }
    hot->generation = 1;
    return true;
}

bool eng_shader_hot_poll(EngShaderHot *hot, EngGpu *gpu, EngArena *scratch, double now_seconds)
{
#ifdef ENG_SHADER_HOT_RELOAD
    if (!hot->hot || !eng_file_watch_poll(&hot->watch, now_seconds)) {
        return false;
    }
    ENG_LOG_INFO("shader « %s » : modification détectée, recompilation", hot->name);
    SDL_GPUShader *fresh = compile_file(hot, gpu, scratch);
    if (fresh == NULL) {
        hot->failed_reloads++;
        ENG_LOG_WARN("shader « %s » : l'ancienne version reste en service", hot->name);
        return false;
    }
    eng_shader_release(gpu, hot->shader);
    hot->shader = fresh;
    hot->generation++;
    ENG_LOG_INFO("shader « %s » : rechargé (version %u)", hot->name, (unsigned)hot->generation);
    return true;
#else
    (void)hot;
    (void)gpu;
    (void)scratch;
    (void)now_seconds;
    return false;
#endif
}

void eng_shader_hot_release(EngShaderHot *hot, EngGpu *gpu)
{
    if (hot->shader != NULL) {
        eng_shader_release(gpu, hot->shader);
        hot->shader = NULL;
    }
}
