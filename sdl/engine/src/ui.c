#include "engine/ui.h"

#include <float.h>
#include <stdio.h>
#include <string.h>

#include "engine/log.h"

#ifdef OW_UI

#include "ui_backend.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

bool eng_ui_available(void)
{
    return true;
}

bool eng_ui_init(EngUi *ui, struct SDL_Window *window, struct SDL_GPUDevice *device,
                 uint32_t color_format)
{
    memset(ui, 0, sizeof *ui);
    if (!ow_ui_create(window, device, color_format)) {
        return false;
    }
    ui->ready = true;
    return true;
}

void eng_ui_shutdown(EngUi *ui)
{
    if (!ui->ready) {
        return;
    }
    ow_ui_destroy();
    memset(ui, 0, sizeof *ui);
}

void eng_ui_set_visible(EngUi *ui, bool visible)
{
    ui->visible = visible;
    if (!visible) {
        ui->draw_pending =
            false; /* une image construite avant le masquage ne doit plus apparaître */
    }
}

bool eng_ui_begin_frame(EngUi *ui, int width, int height, float dt)
{
    if (!ui->ready || !ui->visible || ui->frame_open) {
        return false;
    }
    ui->draw_pending = false;
    ui->tweak_rect_count = 0;
    ui->has_reset_rect = false;
    ow_ui_new_frame(width, height, dt);
    ui->frame_open = true;
    return true;
}

static const ImVec2_c ZERO2 = {0.0F, 0.0F};

static void draw_frame_section(const EngFrameStats *frames)
{
    const EngFrameSummary s = eng_frame_stats_summary(frames);
    igText("%.1f FPS  (image %.2f ms, pire %.2f ms)", (double)s.fps, (double)s.avg_frame_ms,
           (double)s.max_frame_ms);
    igText("CPU %.2f ms  (pire %.2f ms)", (double)s.avg_cpu_ms, (double)s.max_cpu_ms);
    float history[ENG_FRAME_STATS_HISTORY];
    const ImVec2_c graph = {0.0F, 48.0F};
    uint32_t n = eng_frame_stats_copy_frame(frames, history, ENG_FRAME_STATS_HISTORY);
    igPlotLines_FloatPtr("##frame_ms", history, (int)n, 0, "image (ms)", 0.0F, FLT_MAX, graph,
                         (int)sizeof(float));
    n = eng_frame_stats_copy_cpu(frames, history, ENG_FRAME_STATS_HISTORY);
    igPlotLines_FloatPtr("##cpu_ms", history, (int)n, 0, "CPU (ms)", 0.0F, FLT_MAX, graph,
                         (int)sizeof(float));
}

static void draw_render_section(const EngUiPanelData *d)
{
    igText("dessins : %u    pipelines : %u    uniforms : %u", (unsigned)d->draws,
           (unsigned)d->pipeline_binds, (unsigned)d->uniform_pushes);
    igText("rejetés : %llu    perdus : %llu", (unsigned long long)d->rejected_commands,
           (unsigned long long)d->dropped_commands);
}

static void draw_memory_section(const EngUiPanelData *d)
{
    for (uint32_t i = 0; i < d->arena_count && i < ENG_UI_MAX_ARENAS; i++) {
        const EngUiArena *a = &d->arenas[i];
        const float fraction =
            a->capacity > 0 ? (float)((double)a->used / (double)a->capacity) : 0.0F;
        char label[96];
        (void)snprintf(label, sizeof label, "%s : %.2f / %.0f Mo (pic %.2f)", a->name,
                       (double)a->used / 1048576.0, (double)a->capacity / 1048576.0,
                       (double)a->peak / 1048576.0);
        const ImVec2_c size = {-1.0F, 0.0F}; /* toute la largeur */
        igProgressBar(fraction, size, label);
    }
    igText("allocations pendant les images : %llu", (unsigned long long)d->frame_allocs);
}

static void store_rect(float out[4])
{
    const ImVec2_c lo = igGetItemRectMin();
    const ImVec2_c hi = igGetItemRectMax();
    out[0] = lo.x;
    out[1] = lo.y;
    out[2] = hi.x;
    out[3] = hi.y;
}

static void draw_tweaks_section(EngUi *ui, EngTweaks *t)
{
    for (uint32_t i = 0; i < t->count; i++) {
        const EngTweak *e = &t->items[i];
        switch (e->type) {
        case ENG_TWEAK_FLOAT: {
            float v = *(const float *)e->value;
            if (igSliderFloat(e->name, &v, e->min, e->max, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
                (void)eng_tweaks_set_float(t, e->name, v);
            }
            break;
        }
        case ENG_TWEAK_INT: {
            int v = (int)*(const int32_t *)e->value;
            if (igSliderInt(e->name, &v, (int)e->min, (int)e->max, "%d",
                            ImGuiSliderFlags_AlwaysClamp)) {
                (void)eng_tweaks_set_int(t, e->name, (int32_t)v);
            }
            break;
        }
        case ENG_TWEAK_BOOL: {
            bool v = *(const bool *)e->value;
            if (igCheckbox(e->name, &v)) {
                (void)eng_tweaks_set_bool(t, e->name, v);
            }
            break;
        }
        }
        store_rect(ui->tweak_rects[i]);
        ui->tweak_rect_count = i + 1U;
    }
    if (igButton("Tout réinitialiser", ZERO2)) {
        eng_tweaks_reset_all(t);
    }
    store_rect(ui->reset_rect);
    ui->has_reset_rect = true;
}

void eng_ui_draw_panel(EngUi *ui, const EngUiPanelData *data)
{
    if (!ui->frame_open) {
        return;
    }
    igSetNextWindowPos((ImVec2_c){12.0F, 12.0F}, ImGuiCond_FirstUseEver, ZERO2);
    igSetNextWindowSize((ImVec2_c){380.0F, 0.0F}, ImGuiCond_FirstUseEver);
    if (igBegin("Debug", NULL, ImGuiWindowFlags_None)) {
        if (data->frames != NULL &&
            igCollapsingHeader_TreeNodeFlags("Images", ImGuiTreeNodeFlags_DefaultOpen)) {
            draw_frame_section(data->frames);
        }
        if (igCollapsingHeader_TreeNodeFlags("Rendu", ImGuiTreeNodeFlags_DefaultOpen)) {
            draw_render_section(data);
        }
        if (igCollapsingHeader_TreeNodeFlags("Mémoire", ImGuiTreeNodeFlags_DefaultOpen)) {
            draw_memory_section(data);
        }
        if (data->tweaks != NULL &&
            igCollapsingHeader_TreeNodeFlags("Réglages", ImGuiTreeNodeFlags_DefaultOpen)) {
            draw_tweaks_section(ui, data->tweaks);
        }
    }
    igEnd();
}

void eng_ui_end_frame(EngUi *ui)
{
    if (!ui->frame_open) {
        return;
    }
    ui->vertices = ow_ui_render();
    ui->frame_open = false;
    ui->draw_pending = ui->vertices > 0;
    ui->frames++;
}

bool eng_ui_overlay_prepare(void *user, struct SDL_GPUCommandBuffer *commands)
{
    EngUi *ui = (EngUi *)user;
    if (!ui->ready || !ui->draw_pending) {
        return false;
    }
    ow_ui_prepare(commands);
    return true;
}

void eng_ui_overlay_draw(void *user, struct SDL_GPUCommandBuffer *commands,
                         struct SDL_GPURenderPass *pass)
{
    EngUi *ui = (EngUi *)user;
    if (!ui->ready || !ui->draw_pending) {
        return;
    }
    ow_ui_draw(commands, pass);
    ui->draw_pending = false; /* dessinée : la prochaine image doit être reconstruite */
}

bool eng_ui_event_hook(void *user, const void *sdl_event)
{
    const EngUi *ui = (const EngUi *)user;
    /* Masquée, l'interface ne reçoit rien : le jeu garde tous ses événements. */
    return ui->ready && ui->visible && ow_ui_process_event(sdl_event);
}

bool eng_ui_tweak_rect(const EngUi *ui, uint32_t index, float out[4])
{
    if (index >= ui->tweak_rect_count) {
        return false;
    }
    memcpy(out, ui->tweak_rects[index], 4 * sizeof(float));
    return true;
}

bool eng_ui_reset_rect(const EngUi *ui, float out[4])
{
    if (!ui->has_reset_rect) {
        return false;
    }
    memcpy(out, ui->reset_rect, 4 * sizeof(float));
    return true;
}

bool eng_ui_wants_mouse(const EngUi *ui)
{
    return ui->ready && ui->visible && ow_ui_wants_mouse();
}

bool eng_ui_wants_keyboard(const EngUi *ui)
{
    return ui->ready && ui->visible && ow_ui_wants_keyboard();
}

void eng_ui_add_mouse_pos(EngUi *ui, float x, float y)
{
    if (ui->ready) {
        ow_ui_add_mouse_pos(x, y);
    }
}

void eng_ui_add_mouse_button(EngUi *ui, int button, bool down)
{
    if (ui->ready) {
        ow_ui_add_mouse_button(button, down);
    }
}

#else /* sans OW_UI : tout est sans effet */

bool eng_ui_available(void)
{
    return false;
}

bool eng_ui_init(EngUi *ui, struct SDL_Window *window, struct SDL_GPUDevice *device,
                 uint32_t color_format)
{
    (void)window;
    (void)device;
    (void)color_format;
    memset(ui, 0, sizeof *ui);
    ENG_LOG_INFO("interface de debug absente de ce build (OW_UI=OFF)");
    return false;
}

void eng_ui_shutdown(EngUi *ui)
{
    memset(ui, 0, sizeof *ui);
}

void eng_ui_set_visible(EngUi *ui, bool visible)
{
    (void)ui;
    (void)visible;
}

bool eng_ui_begin_frame(EngUi *ui, int width, int height, float dt)
{
    (void)ui;
    (void)width;
    (void)height;
    (void)dt;
    return false;
}

void eng_ui_draw_panel(EngUi *ui, const EngUiPanelData *data)
{
    (void)ui;
    (void)data;
}

void eng_ui_end_frame(EngUi *ui)
{
    (void)ui;
}

bool eng_ui_overlay_prepare(void *ui, struct SDL_GPUCommandBuffer *commands)
{
    (void)ui;
    (void)commands;
    return false;
}

void eng_ui_overlay_draw(void *ui, struct SDL_GPUCommandBuffer *commands,
                         struct SDL_GPURenderPass *pass)
{
    (void)ui;
    (void)commands;
    (void)pass;
}

bool eng_ui_event_hook(void *ui, const void *sdl_event)
{
    (void)ui;
    (void)sdl_event;
    return false;
}

bool eng_ui_tweak_rect(const EngUi *ui, uint32_t index, float out[4])
{
    (void)ui;
    (void)index;
    (void)out;
    return false;
}

bool eng_ui_reset_rect(const EngUi *ui, float out[4])
{
    (void)ui;
    (void)out;
    return false;
}

bool eng_ui_wants_mouse(const EngUi *ui)
{
    (void)ui;
    return false;
}

bool eng_ui_wants_keyboard(const EngUi *ui)
{
    (void)ui;
    return false;
}

void eng_ui_add_mouse_pos(EngUi *ui, float x, float y)
{
    (void)ui;
    (void)x;
    (void)y;
}

void eng_ui_add_mouse_button(EngUi *ui, int button, bool down)
{
    (void)ui;
    (void)button;
    (void)down;
}

#endif
