/* Écran d'attente : la fenêtre est peinte par le processeur pendant que le pilote de la carte
 * graphique démarre dans un fil à part. Voir splash.h ; le dessin : engine/boot_screen.h. */
#include <SDL3/SDL.h>
#include <string.h>

#include "engine/boot_screen.h"
#include "engine/log.h"
#include "splash.h"

#define FRAME_NS                                                                                   \
    16000000ULL /* une image toutes les 16 ms : la surface logicielle n'a pas de vsync */

typedef struct JobRun {
    SplashJob job;
    void *user;
    SDL_AtomicInt done;
    bool result;
} JobRun;

/* l'image de la zone animée, pour la taille courante de la fenêtre */
typedef struct Canvas {
    SDL_Surface *zone; /* XRGB8888, de la taille de `rect` */
    EngBootRect rect;
    int width; /* taille de la fenêtre pour laquelle `zone` est faite */
    int height;
    bool labelled; /* la zone a été calculée avec la légende */
} Canvas;

static int SDLCALL job_thread(void *user)
{
    JobRun *run = user;
    run->result = run->job(run->user);
    SDL_SetAtomicInt(&run->done, 1);
    return 0;
}

/* Les pilotes vidéo dont la surface de fenêtre est native et sans rapport avec le pilote graphique
 * (X11 : mémoire partagée ; offscreen : une image en mémoire). Ailleurs (Wayland, Windows...), ce
 * chemin n'a pas été essayé : le lancement y reste celui d'avant. */
static bool driver_supported(void)
{
    const char *driver = SDL_GetCurrentVideoDriver();
    return driver != NULL &&
           (SDL_strcmp(driver, "x11") == 0 || SDL_strcmp(driver, "offscreen") == 0);
}

static void canvas_free(Canvas *c)
{
    if (c->zone != NULL) {
        SDL_DestroySurface(c->zone);
    }
    memset(c, 0, sizeof *c);
}

/* Prépare la zone animée pour la taille de `screen`. `repaint` : la fenêtre entière est à
 * repeindre (première image, nouvelle taille, zone agrandie par la légende). false : rien à
 * dessiner. */
static bool canvas_fit(Canvas *c, SDL_Surface *screen, const EngBootText *label, bool *repaint)
{
    *repaint = false;
    if (c->zone != NULL && c->width == screen->w && c->height == screen->h &&
        c->labelled == (label != NULL)) {
        return true;
    }
    canvas_free(c);
    c->width = screen->w;
    c->height = screen->h;
    c->labelled = label != NULL;
    *repaint = true;
    (void)SDL_FillSurfaceRect(screen, NULL, SDL_MapSurfaceRGB(screen, 0, 0, 0));
    c->rect = eng_boot_bounds((uint32_t)screen->w, (uint32_t)screen->h, label);
    if (c->rect.width == 0U || c->rect.height == 0U) {
        return false;
    }
    c->zone = SDL_CreateSurface((int)c->rect.width, (int)c->rect.height, SDL_PIXELFORMAT_XRGB8888);
    return c->zone != NULL;
}

/* une image : la corde à l'instant `p->time_s`, sur fond noir */
static void draw_frame(SplashPrelude *p, Canvas *c, SDL_Window *window, const EngBootText *label)
{
    SDL_Surface *screen = SDL_GetWindowSurface(window); /* recréée par SDL si la taille a changé */
    if (screen == NULL || screen->w <= 0 || screen->h <= 0) {
        return;
    }
    bool repaint = false;
    const bool fits = canvas_fit(c, screen, label, &repaint);
    const SDL_Rect dst = {(int)c->rect.x, (int)c->rect.y, (int)c->rect.width, (int)c->rect.height};
    if (fits) {
        const EngBootFrame frame = {.width = (uint32_t)screen->w,
                                    .height = (uint32_t)screen->h,
                                    .time_s = (float)p->time_s,
                                    .progress = p->progress,
                                    .label = label};
        eng_boot_draw(&frame, c->zone->pixels, (size_t)c->zone->pitch);
        (void)SDL_BlitSurface(c->zone, NULL, screen, &dst);
    }
    if (repaint || !fits) {
        (void)SDL_UpdateWindowSurface(window);
    } else {
        (void)SDL_UpdateWindowSurfaceRects(window, &dst, 1);
    }
    p->frames++;
}

bool splash_prelude_run(SplashPrelude *p, EngPlatform *platform, SplashJob job, void *user)
{
    if (p == NULL || job == NULL) {
        return false;
    }
    memset(p, 0, sizeof *p);
    if (platform == NULL || platform->window == NULL || !driver_supported()) {
        return job(user);
    }
    const uint64_t t0 = SDL_GetTicksNS();
    /* La surface NATIVE du pilote vidéo. Sans cette consigne, SDL dessinerait la surface avec un
     * moteur de rendu (OpenGL) : il faudrait le pilote graphique que l'on attend justement. */
    (void)SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
    if (SDL_GetWindowSurface(platform->window) == NULL) {
        ENG_LOG_WARN("écran d'attente indisponible (surface de la fenêtre : %s)", SDL_GetError());
        return job(user);
    }
    Canvas canvas;
    memset(&canvas, 0, sizeof canvas);
    /* tout de suite : la fenêtre est noire (la corde part du noir, en fondu) */
    draw_frame(p, &canvas, platform->window, NULL);
    p->first_ms = (double)(SDL_GetTicksNS() - t0) * 1e-6;

    JobRun run = {.job = job, .user = user};
    SDL_SetAtomicInt(&run.done, 0);
    const uint64_t job_t0 = SDL_GetTicksNS();
    SDL_Thread *thread = SDL_CreateThread(job_thread, "pilote graphique", &run);
    if (thread == NULL) {
        ENG_LOG_WARN("écran d'attente : fil indisponible (%s), démarrage direct", SDL_GetError());
        canvas_free(&canvas);
        (void)SDL_DestroyWindowSurface(platform->window);
        return job(user);
    }
    /* la légende : construite pendant que le pilote démarre déjà (quelques dizaines de ms) */
    EngBootText label;
    memset(&label, 0, sizeof label);
    const bool has_label = splash_text_build(&p->text);
    if (has_label) {
        label.pixels = p->text.pixels;
        label.width = p->text.width;
        label.height = p->text.height;
        memcpy(label.rect, p->text.label_rect, sizeof label.rect);
        label.aspect = p->text.text_info[1];
    }
    uint64_t last = SDL_GetTicksNS();
    while (SDL_GetAtomicInt(&run.done) == 0) {
        (void)eng_platform_pump(platform); /* la fermeture demandée reste dans quit_requested */
        const uint64_t now = SDL_GetTicksNS();
        double dt = (double)(now - last) * 1e-9;
        last = now;
        if (p->frames > 1U && dt * 1000.0 > p->worst_gap_ms) {
            p->worst_gap_ms = dt * 1000.0;
        }
        dt = dt > 0.1 ? 0.1 : dt; /* un à-coup ne fait pas sauter l'animation */
        p->time_s += dt;
        p->progress = eng_boot_progress(p->progress, (float)dt);
        draw_frame(p, &canvas, platform->window, has_label ? &label : NULL);
        const uint64_t spent = SDL_GetTicksNS() - now;
        if (spent < FRAME_NS) {
            SDL_DelayNS(FRAME_NS - spent);
        }
    }
    SDL_WaitThread(thread, NULL);
    p->job_s = (double)(SDL_GetTicksNS() - job_t0) * 1e-9;
    canvas_free(&canvas);
    /* la fenêtre va au GPU : elle ne peut pas avoir à la fois une surface et une swapchain. Sa
     * dernière image reste affichée jusqu'à la première image du GPU. */
    (void)SDL_DestroyWindowSurface(platform->window);
    p->shown = true;
    ENG_LOG_INFO("écran d'attente : %u image(s) pendant le démarrage de la carte graphique "
                 "(%.2f s), fenêtre peinte après %.0f ms, plus long écart entre deux images "
                 "%.0f ms, corde à %.0f %%",
                 p->frames, p->job_s, p->first_ms, p->worst_gap_ms, (double)p->progress * 100.0);
    return run.result;
}

void splash_prelude_shutdown(SplashPrelude *p)
{
    if (p != NULL) {
        splash_text_free(&p->text);
    }
}
