/* Écran de chargement : image d'un instant donné, hors écran, écrite en PPM et mesurée
 * (développement, autotest). Voir splash.h. */
#include <stdio.h>

#include "splash_internal.h"

/* part des pixels du rectangle (fractions de l'image) qui satisfont `kind` : 0 ivoire, 1 rouge */
static float count_pixels(const uint8_t *px, bool bgra, uint32_t width, uint32_t height, float x0,
                          float x1, float y0, float y1, int kind)
{
    uint32_t hits = 0;
    uint32_t total = 0;
    for (uint32_t y = (uint32_t)(y0 * (float)height); y < (uint32_t)(y1 * (float)height); y++) {
        for (uint32_t x = (uint32_t)(x0 * (float)width); x < (uint32_t)(x1 * (float)width); x++) {
            const uint8_t *p = &px[(((size_t)y * width) + x) * 4U];
            const int r = p[bgra ? 2 : 0];
            const int g = p[1];
            const int b = p[bgra ? 0 : 2];
            const bool hit = kind == 0 ? (r > 215 && g > 190 && b > 160)
                                       : (r > 190 && g < 150 && b < 175 && r - g > 80);
            hits += hit ? 1U : 0U;
            total++;
        }
    }
    return total > 0U ? (float)hits / (float)total : 0.0F;
}

static void measure(const uint8_t *px, bool bgra, uint32_t width, uint32_t height,
                    SplashStats *stats)
{
    uint64_t sum = 0;
    for (size_t i = 0; i < (size_t)width * height; i++) {
        sum += (uint64_t)px[i * 4U] + px[(i * 4U) + 1U] + px[(i * 4U) + 2U];
    }
    stats->luma = (float)((double)sum / (765.0 * (double)width * (double)height));
    stats->title = count_pixels(px, bgra, width, height, 0.2F, 0.8F, 0.18F, 0.31F, 0);
    stats->title_left = count_pixels(px, bgra, width, height, 0.2F, 0.5F, 0.18F, 0.31F, 0);
    stats->title_right = count_pixels(px, bgra, width, height, 0.5F, 0.8F, 0.18F, 0.31F, 0);
    /* entre le chevalier et le château, au-dessus des montagnes : seul le fil y est rouge (plus à
     * droite, la lave du volcan le serait aussi) */
    stats->thread = count_pixels(px, bgra, width, height, 0.25F, 0.45F, 0.36F, 0.52F, 1);
    stats->bar = count_pixels(px, bgra, width, height, 0.3F, 0.7F, 0.925F, 0.945F, 1);
}

bool splash_capture(Splash *s, double time_s, float progress, float outro, uint32_t width,
                    uint32_t height, const char *path, SplashStats *stats)
{
    if (s == NULL || !s->ready || width == 0U || height == 0U) {
        return false;
    }
    const SDL_GPUTextureFormat format = SDL_GetGPUSwapchainTextureFormat(s->device, s->window);
    SDL_GPUTextureCreateInfo info;
    SDL_zero(info);
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture *target = SDL_CreateGPUTexture(s->device, &info);
    const SDL_GPUTransferBufferCreateInfo buffer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = width * height * 4U};
    SDL_GPUTransferBuffer *buffer = SDL_CreateGPUTransferBuffer(s->device, &buffer_info);
    SDL_GPUCommandBuffer *cmd =
        target != NULL && buffer != NULL ? SDL_AcquireGPUCommandBuffer(s->device) : NULL;
    bool ok = false;
    if (cmd != NULL) {
        SplashUniforms uniforms;
        splash_fill_uniforms(s, &uniforms, time_s, progress, outro, width, height);
        splash_draw(s, cmd, target, &uniforms);
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
        const SDL_GPUTextureRegion region = {.texture = target, .w = width, .h = height, .d = 1};
        const SDL_GPUTextureTransferInfo dst = {
            .transfer_buffer = buffer, .pixels_per_row = width, .rows_per_layer = height};
        SDL_DownloadFromGPUTexture(copy, &region, &dst);
        SDL_EndGPUCopyPass(copy);
        SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        if (fence != NULL) {
            (void)SDL_WaitForGPUFences(s->device, true, &fence, 1);
            SDL_ReleaseGPUFence(s->device, fence);
            const uint8_t *px = SDL_MapGPUTransferBuffer(s->device, buffer, false);
            const bool bgra = format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM ||
                              format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
            ok = px != NULL;
            if (px != NULL && stats != NULL) {
                measure(px, bgra, width, height, stats);
            }
            FILE *f = px != NULL && path != NULL ? fopen(path, "wb") : NULL;
            if (f != NULL) {
                (void)fprintf(f, "P6\n%u %u\n255\n", (unsigned)width, (unsigned)height);
                for (size_t i = 0; i < (size_t)width * height; i++) {
                    const uint8_t rgb[3] = {px[(i * 4U) + (bgra ? 2U : 0U)], px[(i * 4U) + 1U],
                                            px[(i * 4U) + (bgra ? 0U : 2U)]};
                    (void)fwrite(rgb, 1, 3, f);
                }
                ok = fclose(f) == 0;
            } else if (path != NULL) {
                ok = false;
            }
            if (px != NULL) {
                SDL_UnmapGPUTransferBuffer(s->device, buffer);
            }
        }
    }
    if (buffer != NULL) {
        SDL_ReleaseGPUTransferBuffer(s->device, buffer);
    }
    if (target != NULL) {
        SDL_ReleaseGPUTexture(s->device, target);
    }
    return ok;
}
