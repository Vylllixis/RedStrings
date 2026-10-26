#include "engine/platform.h"

#include <SDL3/SDL.h>
#include <stdio.h>

bool eng_platform_init(EngPlatform *p, const EngWindowDesc *desc)
{
    *p = (EngPlatform){0};

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "SDL_Init a échoué : %s\n", SDL_GetError());
        return false;
    }

    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (desc->resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    p->window = SDL_CreateWindow(desc->title, desc->width, desc->height, flags);
    if (p->window == NULL) {
        fprintf(stderr, "SDL_CreateWindow a échoué : %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    if (desc->fullscreen) {
        SDL_SetWindowFullscreen(p->window, true);
        p->fullscreen = true;
    }
    SDL_GetWindowSizeInPixels(p->window, &p->width, &p->height);
    return true;
}

void eng_platform_shutdown(EngPlatform *p)
{
    if (p->window != NULL) {
        SDL_DestroyWindow(p->window);
        p->window = NULL;
        SDL_Quit();
    }
}

bool eng_platform_pump(EngPlatform *p)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            p->quit_requested = true;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            p->width = ev.window.data1;
            p->height = ev.window.data2;
            printf("fenêtre : %d x %d px\n", p->width, p->height);
            break;
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            p->fullscreen = true;
            break;
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            p->fullscreen = false;
            break;
        case SDL_EVENT_KEY_DOWN:
            /* Touches provisoires, remplacées par le système d'entrées (M0-18). */
            if (!ev.key.repeat && ev.key.key == SDLK_ESCAPE) {
                p->quit_requested = true;
            } else if (!ev.key.repeat && ev.key.key == SDLK_F11) {
                eng_platform_toggle_fullscreen(p);
            }
            break;
        default:
            break;
        }
    }
    return !p->quit_requested;
}

void eng_platform_toggle_fullscreen(EngPlatform *p)
{
    if (!SDL_SetWindowFullscreen(p->window, !p->fullscreen)) {
        fprintf(stderr, "plein écran impossible : %s\n", SDL_GetError());
    }
}

void eng_platform_present(EngPlatform *p, uint64_t frame)
{
    SDL_Surface *s = SDL_GetWindowSurface(p->window);
    if (s == NULL || s->w <= 0 || s->h <= 0) {
        return;
    }

    SDL_FillSurfaceRect(s, NULL, SDL_MapSurfaceRGB(s, 24, 28, 40));

    int size = s->h / 8 + 1;
    int x = (int)((frame * 4u) % (uint64_t)(s->w + size)) - size;
    SDL_Rect square = {x, s->h / 2 - size / 2, size, size};
    SDL_FillSurfaceRect(s, &square, SDL_MapSurfaceRGB(s, 230, 180, 60));

    SDL_UpdateWindowSurface(p->window);
    SDL_Delay(1); /* provisoire : évite de tourner à 100 % CPU avant M0-15/M0-16 */
}
