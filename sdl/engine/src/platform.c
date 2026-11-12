#include "engine/platform.h"

#include <SDL3/SDL.h>

#include "engine/input.h"
#include "engine/log.h"

bool eng_platform_init(EngPlatform *p, const EngWindowDesc *desc)
{
    *p = (EngPlatform){0};
    p->debug_draw = true;
    eng_input_set_default_bindings(&p->input);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        ENG_LOG_ERROR("SDL_Init a échoué : %s", SDL_GetError());
        return false;
    }

    /* Les manettes sont facultatives : leur échec n'empêche pas de jouer au clavier. */
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        ENG_LOG_WARN("manettes indisponibles : %s", SDL_GetError());
    }

    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (desc->resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    p->window = SDL_CreateWindow(desc->title, desc->width, desc->height, flags);
    if (p->window == NULL) {
        ENG_LOG_ERROR("SDL_CreateWindow a échoué : %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    if (desc->fullscreen) {
        SDL_SetWindowFullscreen(p->window, true);
        p->fullscreen = true;
    }
    SDL_GetWindowSizeInPixels(p->window, &p->width, &p->height);
    if (!desc->no_software_surface) {
        eng_platform_use_software_surface(p, desc->vsync);
    }
    ENG_LOG_INFO("fenêtre ouverte : %d x %d px, pilote vidéo %s", p->width, p->height,
                 SDL_GetCurrentVideoDriver());
    return true;
}

void eng_platform_use_software_surface(EngPlatform *p, bool vsync)
{
    /* Surface créée ici : pas d'allocation à la première image. */
    (void)SDL_GetWindowSurface(p->window);
    p->vsync = vsync && SDL_SetWindowSurfaceVSync(p->window, 1);
    if (vsync && !p->vsync) {
        ENG_LOG_WARN("vsync indisponible (%s) : repli sur une pause de 1 ms par image",
                     SDL_GetError());
    }
}

void eng_platform_shutdown(EngPlatform *p)
{
    if (p->gamepad != NULL) {
        SDL_CloseGamepad(p->gamepad);
        p->gamepad = NULL;
    }
    if (p->window != NULL) {
        SDL_DestroyWindow(p->window);
        p->window = NULL;
        SDL_Quit();
    }
}

bool eng_platform_set_window_size(EngPlatform *p, int width, int height)
{
    if (p->window == NULL || width <= 0 || height <= 0) {
        return false;
    }
    return SDL_SetWindowSize(p->window, width, height);
}

bool eng_platform_pump(EngPlatform *p)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (p->event_hook != NULL && p->event_hook(p->event_hook_user, &ev)) {
            /* Consommé par l'interface. Un relâchement part quand même vers les entrées : sinon
             * une touche pressée avant l'ouverture d'un champ de saisie resterait enfoncée. */
            if (ev.type == SDL_EVENT_KEY_UP && !ev.key.repeat) {
                eng_input_key(&p->input, (int)ev.key.scancode, false);
            } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                eng_input_mouse_button(&p->input, (int)ev.button.button, false);
            }
            continue;
        }
        switch (ev.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            p->quit_requested = true;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            p->width = ev.window.data1;
            p->height = ev.window.data2;
            ENG_LOG_DEBUG("fenêtre redimensionnée : %d x %d px", p->width, p->height);
            break;
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            p->fullscreen = true;
            ENG_LOG_DEBUG("plein écran activé");
            break;
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
            p->fullscreen = false;
            ENG_LOG_DEBUG("plein écran désactivé");
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (ev.key.repeat) {
                break; /* la répétition automatique n'est pas une nouvelle pression */
            }
            eng_input_key(&p->input, (int)ev.key.scancode, ev.key.down);
            /* Touches du SYSTÈME (hors actions de jeu) : quitter et plein écran. */
            if (ev.key.down && ev.key.key == SDLK_ESCAPE) {
                p->quit_requested = true;
            } else if (ev.key.down && ev.key.key == SDLK_F11) {
                eng_platform_toggle_fullscreen(p);
            } else if (ev.key.down && ev.key.key == SDLK_F2) {
                (void)eng_platform_set_mouse_capture(p, !p->mouse_captured);
            } else if (ev.key.down && ev.key.key == SDLK_F3) {
                p->debug_draw = !p->debug_draw;
            } else if (ev.key.down && ev.key.key == SDLK_F1) {
                p->debug_ui = !p->debug_ui;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            eng_input_mouse_button(&p->input, (int)ev.button.button, ev.button.down);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            eng_input_mouse_motion(&p->input, ev.motion.xrel, ev.motion.yrel);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            eng_input_mouse_wheel(&p->input, ev.wheel.y);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            eng_input_pad_button(&p->input, (int)ev.gbutton.button, ev.gbutton.down);
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            eng_input_pad_axis(&p->input, (int)ev.gaxis.axis, ev.gaxis.value);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            if (p->gamepad == NULL) { /* une seule manette pour l'instant */
                p->gamepad = SDL_OpenGamepad(ev.gdevice.which);
                if (p->gamepad != NULL) {
                    ENG_LOG_INFO("manette connectée : %s", SDL_GetGamepadName(p->gamepad));
                } else {
                    ENG_LOG_WARN("manette non ouverte : %s", SDL_GetError());
                }
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (p->gamepad != NULL && SDL_GetGamepadID(p->gamepad) == ev.gdevice.which) {
                SDL_CloseGamepad(p->gamepad);
                p->gamepad = NULL;
                eng_input_pad_reset(&p->input);
                ENG_LOG_INFO("manette déconnectée");
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            eng_input_release_all(&p->input);
            (void)eng_platform_set_mouse_capture(p, false); /* ne pas garder la souris en otage */
            break;
        default:
            break;
        }
    }
    return !p->quit_requested;
}

bool eng_platform_set_mouse_capture(EngPlatform *p, bool captured)
{
    if (p->window == NULL || p->mouse_captured == captured) {
        return p->window != NULL;
    }
    if (!SDL_SetWindowRelativeMouseMode(p->window, captured)) {
        ENG_LOG_WARN("capture de la souris impossible : %s", SDL_GetError());
        return false;
    }
    p->mouse_captured = captured;
    return true;
}

void eng_platform_toggle_fullscreen(EngPlatform *p)
{
    if (!SDL_SetWindowFullscreen(p->window, !p->fullscreen)) {
        ENG_LOG_ERROR("plein écran impossible : %s", SDL_GetError());
    }
}

void eng_platform_set_title(EngPlatform *p, const char *title)
{
    SDL_SetWindowTitle(p->window, title);
}

void eng_platform_present(EngPlatform *p, const EngDebugSquare *square)
{
    SDL_Surface *s = SDL_GetWindowSurface(p->window);
    if (s == NULL || s->w <= 0 || s->h <= 0) {
        return;
    }

    SDL_FillSurfaceRect(s, NULL, SDL_MapSurfaceRGB(s, 24, 28, 40));

    int size = (int)(square->size * (double)s->h) + 1;
    int x = (int)(square->x * (double)(s->w + size)) - size;
    int y = (int)(square->y * (double)(s->h + size)) - size;
    SDL_Rect rect = {x, y, size, size};
    SDL_FillSurfaceRect(s, &rect, SDL_MapSurfaceRGB(s, square->r, square->g, square->b));

    SDL_UpdateWindowSurface(p->window);
    if (!p->vsync) {
        SDL_Delay(1); /* provisoire : évite de tourner à 100 % CPU sans vsync */
    }
}
