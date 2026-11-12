#include "engine/shader.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "engine/files.h"
#include "engine/gpu_debug.h"
#include "engine/log.h"

#define DEFAULT_SHADER_DIR "shaders"

bool eng_shader_choose_format(uint32_t sdl_formats, EngShaderFormat *out)
{
    if ((sdl_formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
        *out = (EngShaderFormat){SDL_GPU_SHADERFORMAT_SPIRV, "spv", "main"};
        return true;
    }
    if ((sdl_formats & SDL_GPU_SHADERFORMAT_DXIL) != 0) {
        *out = (EngShaderFormat){SDL_GPU_SHADERFORMAT_DXIL, "dxil", "main"};
        return true;
    }
    if ((sdl_formats & SDL_GPU_SHADERFORMAT_MSL) != 0) {
        *out = (EngShaderFormat){SDL_GPU_SHADERFORMAT_MSL, "msl", "main0"};
        return true;
    }
    return false;
}

/* Valeur entière de « "clé": N » dans le JSON ; false si la clé est absente ou si N n'est pas un
 * entier positif. */
static bool read_count(const char *json, const char *key, uint32_t *out)
{
    char pattern[64];
    if (SDL_snprintf(pattern, sizeof pattern, "\"%s\"", key) >= (int)sizeof pattern) {
        return false;
    }
    const char *at = strstr(json, pattern);
    if (at == NULL) {
        return false;
    }
    at += strlen(pattern);
    while (*at == ' ' || *at == '\t' || *at == '\n' || *at == '\r') {
        at++;
    }
    if (*at != ':') {
        return false;
    }
    at++;
    while (*at == ' ' || *at == '\t' || *at == '\n' || *at == '\r') {
        at++;
    }
    if (*at < '0' || *at > '9') {
        return false;
    }
    char *end = NULL;
    const unsigned long value = strtoul(at, &end, 10);
    if (end == at || value > 1000) { /* 1000 : très au-delà des limites des pilotes */
        return false;
    }
    *out = (uint32_t)value;
    return true;
}

bool eng_shader_parse_counts(const char *json, EngShaderCounts *counts)
{
    EngShaderCounts parsed;
    if (!read_count(json, "samplers", &parsed.samplers) ||
        !read_count(json, "storage_textures", &parsed.storage_textures) ||
        !read_count(json, "storage_buffers", &parsed.storage_buffers) ||
        !read_count(json, "uniform_buffers", &parsed.uniform_buffers)) {
        return false;
    }
    *counts = parsed;
    return true;
}

SDL_GPUShader *eng_shader_load(EngGpu *gpu, EngArena *scratch, const char *dir, const char *name,
                               EngShaderStage stage)
{
    if (dir == NULL) {
        dir = DEFAULT_SHADER_DIR;
    }
    if (gpu->device == NULL) {
        ENG_LOG_ERROR("shader %s : GPU non initialisé", name);
        return NULL;
    }
    EngShaderFormat format;
    if (!eng_shader_choose_format(SDL_GetGPUShaderFormats(gpu->device), &format)) {
        ENG_LOG_ERROR("shader %s : le périphérique n'accepte ni SPIR-V, ni DXIL, ni MSL", name);
        return NULL;
    }

    const EngArenaMark mark = eng_arena_mark(scratch);
    SDL_GPUShader *shader = NULL;
    char relative[256];
    char path[640];
    EngFileData json = {0};
    EngFileData code = {0};
    EngShaderCounts counts;

    if (SDL_snprintf(relative, sizeof relative, "%s/%s.json", dir, name) >= (int)sizeof relative ||
        !eng_path_resolve(path, sizeof path, relative)) {
        ENG_LOG_ERROR("shader %s : chemin trop long", name);
        goto done;
    }
    if (!eng_file_read_all(scratch, path, &json)) {
        ENG_LOG_ERROR("shader %s : %s introuvable (la cible « shaders » a-t-elle été compilée ?)",
                      name, path);
        goto done;
    }
    if (!eng_shader_parse_counts((const char *)json.data, &counts)) {
        ENG_LOG_ERROR("shader %s : réflexion illisible dans %s", name, path);
        goto done;
    }

    if (SDL_snprintf(relative, sizeof relative, "%s/%s.%s", dir, name, format.extension) >=
            (int)sizeof relative ||
        !eng_path_resolve(path, sizeof path, relative)) {
        ENG_LOG_ERROR("shader %s : chemin trop long", name);
        goto done;
    }
    if (!eng_file_read_all(scratch, path, &code)) {
        ENG_LOG_ERROR("shader %s : %s introuvable", name, path);
        goto done;
    }

    const SDL_GPUShaderCreateInfo info = {
        .code_size = code.size,
        .code = code.data,
        .entrypoint = format.entry_point,
        .format = (SDL_GPUShaderFormat)format.sdl_format,
        .stage =
            stage == ENG_SHADER_VERTEX ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = counts.samplers,
        .num_storage_textures = counts.storage_textures,
        .num_storage_buffers = counts.storage_buffers,
        .num_uniform_buffers = counts.uniform_buffers,
    };
    SDL_GPUShaderCreateInfo named_info = info;
    const uint32_t name_props =
        eng_gpu_debug_props("shader", SDL_PROP_GPU_SHADER_CREATE_NAME_STRING, name);
    named_info.props = name_props;
    shader = SDL_CreateGPUShader(gpu->device, &named_info);
    eng_gpu_debug_props_free(name_props);
    if (shader == NULL) {
        ENG_LOG_ERROR("shader %s : création refusée par le pilote (%s) : %s", name,
                      format.extension, SDL_GetError());
    }

done:
    eng_arena_reset_to(scratch, mark);
    return shader;
}

void eng_shader_release(EngGpu *gpu, SDL_GPUShader *shader)
{
    if (gpu->device != NULL && shader != NULL) {
        SDL_ReleaseGPUShader(gpu->device, shader);
    }
}
