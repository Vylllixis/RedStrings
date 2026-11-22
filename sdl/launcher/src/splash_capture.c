/* Écran de chargement : image d'un instant donné, hors écran, écrite en PPM et mesurée
 * (développement, autotest), et relais entre l'écran d'attente et la scène. Voir splash.h. */
#include <stdio.h>
#include <string.h>

#include "engine/boot_screen.h"
#include "splash_internal.h"

/* part des pixels du rectangle (fractions de l'image) qui satisfont `kind` : 0 ivoire, 1 rouge ;
 * `px` : trois octets (rouge, vert, bleu) par pixel */
static float count_pixels(const uint8_t *px, uint32_t width, uint32_t height, float x0, float x1,
                          float y0, float y1, int kind)
{
    uint32_t hits = 0;
    uint32_t total = 0;
    for (uint32_t y = (uint32_t)(y0 * (float)height); y < (uint32_t)(y1 * (float)height); y++) {
        for (uint32_t x = (uint32_t)(x0 * (float)width); x < (uint32_t)(x1 * (float)width); x++) {
            const uint8_t *p = &px[(((size_t)y * width) + x) * 3U];
            const int r = p[0];
            const int g = p[1];
            const int b = p[2];
            const bool hit = kind == 0 ? (r > 215 && g > 190 && b > 160)
                                       : (r > 190 && g < 150 && b < 175 && r - g > 80);
            hits += hit ? 1U : 0U;
            total++;
        }
    }
    return total > 0U ? (float)hits / (float)total : 0.0F;
}

static void measure(const uint8_t *px, uint32_t width, uint32_t height, SplashStats *stats)
{
    uint64_t sum = 0;
    for (size_t i = 0; i < (size_t)width * height * 3U; i++) {
        sum += px[i];
    }
    stats->luma = (float)((double)sum / (765.0 * (double)width * (double)height));
    stats->title = count_pixels(px, width, height, 0.2F, 0.8F, 0.18F, 0.31F, 0);
    stats->title_left = count_pixels(px, width, height, 0.2F, 0.5F, 0.18F, 0.31F, 0);
    stats->title_right = count_pixels(px, width, height, 0.5F, 0.8F, 0.18F, 0.31F, 0);
    /* entre le chevalier et le château, au-dessus des montagnes : seul le fil y est rouge (plus à
     * droite, la lave du volcan le serait aussi) */
    stats->thread = count_pixels(px, width, height, 0.25F, 0.45F, 0.36F, 0.52F, 1);
    stats->bar = count_pixels(px, width, height, 0.3F, 0.7F, 0.925F, 0.945F, 1);
}

static bool write_ppm(const char *path, const uint8_t *rgb, uint32_t width, uint32_t height)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return false;
    }
    (void)fprintf(f, "P6\n%u %u\n255\n", (unsigned)width, (unsigned)height);
    const size_t bytes = (size_t)width * height * 3U;
    const bool written = fwrite(rgb, 1, bytes, f) == bytes;
    return fclose(f) == 0 && written;
}

/* La scène dessinée hors écran avec ces uniforms, relue : trois octets (rouge, vert, bleu) par
 * pixel, alloués (SDL_free). NULL en cas d'échec. */
static uint8_t *render_rgb(Splash *s, const SplashUniforms *uniforms, uint32_t width,
                           uint32_t height)
{
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
    uint8_t *rgb = NULL;
    if (cmd != NULL) {
        splash_draw(s, cmd, target, uniforms);
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
            rgb = px != NULL ? SDL_malloc((size_t)width * height * 3U) : NULL;
            for (size_t i = 0; rgb != NULL && i < (size_t)width * height; i++) {
                rgb[i * 3U] = px[(i * 4U) + (bgra ? 2U : 0U)];
                rgb[(i * 3U) + 1U] = px[(i * 4U) + 1U];
                rgb[(i * 3U) + 2U] = px[(i * 4U) + (bgra ? 0U : 2U)];
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
    return rgb;
}

bool splash_capture(Splash *s, double time_s, float progress, float outro, uint32_t width,
                    uint32_t height, const char *path, SplashStats *stats)
{
    if (s == NULL || !s->ready || width == 0U || height == 0U) {
        return false;
    }
    SplashUniforms uniforms;
    splash_fill_uniforms(s, &uniforms, time_s, progress, outro, width, height);
    uint8_t *rgb = render_rgb(s, &uniforms, width, height);
    if (rgb == NULL) {
        return false;
    }
    if (stats != NULL) {
        measure(rgb, width, height, stats);
    }
    const bool ok = path == NULL || write_ppm(path, rgb, width, height);
    SDL_free(rgb);
    return ok;
}

/* la dernière image de l'écran d'attente, dessinée par le processeur : noir, puis la zone animée */
static uint8_t *prelude_rgb(const SplashText *text, double prelude_s, float progress,
                            uint32_t width, uint32_t height)
{
    EngBootText label;
    memset(&label, 0, sizeof label);
    label.pixels = text->pixels;
    label.width = text->width;
    label.height = text->height;
    memcpy(label.rect, text->label_rect, sizeof label.rect);
    label.aspect = text->text_info[1];
    const EngBootFrame frame = {.width = width,
                                .height = height,
                                .time_s = (float)prelude_s,
                                .progress = progress,
                                .label = &label};
    const EngBootRect zone = eng_boot_bounds(width, height, &label);
    uint32_t *zone_px = SDL_malloc((size_t)zone.width * zone.height * sizeof(uint32_t) + 1U);
    uint8_t *rgb = SDL_calloc((size_t)width * height, 3U);
    if (zone_px != NULL && rgb != NULL) {
        eng_boot_draw(&frame, zone_px, (size_t)zone.width * sizeof(uint32_t));
        for (uint32_t y = 0; y < zone.height; y++) {
            for (uint32_t x = 0; x < zone.width; x++) {
                const uint32_t c = zone_px[((size_t)y * zone.width) + x];
                uint8_t *out = &rgb[((((size_t)zone.y + y) * width) + zone.x + x) * 3U];
                out[0] = (uint8_t)(c >> 16U);
                out[1] = (uint8_t)(c >> 8U);
                out[2] = (uint8_t)c;
            }
        }
    } else {
        SDL_free(rgb);
        rgb = NULL;
    }
    SDL_free(zone_px);
    return rgb;
}

bool splash_handover(Splash *s, const SplashText *text, double prelude_s, float progress,
                     uint32_t width, uint32_t height, const char *dir, const char *name,
                     SplashHandover *out)
{
    if (s == NULL || !s->ready || text == NULL || text->pixels == NULL || out == NULL ||
        width == 0U || height == 0U) {
        return false;
    }
    memset(out, 0, sizeof *out);
    /* la première image de la scène après cette attente : temps 0, la corde à `prelude_s` */
    const double saved = s->prelude_s;
    s->prelude_s = prelude_s;
    SplashUniforms uniforms;
    splash_fill_uniforms(s, &uniforms, 0.0, progress, 0.0F, width, height);
    s->prelude_s = saved;
    uint8_t *gpu = render_rgb(s, &uniforms, width, height);
    uint8_t *cpu = prelude_rgb(text, prelude_s, progress, width, height);
    bool ok = gpu != NULL && cpu != NULL;
    if (ok) {
        out->pixels = width * height;
        for (size_t i = 0; i < (size_t)width * height; i++) {
            uint32_t gap = 0;
            uint32_t light = 0;
            for (size_t k = 0; k < 3U; k++) {
                const int d = (int)gpu[(i * 3U) + k] - (int)cpu[(i * 3U) + k];
                const uint32_t a = (uint32_t)(d < 0 ? -d : d);
                gap = a > gap ? a : gap;
                light += cpu[(i * 3U) + k];
            }
            out->worst = gap > out->worst ? gap : out->worst;
            out->differing += gap > 2U ? 1U : 0U;
            out->lit += light > 0U ? 1U : 0U;
        }
    }
    if (ok && dir != NULL && name != NULL) {
        char path[1024];
        (void)snprintf(path, sizeof path, "%s/relais-%s-gpu.ppm", dir, name);
        ok = write_ppm(path, gpu, width, height);
        (void)snprintf(path, sizeof path, "%s/relais-%s-cpu.ppm", dir, name);
        ok = write_ppm(path, cpu, width, height) && ok;
    }
    SDL_free(gpu);
    SDL_free(cpu);
    return ok;
}
