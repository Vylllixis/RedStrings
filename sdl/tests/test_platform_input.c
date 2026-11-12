#include <SDL3/SDL.h>

#include "engine/platform.h"
#include "utest.h"

/* Traduction des événements SDL en actions, de bout en bout : on injecte de vrais événements SDL
 * dans la file (pilote vidéo factice, sans écran) et on les lit à travers eng_platform_pump. */

static bool open_platform(EngPlatform *p)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    EngWindowDesc desc = {.title = "test", .width = 64, .height = 64};
    return eng_platform_init(p, &desc);
}

static void push_key(SDL_Scancode code, bool down, bool repeat)
{
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    ev.key.scancode = code;
    ev.key.down = down;
    ev.key.repeat = repeat;
    SDL_PushEvent(&ev);
}

static void push_pad_button(SDL_GamepadButton button, bool down)
{
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
    ev.gbutton.button = (Uint8)button;
    ev.gbutton.down = down;
    SDL_PushEvent(&ev);
}

static void push_pad_axis(SDL_GamepadAxis axis, Sint16 value)
{
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    ev.gaxis.axis = (Uint8)axis;
    ev.gaxis.value = value;
    SDL_PushEvent(&ev);
}

static EngInputState tick(EngPlatform *p)
{
    EngInputState s;
    eng_input_begin_tick(&p->input, &s);
    return s;
}

UTEST(eng_platform_input, keyboard_events_become_actions)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    push_key(SDL_SCANCODE_SPACE, true, false);
    push_key(SDL_SCANCODE_W, true, false);
    EXPECT_TRUE(eng_platform_pump(&p));
    EngInputState s = tick(&p);
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_JUMP));
    EXPECT_NEAR(s.axis[ENG_AXIS_MOVE].y, 1.0, 1e-6);
    eng_platform_shutdown(&p);
}

UTEST(eng_platform_input, gamepad_events_become_the_same_actions)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    push_pad_button(SDL_GAMEPAD_BUTTON_SOUTH, true);
    push_pad_axis(SDL_GAMEPAD_AXIS_LEFTY, -32768);
    EXPECT_TRUE(eng_platform_pump(&p));
    EngInputState s = tick(&p);
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_JUMP));
    EXPECT_NEAR(s.axis[ENG_AXIS_MOVE].y, 1.0, 1e-6);
    eng_platform_shutdown(&p);
}

UTEST(eng_platform_input, key_repeat_is_not_a_new_press)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    push_key(SDL_SCANCODE_E, true, false);
    (void)eng_platform_pump(&p);
    (void)tick(&p);
    push_key(SDL_SCANCODE_E, true, true); /* répétition automatique de l'OS */
    push_key(SDL_SCANCODE_E, true, true);
    (void)eng_platform_pump(&p);
    EngInputState s = tick(&p);
    EXPECT_FALSE(eng_action_pressed(&s, ENG_ACTION_INTERACT));
    EXPECT_TRUE(eng_action_held(&s, ENG_ACTION_INTERACT));
    eng_platform_shutdown(&p);
}

UTEST(eng_platform_input, press_and_release_in_the_same_frame_is_kept)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    push_key(SDL_SCANCODE_LCTRL, true, false);
    push_key(SDL_SCANCODE_LCTRL, false, false);
    (void)eng_platform_pump(&p);
    EngInputState s = tick(&p);
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_DODGE));
    EXPECT_TRUE(eng_action_released(&s, ENG_ACTION_DODGE));
    EXPECT_FALSE(eng_action_held(&s, ENG_ACTION_DODGE));
    eng_platform_shutdown(&p);
}

UTEST(eng_platform_input, losing_window_focus_releases_keys)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    push_key(SDL_SCANCODE_LSHIFT, true, false);
    (void)eng_platform_pump(&p);
    (void)tick(&p);
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    SDL_PushEvent(&ev);
    (void)eng_platform_pump(&p);
    EngInputState s = tick(&p);
    EXPECT_FALSE(eng_action_held(&s, ENG_ACTION_SPRINT));
    EXPECT_TRUE(eng_action_released(&s, ENG_ACTION_SPRINT));
    eng_platform_shutdown(&p);
}

UTEST(eng_platform_input, escape_still_quits_as_a_system_key)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_KEY_DOWN;
    ev.key.scancode = SDL_SCANCODE_ESCAPE;
    ev.key.key = SDLK_ESCAPE;
    ev.key.down = true;
    SDL_PushEvent(&ev);
    EXPECT_FALSE(eng_platform_pump(&p));
    eng_platform_shutdown(&p);
}

static void push_wheel(float y)
{
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_MOUSE_WHEEL;
    ev.wheel.y = y;
    SDL_PushEvent(&ev);
}

static void push_system_key(SDL_Keycode key, SDL_Scancode code)
{
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_KEY_DOWN;
    ev.key.scancode = code;
    ev.key.key = key;
    ev.key.down = true;
    SDL_PushEvent(&ev);
}

UTEST(eng_platform_input, mouse_wheel_events_accumulate_until_the_next_tick)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    push_wheel(1.0F);
    push_wheel(2.0F);
    push_wheel(-1.0F);
    (void)eng_platform_pump(&p);
    EngInputState s = tick(&p);
    EXPECT_NEAR(s.wheel, 2.0F, 1e-6F);
    s = tick(&p);
    EXPECT_NEAR(s.wheel, 0.0F, 1e-6F);
    eng_platform_shutdown(&p);
}

UTEST(eng_platform_input, f2_toggles_mouse_capture_when_sdl_allows_it)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    EXPECT_FALSE(p.mouse_captured);
    const bool can_capture = eng_platform_set_mouse_capture(&p, true);
    /* l'état suit exactement la réponse : jamais « capturée » après un refus de SDL */
    EXPECT_EQ(p.mouse_captured, can_capture);
    EXPECT_EQ(SDL_GetWindowRelativeMouseMode(p.window), can_capture);
    if (can_capture) {
        push_system_key(SDLK_F2, SDL_SCANCODE_F2);
        (void)eng_platform_pump(&p);
        EXPECT_FALSE(p.mouse_captured);
        EXPECT_FALSE(SDL_GetWindowRelativeMouseMode(p.window));
        push_system_key(SDLK_F2, SDL_SCANCODE_F2);
        (void)eng_platform_pump(&p);
        EXPECT_TRUE(p.mouse_captured);
        EXPECT_TRUE(SDL_GetWindowRelativeMouseMode(p.window));
    }
    eng_platform_shutdown(&p);
}

UTEST(eng_platform_input, f3_toggles_the_debug_draw_and_it_starts_on)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    EXPECT_TRUE(p.debug_draw);
    push_system_key(SDLK_F3, SDL_SCANCODE_F3);
    (void)eng_platform_pump(&p);
    EXPECT_FALSE(p.debug_draw);
    push_system_key(SDLK_F3, SDL_SCANCODE_F3);
    (void)eng_platform_pump(&p);
    EXPECT_TRUE(p.debug_draw);
    /* F3 n'agit ni sur la capture de la souris ni sur le plein écran */
    EXPECT_FALSE(p.mouse_captured);
    EXPECT_FALSE(p.fullscreen);
    eng_platform_shutdown(&p);
}

UTEST(eng_platform_input, f1_toggles_the_debug_ui_and_it_starts_hidden)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    EXPECT_FALSE(p.debug_ui);
    push_system_key(SDLK_F1, SDL_SCANCODE_F1);
    (void)eng_platform_pump(&p);
    EXPECT_TRUE(p.debug_ui);
    push_system_key(SDLK_F1, SDL_SCANCODE_F1);
    (void)eng_platform_pump(&p);
    EXPECT_FALSE(p.debug_ui);
    EXPECT_TRUE(p.debug_draw); /* F1 ne touche pas à F3 */
    EXPECT_FALSE(p.mouse_captured);
    EXPECT_FALSE(p.fullscreen);
    eng_platform_shutdown(&p);
}

typedef struct HookProbe {
    uint32_t calls;
    bool consume_downs;
} HookProbe;

static bool probe_hook(void *user, const void *sdl_event)
{
    HookProbe *h = (HookProbe *)user;
    const SDL_Event *ev = (const SDL_Event *)sdl_event;
    h->calls++;
    return h->consume_downs &&
           (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP ||
            ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP);
}

UTEST(eng_platform_input, an_event_hook_sees_every_event_and_can_consume_them)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    HookProbe probe = {.calls = 0, .consume_downs = true};
    p.event_hook = probe_hook;
    p.event_hook_user = &probe;
    push_key(SDL_SCANCODE_W, true, false);
    push_key(SDL_SCANCODE_A, true, false);
    (void)eng_platform_pump(&p);
    EXPECT_GE(probe.calls, 2U);
    EXPECT_FALSE(p.input.key_down[SDL_SCANCODE_W]); /* consommé : le jeu ne le voit pas */
    EXPECT_FALSE(p.input.key_down[SDL_SCANCODE_A]);
    probe.consume_downs = false;
    push_key(SDL_SCANCODE_W, true, false);
    (void)eng_platform_pump(&p);
    EXPECT_TRUE(p.input.key_down[SDL_SCANCODE_W]); /* le filtre laisse passer : le jeu reçoit */
    eng_platform_shutdown(&p);
}

UTEST(eng_platform_input, a_consumed_release_still_reaches_the_game_so_nothing_sticks)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    push_key(SDL_SCANCODE_W, true, false); /* pressée avant l'interface */
    (void)eng_platform_pump(&p);
    ASSERT_TRUE(p.input.key_down[SDL_SCANCODE_W]);
    HookProbe probe = {.calls = 0, .consume_downs = true};
    p.event_hook = probe_hook;
    p.event_hook_user = &probe;
    push_key(SDL_SCANCODE_W, false, false); /* relâchée pendant que l'interface consomme */
    (void)eng_platform_pump(&p);
    EXPECT_FALSE(p.input.key_down[SDL_SCANCODE_W]);
    /* idem pour un bouton de souris */
    p.event_hook = NULL;
    eng_input_mouse_button(&p.input, SDL_BUTTON_LEFT, true);
    ASSERT_TRUE(p.input.mouse_down[SDL_BUTTON_LEFT]);
    p.event_hook = probe_hook;
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_MOUSE_BUTTON_UP;
    ev.button.button = SDL_BUTTON_LEFT;
    SDL_PushEvent(&ev);
    (void)eng_platform_pump(&p);
    EXPECT_FALSE(p.input.mouse_down[SDL_BUTTON_LEFT]);
    eng_platform_shutdown(&p);
}

UTEST(eng_platform_input, a_consumed_escape_does_not_quit_and_system_keys_work_without_a_hook)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    HookProbe probe = {.calls = 0, .consume_downs = true};
    p.event_hook = probe_hook;
    p.event_hook_user = &probe;
    push_system_key(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    EXPECT_TRUE(eng_platform_pump(&p)); /* l'interface avait la touche (champ de saisie) */
    p.event_hook = NULL;
    push_system_key(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    EXPECT_FALSE(eng_platform_pump(&p));
    eng_platform_shutdown(&p);
}

UTEST(eng_platform_input, losing_focus_releases_the_mouse)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    if (!eng_platform_set_mouse_capture(&p, true)) {
        eng_platform_shutdown(&p);
        return; /* pilote sans mode relatif : rien à vérifier */
    }
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    SDL_PushEvent(&ev);
    (void)eng_platform_pump(&p);
    EXPECT_FALSE(p.mouse_captured);
    EXPECT_FALSE(SDL_GetWindowRelativeMouseMode(p.window));
    eng_platform_shutdown(&p);
}

UTEST(eng_platform_input, mouse_capture_is_idempotent_and_safe_without_a_window)
{
    EngPlatform p;
    ASSERT_TRUE(open_platform(&p));
    const bool first = eng_platform_set_mouse_capture(&p, false); /* déjà libre : rien à faire */
    EXPECT_TRUE(first);
    EXPECT_FALSE(p.mouse_captured);
    eng_platform_shutdown(&p);
    EngPlatform none = {0};
    EXPECT_FALSE(eng_platform_set_mouse_capture(&none, true));
    EXPECT_FALSE(none.mouse_captured);
}

UTEST(eng_platform_input, refused_capture_leaves_the_state_unchanged)
{
    /* Le pilote « offscreen » n'a pas de mode souris relatif : SDL refuse. Si un pilote
     * l'acceptait, l'état doit simplement suivre la réponse. */
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    /* sans surface logicielle : celle du pilote offscreen fuit dans SDL (hors de notre code) */
    EngWindowDesc desc = {.title = "test", .width = 64, .height = 64, .no_software_surface = true};
    EngPlatform p;
    ASSERT_TRUE(eng_platform_init(&p, &desc));
    const bool accepted = eng_platform_set_mouse_capture(&p, true);
    EXPECT_EQ(p.mouse_captured, accepted);
    EXPECT_EQ(SDL_GetWindowRelativeMouseMode(p.window), accepted);
    eng_platform_shutdown(&p);
}
