/* Écran de chargement animé : l'animation sur le fil principal et le chargement dans un fil à
 * part. Voir splash.h ; ressources : splash_build.c ; images hors écran : splash_capture.c. */
#include "splash.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/log.h"
#include "splash_internal.h"

#define TITLE_START_S 1.1 /* le titre commence à apparaître */
#define TITLE_TIME_S 1.5

typedef struct JobRun {
    SplashJob job;
    void *user;
    SDL_AtomicInt done;
    bool result;
} JobRun;

bool splash_init(Splash *s, EngGpu *gpu, EngPlatform *platform)
{
    memset(s, 0, sizeof *s);
    if (gpu == NULL || gpu->device == NULL || platform == NULL || platform->window == NULL) {
        return false;
    }
    s->device = gpu->device;
    s->window = platform->window;
    EngArena scratch;
    if (!eng_arena_init(&scratch, "chargement", (size_t)8 << 20)) {
        return false;
    }
    const bool ok = splash_build_pipeline(s, gpu, &scratch) && splash_build_text(s, &scratch);
    eng_arena_shutdown(&scratch);
    if (!ok) {
        ENG_LOG_WARN("écran de chargement indisponible (shader, police ou texture) : %s",
                     SDL_GetError());
        splash_shutdown(s);
        return false;
    }
    s->last_ns = SDL_GetTicksNS();
    s->ready = true;
    ENG_LOG_INFO("écran de chargement : prêt (animation sur le fil principal, chargement dans un "
                 "fil à part)");
    return true;
}

void splash_shutdown(Splash *s)
{
    if (s == NULL || s->device == NULL) {
        return;
    }
    if (s->pipeline != NULL) {
        SDL_ReleaseGPUGraphicsPipeline(s->device, s->pipeline);
    }
    if (s->text != NULL) {
        SDL_ReleaseGPUTexture(s->device, s->text);
    }
    if (s->sampler != NULL) {
        SDL_ReleaseGPUSampler(s->device, s->sampler);
    }
    s->pipeline = NULL;
    s->text = NULL;
    s->sampler = NULL;
    s->ready = false;
}

void splash_fill_uniforms(const Splash *s, SplashUniforms *u, double time_s, float progress,
                          float outro, uint32_t width, uint32_t height)
{
    const double reveal = (time_s - TITLE_START_S) / TITLE_TIME_S;
    u->screen[0] = (float)width;
    u->screen[1] = (float)height;
    u->screen[2] = (float)time_s;
    u->screen[3] = progress;
    u->anim[0] = outro;
    u->anim[1] = (float)(reveal < 0.0 ? 0.0 : (reveal > 1.0 ? 1.0 : reveal));
    u->anim[2] = 7.0F;
    u->anim[3] = 0.0F;
    memcpy(u->title_rect, s->title_rect, sizeof u->title_rect);
    memcpy(u->label_rect, s->label_rect, sizeof u->label_rect);
    memcpy(u->text_info, s->text_info, sizeof u->text_info);
}

/* la scène dans `target` : un triangle plein écran */
void splash_draw(const Splash *s, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target,
                 const SplashUniforms *uniforms)
{
    SDL_GPUColorTargetInfo color;
    SDL_zero(color);
    color.texture = target;
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    color.clear_color = (SDL_FColor){0.0F, 0.0F, 0.0F, 1.0F};
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color, 1, NULL);
    if (pass == NULL) {
        return;
    }
    SDL_BindGPUGraphicsPipeline(pass, s->pipeline);
    const SDL_GPUTextureSamplerBinding binding = {.texture = s->text, .sampler = s->sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    SDL_PushGPUFragmentUniformData(cmd, 0, uniforms, (Uint32)sizeof *uniforms);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

/* une image à l'écran : le temps avance, la progression se lisse */
static void present(Splash *s, float outro)
{
    const uint64_t now = SDL_GetTicksNS();
    double dt = (double)(now - s->last_ns) * 1e-9;
    s->last_ns = now;
    if (s->frames > 0U && dt * 1000.0 > s->worst_gap_ms) {
        s->worst_gap_ms = dt * 1000.0;
    }
    dt = dt > 0.1 ? 0.1 : dt; /* un à-coup ne fait pas sauter l'animation */
    s->time_s += dt;
    if (s->loading) { /* l'étape avance sans jamais atteindre sa fin avant d'être finie */
        s->target += (s->stage_end - 0.03F - s->target) * (float)(1.0 - exp(-dt / 1.6));
    }
    s->progress += (s->target - s->progress) * (float)(1.0 - exp(-dt * 7.0));
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(s->device);
    if (cmd == NULL) {
        return;
    }
    SDL_GPUTexture *target = NULL;
    Uint32 width = 0;
    Uint32 height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, s->window, &target, &width, &height)) {
        (void)SDL_CancelGPUCommandBuffer(cmd);
        SDL_Delay(4);
        return;
    }
    if (target == NULL) { /* fenêtre réduite */
        (void)SDL_SubmitGPUCommandBuffer(cmd);
        SDL_Delay(8);
        return;
    }
    SplashUniforms uniforms;
    splash_fill_uniforms(s, &uniforms, s->time_s, s->progress, outro, width, height);
    splash_draw(s, cmd, target, &uniforms);
    (void)SDL_SubmitGPUCommandBuffer(cmd);
    s->frames++;
    s->frames_loading += s->loading ? 1U : 0U;
    const uint64_t spent = SDL_GetTicksNS() - now;
    if (spent < 3000000ULL) { /* sans vsync : pas plus de ~300 images par seconde */
        SDL_DelayNS(3000000ULL - spent);
    }
}

static void pump(Splash *s, EngPlatform *platform)
{
    (void)eng_platform_pump(platform); /* la fermeture demandée reste dans quit_requested */
    if (s->time_s > 1.0) {             /* une touche ou un clic : passer l'animation */
        int count = 0;
        const bool *keys = SDL_GetKeyboardState(&count);
        for (int k = 0; keys != NULL && k < count && !s->skip; k++) {
            s->skip = keys[k];
        }
        s->skip = s->skip || SDL_GetMouseState(NULL, NULL) != 0U;
    }
}

void splash_frame(Splash *s, EngPlatform *platform)
{
    if (s == NULL || !s->ready || platform == NULL) {
        return;
    }
    pump(s, platform);
    present(s, 0.0F);
}

static int SDLCALL job_thread(void *user)
{
    JobRun *run = user;
    run->result = run->job(run->user);
    SDL_SetAtomicInt(&run->done, 1);
    return 0;
}

bool splash_run(Splash *s, EngPlatform *platform, SplashJob job, void *user, float progress_end)
{
    if (job == NULL) {
        return false;
    }
    if (s == NULL || !s->ready || platform == NULL) {
        return job(user);
    }
    JobRun run = {.job = job, .user = user};
    SDL_SetAtomicInt(&run.done, 0);
    const uint64_t t0 = SDL_GetTicksNS();
    SDL_Thread *thread = SDL_CreateThread(job_thread, "chargement", &run);
    if (thread == NULL) {
        ENG_LOG_WARN("écran de chargement : fil indisponible (%s), chargement direct",
                     SDL_GetError());
        return job(user);
    }
    s->stage_end =
        progress_end < s->target ? s->target : (progress_end > 1.0F ? 1.0F : progress_end);
    s->loading = true;
    while (SDL_GetAtomicInt(&run.done) == 0) {
        pump(s, platform);
        present(s, 0.0F);
    }
    SDL_WaitThread(thread, NULL);
    s->loading = false;
    s->target = s->stage_end;
    s->jobs++;
    s->loading_s += (double)(SDL_GetTicksNS() - t0) * 1e-9;
    return run.result;
}

void splash_finish(Splash *s, EngPlatform *platform)
{
    if (s == NULL || !s->ready || platform == NULL) {
        return;
    }
    s->target = 1.0F;
    while (s->time_s < SPLASH_MIN_SECONDS && !s->skip && !platform->quit_requested) {
        pump(s, platform);
        present(s, 0.0F);
    }
    const double outro_from = s->time_s;
    while (s->time_s - outro_from < SPLASH_OUTRO_SECONDS && !platform->quit_requested) {
        pump(s, platform);
        present(s, (float)((s->time_s - outro_from) / SPLASH_OUTRO_SECONDS));
    }
    ENG_LOG_INFO("écran de chargement : %u image(s) dont %u pendant le chargement (%u travaux "
                 "dans le fil de chargement, %.1f s), animation de %.1f s, plus long écart entre "
                 "deux images %.0f ms%s",
                 s->frames, s->frames_loading, s->jobs, s->loading_s, s->time_s, s->worst_gap_ms,
                 s->skip ? " ; passée par une touche" : "");
    (void)SDL_WaitForGPUIdle(s->device); /* plus aucune image de l'écran en vol */
    splash_shutdown(s);
}
