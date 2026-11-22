#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#include "engine/frame_stats.h"
#include "engine/memory.h"
#include "engine/platform.h"
#include "engine/tweak.h"
#include "engine/ui.h"
#include "test_render_util.h"
#include "utest.h"

/* Interface de debug (M1-12) : Dear ImGui pour de vrai. Le panneau est construit, dessiné dans une
 * texture par le backend SDL_GPU (on lit les pixels), et piloté avec des événements de souris
 * synthétiques : on clique sur les curseurs et on regarde la variable du programme changer. Sans
 * GPU ces tests s'arrêtent là (OW_REQUIRE_GPU=1 : échec). Sans OW_UI, seule la version « sans effet
 * » est vérifiée. */

#ifndef OW_UI

UTEST(eng_ui_disabled, everything_is_a_harmless_no_op)
{
    EngUi ui;
    EXPECT_FALSE(eng_ui_available());
    EXPECT_FALSE(eng_ui_init(&ui, NULL, NULL, 0));
    EXPECT_FALSE(ui.ready);
    eng_ui_set_visible(&ui, true);
    EXPECT_FALSE(eng_ui_begin_frame(&ui, 64, 64, 0.016F));
    EXPECT_FALSE(eng_ui_overlay_prepare(&ui, NULL));
    EXPECT_FALSE(eng_ui_event_hook(&ui, NULL));
    eng_ui_shutdown(&ui);
}

#else

#define RGBA8 ((uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM)
#define SIZE 512U
#define MIB ((size_t)1024 * 1024)

typedef struct UiTest {
    RenderFixture f;
    EngUi ui;
    EngTweaks tweaks;
    EngFrameStats frames;
    float speed;   /* 0..10, initial 1 */
    int32_t count; /* 1..8, initial 3 */
    bool flag;     /* initial false */
} UiTest;

static bool open_ui_test(UiTest *t, bool with_window)
{
    memset(t, 0, sizeof *t);
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_ui_init(&t->ui, with_window ? t->f.platform.window : NULL, t->f.gpu.device, RGBA8)) {
        close_render_fixture(&t->f);
        return false;
    }
    eng_ui_set_visible(&t->ui, true);
    t->speed = 1.0F;
    t->count = 3;
    t->flag = false;
    eng_tweaks_init(&t->tweaks);
    (void)eng_tweaks_add_float(&t->tweaks, "vitesse", &t->speed, 0.0F, 10.0F);
    (void)eng_tweaks_add_int(&t->tweaks, "nombre", &t->count, 1, 8);
    (void)eng_tweaks_add_bool(&t->tweaks, "actif", &t->flag);
    eng_frame_stats_init(&t->frames);
    for (int i = 0; i < 30; i++) {
        (void)eng_frame_stats_push(&t->frames, 16.0F + (float)(i % 4), 3.0F + (float)(i % 3));
    }
    return true;
}

static void close_ui_test(UiTest *t)
{
    eng_ui_shutdown(&t->ui);
    close_render_fixture(&t->f);
}

static EngUiPanelData panel_data(UiTest *t, bool full)
{
    EngUiPanelData d;
    memset(&d, 0, sizeof d);
    if (full) {
        d.frames = &t->frames;
        d.tweaks = &t->tweaks;
    }
    d.draws = 12;
    d.pipeline_binds = 3;
    d.uniform_pushes = 12;
    d.arena_count = 3;
    d.arenas[0] = (EngUiArena){"permanent", MIB, 2 * MIB, 64 * MIB};
    d.arenas[1] = (EngUiArena){"level", 0, 0, 64 * MIB};
    d.arenas[2] = (EngUiArena){"frame", 4096, 8192, 8 * MIB};
    return d;
}

/* Une image d'interface complète (sans dessin GPU). Retourne false si begin_frame refuse. */
static bool ui_frame(UiTest *t, uint32_t width, uint32_t height, bool full)
{
    if (!eng_ui_begin_frame(&t->ui, (int)width, (int)height, 1.0F / 60.0F)) {
        return false;
    }
    EngUiPanelData d = panel_data(t, full);
    eng_ui_draw_panel(&t->ui, &d);
    eng_ui_end_frame(&t->ui);
    return true;
}

static void ui_frames(UiTest *t, int n)
{
    for (int i = 0; i < n; i++) {
        (void)ui_frame(t, SIZE, SIZE, true);
    }
}

/* ── cycle de vie ── */

UTEST(eng_ui, is_available_and_initialises_then_shuts_down_repeatably)
{
    EXPECT_TRUE(eng_ui_available());
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EXPECT_TRUE(t.ui.ready);
    eng_ui_shutdown(&t.ui);
    EXPECT_FALSE(t.ui.ready);
    eng_ui_shutdown(&t.ui); /* deuxième arrêt : sans effet */
    close_render_fixture(&t.f);
}

UTEST(eng_ui, a_second_context_is_refused_while_one_exists)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngUi second;
    EXPECT_FALSE(eng_ui_init(&second, NULL, t.f.gpu.device, RGBA8));
    EXPECT_FALSE(second.ready);
    EXPECT_TRUE(t.ui.ready); /* le premier n'est pas affecté */
    EXPECT_TRUE(ui_frame(&t, SIZE, SIZE, true));
    close_ui_test(&t);
    /* après l'arrêt, un nouveau contexte est possible */
    UiTest again;
    ASSERT_TRUE(open_ui_test(&again, false));
    EXPECT_TRUE(again.ui.ready);
    close_ui_test(&again);
}

UTEST(eng_ui, init_without_a_device_fails_cleanly)
{
    EngUi ui;
    EXPECT_FALSE(eng_ui_init(&ui, NULL, NULL, RGBA8));
    EXPECT_FALSE(ui.ready);
}

UTEST(eng_ui, hidden_does_nothing_and_frames_cannot_nest)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    eng_ui_set_visible(&t.ui, false);
    EXPECT_FALSE(eng_ui_begin_frame(&t.ui, 64, 64, 0.016F));
    EXPECT_FALSE(t.ui.frame_open);
    EXPECT_FALSE(eng_ui_overlay_prepare(&t.ui, NULL)); /* rien à dessiner */
    eng_ui_set_visible(&t.ui, true);
    EXPECT_TRUE(eng_ui_begin_frame(&t.ui, 64, 64, 0.016F));
    EXPECT_FALSE(eng_ui_begin_frame(&t.ui, 64, 64, 0.016F)); /* déjà ouverte */
    eng_ui_end_frame(&t.ui);
    EXPECT_FALSE(t.ui.frame_open);
    eng_ui_end_frame(&t.ui); /* sans image ouverte : sans effet */
    EXPECT_EQ(t.ui.frames, (uint64_t)1);
    close_ui_test(&t);
}

UTEST(eng_ui, a_frame_with_no_widget_has_nothing_to_draw)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ASSERT_TRUE(eng_ui_begin_frame(&t.ui, 64, 64, 0.016F));
    eng_ui_end_frame(&t.ui); /* aucun panneau : pas de sommet */
    EXPECT_EQ(t.ui.vertices, 0U);
    EXPECT_FALSE(t.ui.draw_pending);
    EXPECT_FALSE(eng_ui_overlay_prepare(&t.ui, NULL)); /* la passe overlay ne serait pas ouverte */
    close_ui_test(&t);
}

UTEST(eng_ui, shutdown_in_the_middle_of_a_frame_is_safe)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EXPECT_TRUE(eng_ui_begin_frame(&t.ui, 64, 64, 0.016F));
    close_ui_test(&t); /* ImGui refuse normalement DestroyContext pendant une image */
}

UTEST(eng_ui, the_panel_draws_more_geometry_when_sections_are_provided)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ui_frames(&t, 3);
    (void)ui_frame(&t, SIZE, SIZE, false); /* sans durées ni réglages */
    const uint32_t minimal = t.ui.vertices;
    ui_frames(&t, 3);
    (void)ui_frame(&t, SIZE, SIZE, true);
    const uint32_t full = t.ui.vertices;
    EXPECT_GT(minimal, 100U);        /* le panneau existe : fenêtre, titre, rendu, mémoire */
    EXPECT_GT(full, minimal + 500U); /* graphiques, curseurs et leurs libellés */
    EXPECT_TRUE(t.ui.draw_pending);
    close_ui_test(&t);
}

/* ── rendu réel dans une texture ── */

static void ui_prepare_cb(SDL_GPUCommandBuffer *commands, void *context)
{
    (void)eng_ui_overlay_prepare(&((UiTest *)context)->ui, commands);
}

static void ui_draw_cb(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    eng_ui_overlay_draw(&((UiTest *)context)->ui, commands, pass);
}

/* Aide au développement : OW_UI_DUMP=chemin.ppm enregistre l'image du panneau (RGBA8 -> PPM), pour
 * LA REGARDER (ex. : `OW_UI_DUMP=/tmp/panneau.ppm ctest -R engine.ui`). Sans la variable : rien. */
static void dump_if_requested(const uint8_t *rgba)
{
    const char *path = getenv("OW_UI_DUMP");
    if (path == NULL || path[0] == '\0') {
        return;
    }
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    if (io == NULL) {
        return;
    }
    char header[32];
    const int n = SDL_snprintf(header, sizeof header, "P6\n%u %u\n255\n", SIZE, SIZE);
    (void)SDL_WriteIO(io, header, (size_t)n);
    for (size_t i = 0; i < (size_t)SIZE * SIZE; i++) {
        (void)SDL_WriteIO(io, rgba + i * 4, 3);
    }
    (void)SDL_CloseIO(io);
}

UTEST(eng_ui, the_panel_is_drawn_over_the_scene_without_erasing_it)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ui_frames(&t, 3);
    ASSERT_TRUE(ui_frame(&t, SIZE, SIZE, true));
    static uint8_t rgba[SIZE * SIZE * 4];
    const float background[4] = {0.2F, 0.4F, 0.6F, 1.0F}; /* (51, 102, 153) */
    ASSERT_TRUE(
        render_to_pixels_overlay(&t.f, background, ui_prepare_cb, ui_draw_cb, &t, SIZE, rgba));
    dump_if_requested(rgba);
    /* hors du panneau (fenêtre : x de 12 à 392) : la scène est conservée, ni effacée ni recouverte
     */
    EXPECT_TRUE(pixel_is(rgba, SIZE, 500, 500, 51, 102, 153, 1));
    EXPECT_TRUE(pixel_is(rgba, SIZE, 5, 5, 51, 102, 153, 1));
    EXPECT_TRUE(pixel_is(rgba, SIZE, 450, 200, 51, 102, 153, 1));
    /* dans le panneau : un fond sombre et neutre (mesuré : 46, 46, 46), nettement différent de la
     * scène bleue (51, 102, 153) : le panneau est bien dessiné PAR-DESSUS elle */
    const uint8_t *inside = rgba + ((size_t)100 * SIZE + 385) * 4;
    EXPECT_LT(inside[0], 90);
    EXPECT_LT(inside[1], 90);
    EXPECT_LT(inside[2], 90);
    EXPECT_EQ(inside[3], 255);
    /* la barre de titre a la couleur de ImGuiCol_TitleBgActive (0,16 ; 0,29 ; 0,48) */
    EXPECT_TRUE(pixel_is(rgba, SIZE, 380, 24, 41, 74, 122, 3));
    /* des pixels d'écriture (texte) existent dans le panneau : il ne s'agit pas d'un aplat */
    uint32_t distinct = 0;
    for (uint32_t y = 40; y < 200; y++) {
        for (uint32_t x = 20; x < 380; x++) {
            const uint8_t *p = rgba + ((size_t)y * SIZE + x) * 4;
            if (p[0] > 120 && p[1] > 120 && p[2] > 120) { /* texte clair du panneau */
                distinct++;
            }
        }
    }
    EXPECT_GT(distinct, 200U);
    close_ui_test(&t);
}

UTEST(eng_ui, a_frame_is_drawn_once_then_must_be_rebuilt)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ui_frames(&t, 2);
    EXPECT_TRUE(t.ui.draw_pending);
    static uint8_t rgba[SIZE * SIZE * 4];
    const float background[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    ASSERT_TRUE(
        render_to_pixels_overlay(&t.f, background, ui_prepare_cb, ui_draw_cb, &t, SIZE, rgba));
    EXPECT_FALSE(t.ui.draw_pending); /* dessinée */
    /* sans nouvelle image d'interface, plus rien à dessiner : l'overlay n'est pas redemandé */
    EXPECT_FALSE(eng_ui_overlay_prepare(&t.ui, NULL));
    close_ui_test(&t);
}

UTEST(eng_ui, hidden_ui_leaves_the_scene_untouched)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ui_frames(&t, 3);
    eng_ui_set_visible(&t.ui, false);
    EXPECT_FALSE(ui_frame(&t, SIZE, SIZE, true));
    static uint8_t rgba[SIZE * SIZE * 4];
    const float background[4] = {0.2F, 0.4F, 0.6F, 1.0F};
    ASSERT_TRUE(
        render_to_pixels_overlay(&t.f, background, ui_prepare_cb, ui_draw_cb, &t, SIZE, rgba));
    EXPECT_TRUE(pixel_is(rgba, SIZE, 385, 100, 51, 102, 153, 1)); /* là où le panneau était */
    EXPECT_TRUE(pixel_is(rgba, SIZE, 380, 24, 51, 102, 153, 1));
    close_ui_test(&t);
}

/* ── interaction : souris synthétique ── */

static void mouse_move(UiTest *t, float x, float y)
{
    eng_ui_add_mouse_pos(&t->ui, x, y);
    ui_frames(t, 2); /* ImGui étale les événements d'entrée sur plusieurs images */
}

static void mouse_button(UiTest *t, bool down)
{
    eng_ui_add_mouse_button(&t->ui, 0, down);
    ui_frames(t, 2);
}

/* Prépare : trois images pour que la fenêtre prenne sa taille, puis lit le rectangle du widget. */
static bool widget_rect(UiTest *t, uint32_t index, float r[4])
{
    ui_frames(t, 3);
    return eng_ui_tweak_rect(&t->ui, index, r);
}

UTEST(eng_ui, the_panel_reports_one_rectangle_per_tweak_and_the_reset_button)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ui_frames(&t, 3);
    float a[4];
    float b[4];
    float c[4];
    float reset[4];
    ASSERT_TRUE(eng_ui_tweak_rect(&t.ui, 0, a));
    ASSERT_TRUE(eng_ui_tweak_rect(&t.ui, 1, b));
    ASSERT_TRUE(eng_ui_tweak_rect(&t.ui, 2, c));
    EXPECT_FALSE(eng_ui_tweak_rect(&t.ui, 3, a)); /* il n'y en a que trois */
    ASSERT_TRUE(eng_ui_reset_rect(&t.ui, reset));
    EXPECT_LT(a[0], a[2]);
    EXPECT_LT(a[1], a[3]);
    /* empilés de haut en bas, sans chevauchement */
    EXPECT_LE(a[3], b[1]);
    EXPECT_LE(b[3], c[1]);
    EXPECT_LE(c[3], reset[1]);
    /* dans l'écran, dans la fenêtre placée en (12, 12) */
    EXPECT_GE(a[0], 12.0F);
    EXPECT_LT(reset[3], (float)SIZE);
    close_ui_test(&t);
}

UTEST(eng_ui, dragging_a_float_slider_changes_the_game_variable)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    float r[4];
    ASSERT_TRUE(widget_rect(&t, 0, r));
    const float y = (r[1] + r[3]) * 0.5F;
    EXPECT_EQ(t.speed, 1.0F);
    mouse_move(&t, r[0] + 20.0F, y);
    mouse_button(&t, true);
    const float near_left = t.speed;
    EXPECT_LT(near_left, 2.0F); /* tout à gauche du curseur : valeur basse */
    mouse_move(&t, r[0] + 150.0F, y);
    const float middle = t.speed;
    EXPECT_GT(middle, near_left + 1.0F); /* glisser vers la droite augmente */
    EXPECT_LT(middle, 10.0F);
    mouse_move(&t, r[2] + 400.0F, y); /* bien au-delà du bord : la valeur est BORNÉE */
    EXPECT_EQ(t.speed, 10.0F);
    mouse_move(&t, -300.0F, y);
    EXPECT_EQ(t.speed, 0.0F);
    mouse_button(&t, false);
    EXPECT_EQ(t.speed, 0.0F); /* relâcher ne change plus rien */
    float via_registry = -1.0F;
    EXPECT_TRUE(eng_tweaks_get_float(&t.tweaks, "vitesse", &via_registry));
    EXPECT_EQ(via_registry, t.speed);
    close_ui_test(&t);
}

UTEST(eng_ui, dragging_an_integer_slider_gives_integers_within_bounds)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    float r[4];
    ASSERT_TRUE(widget_rect(&t, 1, r));
    const float y = (r[1] + r[3]) * 0.5F;
    mouse_move(&t, r[0] + 20.0F, y);
    mouse_button(&t, true);
    mouse_move(&t, r[2] + 400.0F, y);
    EXPECT_EQ(t.count, 8);
    mouse_move(&t, -300.0F, y);
    EXPECT_EQ(t.count, 1);
    mouse_button(&t, false);
    close_ui_test(&t);
}

UTEST(eng_ui, clicking_the_checkbox_toggles_the_bool_each_time)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    float r[4];
    ASSERT_TRUE(widget_rect(&t, 2, r));
    const float x = r[0] + 8.0F;
    const float y = (r[1] + r[3]) * 0.5F;
    mouse_move(&t, x, y);
    EXPECT_FALSE(t.flag);
    mouse_button(&t, true);
    mouse_button(&t, false);
    EXPECT_TRUE(t.flag);
    mouse_button(&t, true);
    mouse_button(&t, false);
    EXPECT_FALSE(t.flag);
    close_ui_test(&t);
}

UTEST(eng_ui, the_reset_button_restores_every_variable)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    t.speed = 7.0F;
    t.count = 8;
    t.flag = true;
    ui_frames(&t, 3);
    float reset[4];
    ASSERT_TRUE(eng_ui_reset_rect(&t.ui, reset));
    mouse_move(&t, (reset[0] + reset[2]) * 0.5F, (reset[1] + reset[3]) * 0.5F);
    mouse_button(&t, true);
    mouse_button(&t, false);
    EXPECT_EQ(t.speed, 1.0F);
    EXPECT_EQ(t.count, 3);
    EXPECT_FALSE(t.flag);
    close_ui_test(&t);
}

UTEST(eng_ui, a_variable_changed_by_the_program_is_shown_by_the_slider)
{
    /* l'autre sens : la valeur affichée suit la variable (le curseur ne garde pas sa propre copie)
     */
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    t.speed = 9.0F;
    ui_frames(&t, 3);
    float r[4];
    ASSERT_TRUE(eng_ui_tweak_rect(&t.ui, 0, r));
    const float y = (r[1] + r[3]) * 0.5F;
    /* un simple clic sur le curseur sans bouger ne doit pas ramener la valeur à celle d'une copie
     * périmée : on clique au tout début (valeur ~0) puis on vérifie qu'on est bien repartis de là
     */
    mouse_move(&t, r[0] + 5.0F, y);
    mouse_button(&t, true);
    EXPECT_LT(t.speed, 2.0F);
    mouse_button(&t, false);
    close_ui_test(&t);
}

UTEST(eng_ui, the_pointer_is_wanted_over_the_panel_and_not_elsewhere)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ui_frames(&t, 3);
    mouse_move(&t, 100.0F, 20.0F); /* barre de titre */
    EXPECT_TRUE(eng_ui_wants_mouse(&t.ui));
    mouse_move(&t, 480.0F, 480.0F); /* hors du panneau */
    EXPECT_FALSE(eng_ui_wants_mouse(&t.ui));
    mouse_move(&t, 100.0F, 20.0F);
    eng_ui_set_visible(&t.ui, false); /* masquée : ne réclame rien */
    EXPECT_FALSE(eng_ui_wants_mouse(&t.ui));
    EXPECT_FALSE(eng_ui_wants_keyboard(&t.ui));
    close_ui_test(&t);
}

/* ── événements SDL réels, fenêtre réelle (pilote offscreen) ── */

static void push_mouse_motion(UiTest *t, float x, float y)
{
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_MOUSE_MOTION;
    ev.motion.windowID = SDL_GetWindowID(t->f.platform.window);
    ev.motion.x = x;
    ev.motion.y = y;
    SDL_PushEvent(&ev);
}

static void push_mouse_button(UiTest *t, bool down, float x, float y)
{
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    ev.button.windowID = SDL_GetWindowID(t->f.platform.window);
    ev.button.button = SDL_BUTTON_LEFT;
    ev.button.down = down;
    ev.button.x = x;
    ev.button.y = y;
    SDL_PushEvent(&ev);
}

static void push_key(SDL_Scancode code, bool down)
{
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    ev.key.scancode = code;
    ev.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false);
    ev.key.down = down;
    SDL_PushEvent(&ev);
}

UTEST(eng_ui, the_event_hook_keeps_clicks_on_the_panel_away_from_the_game)
{
    UiTest t;
    if (!open_ui_test(&t, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngPlatform *p = &t.f.platform;
    p->event_hook = eng_ui_event_hook;
    p->event_hook_user = &t.ui;
    (void)eng_platform_pump(p);
    uint32_t w = 64;
    uint32_t h = 64;
    ui_frames(&t, 1);
    (void)w;
    (void)h;
    for (int i = 0; i < 3; i++) {
        (void)ui_frame(&t, 64, 64, true);
    }
    /* clic sur le panneau (la fenêtre de 64 x 64 px est couverte à partir de (12, 12)) */
    push_mouse_motion(&t, 30.0F, 30.0F);
    (void)eng_platform_pump(p);
    for (int i = 0; i < 3; i++) {
        (void)ui_frame(&t, 64, 64, true);
    }
    ASSERT_TRUE(eng_ui_wants_mouse(&t.ui));
    push_mouse_button(&t, true, 30.0F, 30.0F);
    (void)eng_platform_pump(p);
    EXPECT_FALSE(p->input.mouse_down[SDL_BUTTON_LEFT]); /* consommé par l'interface */
    push_mouse_button(&t, false, 30.0F, 30.0F);
    (void)eng_platform_pump(p);
    EXPECT_FALSE(p->input.mouse_down[SDL_BUTTON_LEFT]);
    /* clic en dehors du panneau : le jeu le reçoit */
    push_mouse_motion(&t, 4.0F, 4.0F);
    (void)eng_platform_pump(p);
    for (int i = 0; i < 3; i++) {
        (void)ui_frame(&t, 64, 64, true);
    }
    EXPECT_FALSE(eng_ui_wants_mouse(&t.ui));
    push_mouse_button(&t, true, 4.0F, 4.0F);
    (void)eng_platform_pump(p);
    EXPECT_TRUE(p->input.mouse_down[SDL_BUTTON_LEFT]);
    push_mouse_button(&t, false, 4.0F, 4.0F);
    (void)eng_platform_pump(p);
    EXPECT_FALSE(p->input.mouse_down[SDL_BUTTON_LEFT]);
    p->event_hook = NULL;
    close_ui_test(&t);
}

UTEST(eng_ui, the_event_hook_lets_the_keyboard_through_when_no_text_field_is_active)
{
    UiTest t;
    if (!open_ui_test(&t, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngPlatform *p = &t.f.platform;
    p->event_hook = eng_ui_event_hook;
    p->event_hook_user = &t.ui;
    for (int i = 0; i < 3; i++) {
        (void)ui_frame(&t, 64, 64, true);
    }
    push_key(SDL_SCANCODE_W, true);
    (void)eng_platform_pump(p);
    EXPECT_TRUE(p->input.key_down[SDL_SCANCODE_W]);
    push_key(SDL_SCANCODE_W, false);
    (void)eng_platform_pump(p);
    EXPECT_FALSE(p->input.key_down[SDL_SCANCODE_W]);
    p->event_hook = NULL;
    close_ui_test(&t);
}

UTEST(eng_ui, a_hidden_ui_never_consumes_events)
{
    UiTest t;
    if (!open_ui_test(&t, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngPlatform *p = &t.f.platform;
    p->event_hook = eng_ui_event_hook;
    p->event_hook_user = &t.ui;
    for (int i = 0; i < 3; i++) {
        (void)ui_frame(&t, 64, 64, true);
    }
    push_mouse_motion(&t, 30.0F, 30.0F);
    (void)eng_platform_pump(p);
    for (int i = 0; i < 3; i++) {
        (void)ui_frame(&t, 64, 64, true);
    }
    eng_ui_set_visible(&t.ui, false); /* masquée alors que le pointeur est sur le panneau */
    push_mouse_button(&t, true, 30.0F, 30.0F);
    (void)eng_platform_pump(p);
    EXPECT_TRUE(p->input.mouse_down[SDL_BUTTON_LEFT]); /* le jeu reçoit le clic */
    push_mouse_button(&t, false, 30.0F, 30.0F);
    (void)eng_platform_pump(p);
    p->event_hook = NULL;
    close_ui_test(&t);
}

/* ── mémoire ── */

UTEST(eng_ui, a_steady_state_ui_frame_allocates_nothing)
{
    UiTest t;
    if (!open_ui_test(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    eng_mem_track_install();
    ui_frames(&t, 10); /* échauffement : tampons d'ImGui, fenêtre */
    const uint32_t before = eng_mem_alloc_count();
    ui_frames(&t, 20);
    const uint32_t during = eng_mem_alloc_count() - before;
    EXPECT_EQ(during, 0U); /* construction du panneau : aucune allocation une fois chaud */
    close_ui_test(&t);
}

#endif /* OW_UI */
