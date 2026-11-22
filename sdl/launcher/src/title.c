/* Écran-titre : la scène de l'écran de chargement derrière le menu. Voir title.h. */
#include "title.h"

#include <string.h>

#include "splash_internal.h"

_Static_assert(sizeof(SplashUniforms) == sizeof(((Title *)0)->uniforms),
               "Title.uniforms doit contenir un SplashUniforms");

void title_init(Title *t, const Splash *scene, EngUi *ui)
{
    memset(t, 0, sizeof *t);
    t->ui = ui;
    if (scene != NULL && scene->ready && scene->pipeline != NULL) {
        t->scene = scene;
        t->time_s = scene->time_s;
    }
}

bool title_ready(const Title *t)
{
    return t != NULL && t->scene != NULL;
}

void title_frame(Title *t, const GameFlow *flow, float dt, uint32_t width, uint32_t height)
{
    if (t == NULL) {
        return;
    }
    t->active = false;
    if (t->scene == NULL || flow == NULL || width == 0U || height == 0U) {
        return;
    }
    t->time_s += dt > 0.0F ? (dt > 0.1F ? 0.1 : (double)dt) : 0.0;
    if (!game_flow_in_title(flow)) {
        return;
    }
    const EngMenuMotion *a = &flow->motion;
    SplashUniforms u;
    splash_fill_uniforms(t->scene, &u, t->time_s, 1.0F, game_flow_title_outro(flow), width, height);
    u.menu[0] = a->title_menu;
    u.menu[1] = a->dim.x;
    u.view[0] = a->cam_x.x;
    u.view[1] = a->cam_y.x;
    u.view[2] = a->cam_zoom.x;
    u.title[0] = 0.0F;
    u.title[1] = a->title_y.x;
    u.title[2] = a->title_w.x;
    u.title[3] = 1.0F;
    memcpy(t->uniforms, &u, sizeof u);
    t->active = true;
}

bool title_overlay_prepare(void *user, struct SDL_GPUCommandBuffer *commands)
{
    const Title *t = user;
    const bool ui = t->ui != NULL && eng_ui_overlay_prepare(t->ui, commands);
    return ui || t->active;
}

void title_overlay_draw(void *user, struct SDL_GPUCommandBuffer *commands,
                        struct SDL_GPURenderPass *pass)
{
    Title *t = user;
    if (t->active) {
        SplashUniforms u;
        memcpy(&u, t->uniforms, sizeof u);
        splash_draw_pass(t->scene, commands, pass, &u);
        t->frames++;
    }
    if (t->ui != NULL) {
        eng_ui_overlay_draw(t->ui, commands, pass);
    }
}
