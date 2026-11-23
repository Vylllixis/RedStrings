#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/gpu.h"
#include "engine/platform.h"

/* Outils communs aux tests de rendu : un contexte GPU hors écran et la lecture des pixels d'une
 * image dessinée dans une texture. Sans GPU, open_render_fixture retourne false et le test s'arrête
 * là, sauf si OW_REQUIRE_GPU=1 (alors c'est un échec : `OW_REQUIRE_GPU=1 ctest -R triangle`). */

typedef struct RenderFixture {
    EngPlatform platform;
    EngGpu gpu;
    EngArena scratch;
} RenderFixture;

static inline bool render_gpu_required(void)
{
    const char *value = getenv("OW_REQUIRE_GPU");
    return value != NULL && value[0] == '1';
}

/* `debug` : validation de SDL_GPU (SDL refuse alors les paramètres invalides au lieu de les
 * transmettre au pilote). */
static inline bool open_render_fixture_with(RenderFixture *f, bool debug)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    const EngWindowDesc window = {
        .title = "test rendu", .width = 64, .height = 64, .no_software_surface = true};
    if (!eng_platform_init(&f->platform, &window)) {
        return false;
    }
    const EngGpuDesc desc = {.debug = debug, .present_mode = ENG_PRESENT_VSYNC};
    if (!eng_gpu_init(&f->gpu, f->platform.window, &desc)) {
        eng_platform_shutdown(&f->platform);
        return false;
    }
    if (!eng_arena_init(&f->scratch, "render-test", (size_t)1 << 20)) {
        eng_gpu_shutdown(&f->gpu);
        eng_platform_shutdown(&f->platform);
        return false;
    }
    return true;
}

static inline bool open_render_fixture(RenderFixture *f)
{
    return open_render_fixture_with(f, false);
}

static inline void close_render_fixture(RenderFixture *f)
{
    eng_arena_shutdown(&f->scratch);
    eng_gpu_shutdown(&f->gpu);
    eng_platform_shutdown(&f->platform);
}

/* Dessine UNE image dans une texture RGBA8 de `size` x `size` pixels, effacée en noir, grâce à
 * `draw` (appelée dans la passe de rendu, avec le tampon de commandes pour les uniforms), puis
 * copie les pixels dans `rgba` (size * size * 4 octets, ligne du haut en premier). Si
 * `depth_format` n'est pas 0, la passe a une texture de profondeur de ce format, effacée à 1,0.
 * false si une étape échoue. */
typedef void (*RenderDrawFn)(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass,
                             void *context);

/* `prepare` (peut être NULL) est appelée avec le tampon de commandes AVANT l'ouverture de la passe
 * de rendu : c'est là qu'on envoie les buffers de streaming (eng_renderer_flush_streams). */
typedef void (*RenderPrepareFn)(SDL_GPUCommandBuffer *commands, void *context);

static inline bool render_to_pixels_prepared(RenderFixture *f, uint32_t depth_format,
                                             RenderPrepareFn prepare, RenderDrawFn draw,
                                             void *context, uint32_t size, uint8_t *rgba)
{
    SDL_GPUDevice *device = f->gpu.device;
    SDL_GPUTextureCreateInfo texture_info;
    SDL_zero(texture_info);
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    texture_info.width = size;
    texture_info.height = size;
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &texture_info);
    SDL_GPUTexture *depth_texture = NULL;
    if (depth_format != 0) {
        SDL_GPUTextureCreateInfo depth_info = texture_info;
        depth_info.format = (SDL_GPUTextureFormat)depth_format;
        depth_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        depth_texture = SDL_CreateGPUTexture(device, &depth_info);
    }

    const SDL_GPUTransferBufferCreateInfo buffer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = size * size * 4};
    SDL_GPUTransferBuffer *buffer = SDL_CreateGPUTransferBuffer(device, &buffer_info);

    bool ok = false;
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
    if (texture != NULL && buffer != NULL && commands != NULL &&
        (depth_format == 0 || depth_texture != NULL)) {
        const SDL_GPUColorTargetInfo color = {.texture = texture,
                                              .clear_color = {0.0F, 0.0F, 0.0F, 1.0F},
                                              .load_op = SDL_GPU_LOADOP_CLEAR,
                                              .store_op = SDL_GPU_STOREOP_STORE};
        SDL_GPUDepthStencilTargetInfo depth;
        eng_gpu_fill_depth_target(depth_texture, &depth); /* les vrais réglages de la passe */
        if (prepare != NULL) {
            prepare(commands, context);
        }
        SDL_GPURenderPass *pass =
            SDL_BeginGPURenderPass(commands, &color, 1, depth_texture != NULL ? &depth : NULL);
        draw(commands, pass, context);
        SDL_EndGPURenderPass(pass);

        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
        const SDL_GPUTextureRegion region = {.texture = texture, .w = size, .h = size, .d = 1};
        const SDL_GPUTextureTransferInfo destination = {
            .transfer_buffer = buffer, .pixels_per_row = size, .rows_per_layer = size};
        SDL_DownloadFromGPUTexture(copy, &region, &destination);
        SDL_EndGPUCopyPass(copy);

        SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
        commands = NULL;
        if (fence != NULL) {
            ok = SDL_WaitForGPUFences(device, true, &fence, 1);
            SDL_ReleaseGPUFence(device, fence);
        }
        if (ok) {
            const uint8_t *mapped =
                (const uint8_t *)SDL_MapGPUTransferBuffer(device, buffer, false);
            ok = mapped != NULL;
            if (ok) {
                memcpy(rgba, mapped, (size_t)size * size * 4);
                SDL_UnmapGPUTransferBuffer(device, buffer);
            }
        }
    }
    if (commands != NULL) {
        (void)SDL_CancelGPUCommandBuffer(commands);
    }
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    SDL_ReleaseGPUTexture(device, texture);
    if (depth_texture != NULL) {
        SDL_ReleaseGPUTexture(device, depth_texture);
    }
    return ok;
}

static inline bool render_to_pixels_depth(RenderFixture *f, uint32_t depth_format,
                                          RenderDrawFn draw, void *context, uint32_t size,
                                          uint8_t *rgba)
{
    return render_to_pixels_prepared(f, depth_format, NULL, draw, context, size, rgba);
}

static inline bool render_to_pixels(RenderFixture *f, RenderDrawFn draw, void *context,
                                    uint32_t size, uint8_t *rgba)
{
    return render_to_pixels_depth(f, 0, draw, context, size, rgba);
}

/* Vrai si le pixel (x, y) vaut (r, g, b) à `tolerance` près par canal. */
static inline bool pixel_is(const uint8_t *rgba, uint32_t size, uint32_t x, uint32_t y, int r,
                            int g, int b, int tolerance)
{
    const uint8_t *p = rgba + ((size_t)y * size + x) * 4;
    return abs((int)p[0] - r) <= tolerance && abs((int)p[1] - g) <= tolerance &&
           abs((int)p[2] - b) <= tolerance && p[3] == 255;
}

/* Comme render_to_pixels_prepared, mais en DEUX passes sur la même texture, comme une image du jeu
 * avec interface : une passe « scène » qui efface la texture à `background` (la scène est remplacée
 * par ce fond uni), puis une passe overlay SANS profondeur avec les vrais réglages de
 * eng_gpu_fill_overlay_target (la scène est conservée). `prepare` est appelée avant toute passe
 * (envoi de données au GPU), `draw_overlay` dans la passe overlay. */
static inline bool render_to_pixels_overlay(RenderFixture *f, const float background[4],
                                            RenderPrepareFn prepare, RenderDrawFn draw_overlay,
                                            void *context, uint32_t size, uint8_t *rgba)
{
    SDL_GPUDevice *device = f->gpu.device;
    SDL_GPUTextureCreateInfo texture_info;
    SDL_zero(texture_info);
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    texture_info.width = size;
    texture_info.height = size;
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &texture_info);
    const SDL_GPUTransferBufferCreateInfo buffer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = size * size * 4};
    SDL_GPUTransferBuffer *buffer = SDL_CreateGPUTransferBuffer(device, &buffer_info);

    bool ok = false;
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
    if (texture != NULL && buffer != NULL && commands != NULL) {
        if (prepare != NULL) {
            prepare(commands, context);
        }
        const SDL_GPUColorTargetInfo scene = {
            .texture = texture,
            .clear_color = {background[0], background[1], background[2], background[3]},
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE};
        SDL_GPURenderPass *scene_pass = SDL_BeginGPURenderPass(commands, &scene, 1, NULL);
        SDL_EndGPURenderPass(scene_pass);
        SDL_GPUColorTargetInfo overlay;
        eng_gpu_fill_overlay_target(texture, &overlay);
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &overlay, 1, NULL);
        draw_overlay(commands, pass, context);
        SDL_EndGPURenderPass(pass);

        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
        const SDL_GPUTextureRegion region = {.texture = texture, .w = size, .h = size, .d = 1};
        const SDL_GPUTextureTransferInfo destination = {
            .transfer_buffer = buffer, .pixels_per_row = size, .rows_per_layer = size};
        SDL_DownloadFromGPUTexture(copy, &region, &destination);
        SDL_EndGPUCopyPass(copy);

        SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
        commands = NULL;
        if (fence != NULL) {
            ok = SDL_WaitForGPUFences(device, true, &fence, 1);
            SDL_ReleaseGPUFence(device, fence);
        }
        if (ok) {
            const uint8_t *mapped =
                (const uint8_t *)SDL_MapGPUTransferBuffer(device, buffer, false);
            ok = mapped != NULL;
            if (ok) {
                memcpy(rgba, mapped, (size_t)size * size * 4);
                SDL_UnmapGPUTransferBuffer(device, buffer);
            }
        }
    }
    if (commands != NULL) {
        (void)SDL_CancelGPUCommandBuffer(commands);
    }
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    SDL_ReleaseGPUTexture(device, texture);
    return ok;
}
