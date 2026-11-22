#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#include "engine/input.h"
#include "utest.h"

static void setup(EngInput *input)
{
    eng_input_set_default_bindings(input);
}

static EngInputState tick(EngInput *input)
{
    EngInputState state;
    eng_input_begin_tick(input, &state);
    return state;
}

#define A ENG_ACTION_ATTACK

UTEST(eng_input, press_hold_release_lifecycle)
{
    EngInput in;
    setup(&in);
    EngInputState s = tick(&in);
    EXPECT_FALSE(eng_action_pressed(&s, A) || eng_action_held(&s, A) || eng_action_released(&s, A));

    eng_input_mouse_button(&in, SDL_BUTTON_LEFT, true);
    s = tick(&in);
    EXPECT_TRUE(eng_action_pressed(&s, A));
    EXPECT_TRUE(eng_action_held(&s, A));
    EXPECT_FALSE(eng_action_released(&s, A));

    s = tick(&in); /* maintenue : plus « pressed » */
    EXPECT_FALSE(eng_action_pressed(&s, A));
    EXPECT_TRUE(eng_action_held(&s, A));

    eng_input_mouse_button(&in, SDL_BUTTON_LEFT, false);
    s = tick(&in);
    EXPECT_TRUE(eng_action_released(&s, A));
    EXPECT_FALSE(eng_action_held(&s, A));

    s = tick(&in);
    EXPECT_EQ(s.pressed | s.held | s.released, (uint32_t)0);
}

/* Critère M0-18 : aucune pression perdue entre deux ticks. */
UTEST(eng_input, a_press_shorter_than_a_tick_is_not_lost)
{
    EngInput in;
    setup(&in);
    eng_input_mouse_button(&in, SDL_BUTTON_LEFT, true);
    eng_input_mouse_button(&in, SDL_BUTTON_LEFT, false); /* relâché avant même le prochain pas */
    EngInputState s = tick(&in);
    EXPECT_TRUE(eng_action_pressed(&s, A));
    EXPECT_TRUE(eng_action_released(&s, A));
    EXPECT_FALSE(eng_action_held(&s, A));
}

UTEST(eng_input, repeated_taps_between_ticks_are_delivered_once_and_stay_consistent)
{
    EngInput in;
    setup(&in);
    for (int i = 0; i < 3; i++) {
        eng_input_key(&in, SDL_SCANCODE_LCTRL, true);
        eng_input_key(&in, SDL_SCANCODE_LCTRL, false);
    }
    eng_input_key(&in, SDL_SCANCODE_LCTRL, true); /* et finit enfoncée */
    EngInputState s = tick(&in);
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_DODGE));
    EXPECT_TRUE(eng_action_held(&s, ENG_ACTION_DODGE));
    s = tick(&in);
    EXPECT_FALSE(eng_action_pressed(&s, ENG_ACTION_DODGE));
    EXPECT_TRUE(eng_action_held(&s, ENG_ACTION_DODGE));
}

UTEST(eng_input, frames_without_a_tick_accumulate_presses)
{
    /* Affichage à 144 Hz, simulation à 60 Hz : plusieurs images sans pas de simulation. */
    EngInput in;
    setup(&in);
    eng_input_key(&in, SDL_SCANCODE_E, true);
    /* (images sans begin_tick) */
    eng_input_key(&in, SDL_SCANCODE_E, false);
    eng_input_key(&in, SDL_SCANCODE_SPACE, true);
    EngInputState s = tick(&in);
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_INTERACT));
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_JUMP));
    EXPECT_TRUE(eng_action_held(&s, ENG_ACTION_JUMP));
}

UTEST(eng_input, several_ticks_in_one_frame_deliver_the_press_only_once)
{
    EngInput in;
    setup(&in);
    eng_input_mouse_button(&in, SDL_BUTTON_LEFT, true);
    EngInputState first = tick(&in);
    EngInputState second = tick(&in); /* rattrapage : 2e pas de la même image */
    EngInputState third = tick(&in);
    EXPECT_TRUE(eng_action_pressed(&first, A));
    EXPECT_FALSE(eng_action_pressed(&second, A));
    EXPECT_FALSE(eng_action_pressed(&third, A));
    EXPECT_TRUE(eng_action_held(&second, A) && eng_action_held(&third, A));
}

/* Critère M0-18 : le même code de jeu fonctionne au clavier et à la manette. Le jeu ne voit que
 * l'état d'actions ; on vérifie que clavier et manette produisent exactement le même. */
UTEST(eng_input, keyboard_and_gamepad_produce_the_same_game_input)
{
    EngInput keyboard;
    EngInput pad;
    setup(&keyboard);
    setup(&pad);

    eng_input_key(&keyboard, SDL_SCANCODE_SPACE, true);         /* saut */
    eng_input_pad_button(&pad, SDL_GAMEPAD_BUTTON_SOUTH, true); /* saut */
    eng_input_key(&keyboard, SDL_SCANCODE_W, true);             /* avancer */
    eng_input_pad_axis(&pad, ENG_PAD_AXIS_LEFT_Y, -32768);      /* stick vers le haut */
    eng_input_key(&keyboard, SDL_SCANCODE_LCTRL, true);         /* esquive */
    eng_input_pad_button(&pad, SDL_GAMEPAD_BUTTON_EAST, true);

    EngInputState k = tick(&keyboard);
    EngInputState p = tick(&pad);
    EXPECT_EQ(k.pressed, p.pressed);
    EXPECT_EQ(k.held, p.held);
    EXPECT_EQ(k.released, p.released);
    EXPECT_NEAR(k.axis[ENG_AXIS_MOVE].x, p.axis[ENG_AXIS_MOVE].x, 1e-6);
    EXPECT_NEAR(k.axis[ENG_AXIS_MOVE].y, p.axis[ENG_AXIS_MOVE].y, 1e-6);
    EXPECT_NEAR(k.axis[ENG_AXIS_MOVE].y, 1.0, 1e-6);
    EXPECT_TRUE(eng_action_held(&k, ENG_ACTION_JUMP));
}

UTEST(eng_input, two_sources_for_one_action_do_not_cause_spurious_edges)
{
    EngInput in;
    setup(&in);
    eng_input_key(&in, SDL_SCANCODE_SPACE, true);
    eng_input_pad_button(&in, SDL_GAMEPAD_BUTTON_SOUTH, true); /* 2e source, déjà « enfoncée » */
    EngInputState s = tick(&in);
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_JUMP));

    eng_input_key(&in, SDL_SCANCODE_SPACE, false); /* l'autre source la maintient */
    s = tick(&in);
    EXPECT_TRUE(eng_action_held(&s, ENG_ACTION_JUMP));
    EXPECT_FALSE(eng_action_released(&s, ENG_ACTION_JUMP));

    eng_input_pad_button(&in, SDL_GAMEPAD_BUTTON_SOUTH, false);
    s = tick(&in);
    EXPECT_TRUE(eng_action_released(&s, ENG_ACTION_JUMP));
}

UTEST(eng_input, move_axis_from_keys_is_normalized_on_diagonals)
{
    EngInput in;
    setup(&in);
    eng_input_key(&in, SDL_SCANCODE_D, true);
    eng_input_key(&in, SDL_SCANCODE_W, true);
    EngInputState s = tick(&in);
    float length = SDL_sqrtf(s.axis[ENG_AXIS_MOVE].x * s.axis[ENG_AXIS_MOVE].x +
                             s.axis[ENG_AXIS_MOVE].y * s.axis[ENG_AXIS_MOVE].y);
    EXPECT_NEAR(length, 1.0, 1e-5); /* pas de diagonale plus rapide (1,41) */
    EXPECT_TRUE(s.axis[ENG_AXIS_MOVE].x > 0.0F && s.axis[ENG_AXIS_MOVE].y > 0.0F);
}

UTEST(eng_input, opposite_keys_cancel)
{
    EngInput in;
    setup(&in);
    eng_input_key(&in, SDL_SCANCODE_A, true);
    eng_input_key(&in, SDL_SCANCODE_D, true);
    EngInputState s = tick(&in);
    EXPECT_EQ(s.axis[ENG_AXIS_MOVE].x, 0.0F);
}

UTEST(eng_input, stick_deadzone_hides_drift_and_rescales_smoothly)
{
    EngInput in;
    setup(&in);
    eng_input_pad_axis(&in, ENG_PAD_AXIS_LEFT_X, 4000); /* dérive : 0,12 < zone morte 0,2 */
    eng_input_pad_axis(&in, ENG_PAD_AXIS_LEFT_Y, -3000);
    EngInputState s = tick(&in);
    EXPECT_EQ(s.axis[ENG_AXIS_MOVE].x, 0.0F);
    EXPECT_EQ(s.axis[ENG_AXIS_MOVE].y, 0.0F);

    eng_input_pad_axis(&in, ENG_PAD_AXIS_LEFT_X, 32767); /* plein à droite */
    eng_input_pad_axis(&in, ENG_PAD_AXIS_LEFT_Y, 0);
    s = tick(&in);
    EXPECT_NEAR(s.axis[ENG_AXIS_MOVE].x, 1.0, 1e-5);

    /* Juste au-dessus de la zone morte : valeur proche de 0, pas de saut brutal à 0,2. */
    eng_input_pad_axis(&in, ENG_PAD_AXIS_LEFT_X, 6600); /* ~0,2014 */
    s = tick(&in);
    EXPECT_TRUE(s.axis[ENG_AXIS_MOVE].x > 0.0F && s.axis[ENG_AXIS_MOVE].x < 0.01F);
}

UTEST(eng_input, deadzone_function_properties)
{
    EngAxisValue v = eng_apply_deadzone(0.1F, 0.1F, 0.25F);
    EXPECT_EQ(v.x, 0.0F);
    v = eng_apply_deadzone(1.0F, 0.0F, 0.25F);
    EXPECT_NEAR(v.x, 1.0, 1e-6);
    v = eng_apply_deadzone(0.625F, 0.0F, 0.25F); /* milieu de [0,25 ; 1] */
    EXPECT_NEAR(v.x, 0.5, 1e-6);
    v = eng_apply_deadzone(0.6F, 0.6F, 0.2F); /* la direction est conservée */
    EXPECT_NEAR(v.x, v.y, 1e-6);
    v = eng_apply_deadzone(1.0F, 1.0F, 0.2F); /* hors du cercle unité : ramené à 1 */
    EXPECT_NEAR(SDL_sqrtf(v.x * v.x + v.y * v.y), 1.0, 1e-5);
}

UTEST(eng_input, stick_y_is_inverted_so_up_is_positive)
{
    EngInput in;
    setup(&in);
    eng_input_pad_axis(&in, ENG_PAD_AXIS_LEFT_Y, -32768); /* SDL : haut = négatif */
    EngInputState s = tick(&in);
    EXPECT_TRUE(s.axis[ENG_AXIS_MOVE].y > 0.9F);
}

UTEST(eng_input, mouse_motion_is_accumulated_and_consumed_once)
{
    EngInput in;
    setup(&in);
    for (int i = 0; i < 50; i++) {
        eng_input_mouse_motion(&in, 2.0F, -1.0F); /* 50 événements entre deux pas */
    }
    EngInputState s = tick(&in);
    EXPECT_NEAR(s.axis[ENG_AXIS_LOOK].x, 100.0 * 0.002, 1e-5); /* 100 px, aucun perdu */
    EXPECT_NEAR(s.axis[ENG_AXIS_LOOK].y, 50.0 * 0.002, 1e-5);  /* souris vers le haut = regard + */
    s = tick(&in);
    EXPECT_EQ(s.axis[ENG_AXIS_LOOK].x, 0.0F); /* consommé : pas de double comptage */
    EXPECT_EQ(s.axis[ENG_AXIS_LOOK].y, 0.0F);
}

UTEST(eng_input, trigger_acts_as_a_button_above_the_threshold)
{
    EngInput in;
    setup(&in);
    eng_input_pad_axis(&in, ENG_PAD_AXIS_RIGHT_TRIGGER, 10000); /* ~0,3 */
    EngInputState s = tick(&in);
    EXPECT_FALSE(eng_action_held(&s, ENG_ACTION_HEAVY_ATTACK));
    eng_input_pad_axis(&in, ENG_PAD_AXIS_RIGHT_TRIGGER, 30000); /* ~0,92 */
    s = tick(&in);
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_HEAVY_ATTACK));
    eng_input_pad_axis(&in, ENG_PAD_AXIS_RIGHT_TRIGGER, 0);
    s = tick(&in);
    EXPECT_TRUE(eng_action_released(&s, ENG_ACTION_HEAVY_ATTACK));
}

UTEST(eng_input, losing_focus_releases_everything)
{
    EngInput in;
    setup(&in);
    eng_input_key(&in, SDL_SCANCODE_W, true);
    eng_input_mouse_button(&in, SDL_BUTTON_LEFT, true);
    (void)tick(&in);
    eng_input_release_all(&in);
    EngInputState s = tick(&in);
    EXPECT_TRUE(eng_action_released(&s, A));
    EXPECT_FALSE(eng_action_held(&s, A));
    EXPECT_EQ(s.axis[ENG_AXIS_MOVE].y, 0.0F); /* plus de touche « bloquée » */
}

UTEST(eng_input, gamepad_unplug_resets_its_state)
{
    EngInput in;
    setup(&in);
    eng_input_pad_button(&in, SDL_GAMEPAD_BUTTON_WEST, true);
    eng_input_pad_axis(&in, ENG_PAD_AXIS_LEFT_X, 32767);
    (void)tick(&in);
    eng_input_pad_reset(&in);
    EngInputState s = tick(&in);
    EXPECT_TRUE(eng_action_released(&s, A));
    EXPECT_EQ(s.axis[ENG_AXIS_MOVE].x, 0.0F);
}

UTEST(eng_input, out_of_range_codes_are_ignored_and_binding_limits_enforced)
{
    EngInput in;
    setup(&in);
    eng_input_key(&in, -1, true);
    eng_input_key(&in, 100000, true);
    eng_input_pad_button(&in, 99, true);
    eng_input_pad_axis(&in, 17, 5);
    eng_input_mouse_button(&in, 99, true);
    EngInputState s = tick(&in);
    EXPECT_EQ(s.pressed | s.held, (uint32_t)0);

    EngInput empty;
    eng_input_init(&empty);
    for (int i = 0; i < ENG_MAX_BUTTON_BINDINGS; i++) {
        EXPECT_TRUE(eng_input_bind_button(&empty, A, ENG_BIND_KEY, 10 + i));
    }
    EXPECT_FALSE(eng_input_bind_button(&empty, A, ENG_BIND_KEY, 99)); /* plein */
    EXPECT_FALSE(eng_input_bind_button(&empty, ENG_ACTION_COUNT, ENG_BIND_KEY, 1));
    EngAxisBinding bad = {.deadzone = 1.0F};
    EXPECT_FALSE(eng_input_bind_axis(&empty, ENG_AXIS_MOVE, &bad));
}

#define FASTER ENG_ACTION_FLY_FASTER
#define SLOWER ENG_ACTION_FLY_SLOWER
#define FLY_BITS ((1U << FASTER) | (1U << SLOWER))

/* La molette presse les actions qui lui sont liées (par défaut : la vitesse du vol créatif), un
 * cran par pas : trois crans arrivés ensemble comptent trois fois. */
UTEST(eng_input, wheel_notches_press_their_actions_one_per_tick)
{
    EngInput in;
    setup(&in);
    EngInputState s = tick(&in);
    EXPECT_EQ(FLY_BITS, s.wheel_bound); /* seules ces deux actions sont sur la molette */
    EXPECT_EQ(0U, s.pressed);

    eng_input_mouse_wheel(&in, 3.0F);
    for (int i = 0; i < 3; i++) {
        s = tick(&in);
        EXPECT_TRUE(eng_action_pressed(&s, FASTER));
        EXPECT_TRUE(eng_action_released(&s, FASTER)); /* jamais « enfoncée » */
        EXPECT_FALSE(eng_action_held(&s, FASTER));
        EXPECT_FALSE(eng_action_pressed(&s, SLOWER));
        /* les crans bruts, eux, arrivent tous au premier pas (zoom de la caméra) */
        EXPECT_NEAR(i == 0 ? 3.0F : 0.0F, s.wheel, 1e-6F);
    }
    s = tick(&in);
    EXPECT_EQ(0U, s.pressed | s.released);

    eng_input_mouse_wheel(&in, -2.0F); /* vers le bas : l'autre action */
    for (int i = 0; i < 2; i++) {
        s = tick(&in);
        EXPECT_TRUE(eng_action_pressed(&s, SLOWER));
        EXPECT_FALSE(eng_action_pressed(&s, FASTER));
    }
    s = tick(&in);
    EXPECT_EQ(0U, s.pressed);

    /* un cran dans l'autre sens annule un cran pas encore livré */
    eng_input_mouse_wheel(&in, 3.0F);
    eng_input_mouse_wheel(&in, -1.0F);
    uint32_t up = 0;
    uint32_t down = 0;
    for (int i = 0; i < 6; i++) {
        s = tick(&in);
        up += eng_action_pressed(&s, FASTER) ? 1U : 0U;
        down += eng_action_pressed(&s, SLOWER) ? 1U : 0U;
    }
    EXPECT_EQ(2U, up);
    EXPECT_EQ(0U, down);
}

/* Pavé tactile : la molette arrive par fractions de cran ; elles s'additionnent jusqu'à faire un
 * cran. Un changement de sens repart de zéro. */
UTEST(eng_input, wheel_fractions_add_up_to_whole_notches)
{
    EngInput in;
    setup(&in);
    uint32_t serial = in.last_serial;
    eng_input_mouse_wheel(&in, 0.4F);
    eng_input_mouse_wheel(&in, 0.4F);
    EngInputState s = tick(&in);
    EXPECT_FALSE(eng_action_pressed(&s, FASTER));
    EXPECT_EQ(serial, in.last_serial); /* pas encore un cran : pas une « pression » (remappage) */
    eng_input_mouse_wheel(&in, 0.4F);  /* 1,2 cran : un cran, reste 0,2 */
    EXPECT_EQ(serial + 1U, in.last_serial);
    EXPECT_EQ(ENG_BIND_MOUSE_WHEEL, in.last_kind);
    EXPECT_EQ(ENG_WHEEL_UP, in.last_code);
    s = tick(&in);
    EXPECT_TRUE(eng_action_pressed(&s, FASTER));
    s = tick(&in);
    EXPECT_FALSE(eng_action_pressed(&s, FASTER));
    eng_input_mouse_wheel(&in, 0.75F); /* 0,95 : toujours pas */
    s = tick(&in);
    EXPECT_FALSE(eng_action_pressed(&s, FASTER));
    /* demi-tour : le reste (0,95 vers le haut) est oublié, 0,6 vers le bas ne fait pas un cran */
    eng_input_mouse_wheel(&in, -0.6F);
    s = tick(&in);
    EXPECT_EQ(0U, s.pressed);
    serial = in.last_serial;
    eng_input_mouse_wheel(&in, -0.5F); /* 1,1 vers le bas : un cran */
    EXPECT_EQ(serial + 1U, in.last_serial);
    EXPECT_EQ(ENG_WHEEL_DOWN, in.last_code);
    s = tick(&in);
    EXPECT_TRUE(eng_action_pressed(&s, SLOWER));
    EXPECT_FALSE(eng_action_pressed(&s, FASTER));
}

/* Bornes et cas limites de la molette : grand coup de molette plafonné, valeurs non finies,
 * perte du focus, action tenue par une touche en même temps, molette libre. */
UTEST(eng_input, wheel_binding_limits_and_edge_cases)
{
    EngInput in;
    setup(&in);
    /* molette libre : au plus ENG_WHEEL_PENDING_MAX crans en attente, même en plusieurs coups */
    eng_input_mouse_wheel(&in, 1000.0F);
    eng_input_mouse_wheel(&in, 40.0F);
    uint32_t pulses = 0;
    for (int i = 0; i < 200; i++) {
        const EngInputState s = tick(&in);
        pulses += eng_action_pressed(&s, FASTER) ? 1U : 0U;
    }
    EXPECT_EQ((uint32_t)ENG_WHEEL_PENDING_MAX, pulses);
    eng_input_mouse_wheel(&in, -1000.0F);
    eng_input_mouse_wheel(&in, -40.0F);
    pulses = 0;
    for (int i = 0; i < 200; i++) {
        const EngInputState s = tick(&in);
        pulses += eng_action_pressed(&s, SLOWER) ? 1U : 0U;
    }
    EXPECT_EQ((uint32_t)ENG_WHEEL_PENDING_MAX, pulses);

    eng_input_mouse_wheel(&in, NAN); /* valeurs absurdes : ignorées, la molette brute aussi */
    eng_input_mouse_wheel(&in, INFINITY);
    EngInputState s = tick(&in);
    EXPECT_EQ(0U, s.pressed);
    EXPECT_EQ(0.0F, s.wheel);

    eng_input_mouse_wheel(&in, 5.0F); /* la fenêtre perd le focus : les crans en attente tombent */
    eng_input_release_all(&in);
    s = tick(&in);
    EXPECT_FALSE(eng_action_pressed(&s, FASTER));
    eng_input_mouse_wheel(&in, 2.0F); /* eng_input_drop_wheel aussi (le menu, après un remappage) */
    eng_input_drop_wheel(&in);
    s = tick(&in);
    EXPECT_FALSE(eng_action_pressed(&s, FASTER));

    /* une action tenue par une touche ET pressée par la molette : pressée de nouveau, pas relâchée
     */
    ASSERT_TRUE(eng_input_bind_button(&in, FASTER, ENG_BIND_KEY, SDL_SCANCODE_UP));
    eng_input_key(&in, SDL_SCANCODE_UP, true);
    s = tick(&in);
    EXPECT_TRUE(eng_action_pressed(&s, FASTER) && eng_action_held(&s, FASTER));
    eng_input_mouse_wheel(&in, 1.0F);
    s = tick(&in);
    EXPECT_TRUE(eng_action_pressed(&s, FASTER));
    EXPECT_TRUE(eng_action_held(&s, FASTER));
    EXPECT_FALSE(eng_action_released(&s, FASTER));
    eng_input_key(&in, SDL_SCANCODE_UP, false);
    s = tick(&in);

    /* une liaison de molette n'accepte que ses deux sens ; un genre inconnu est refusé */
    EXPECT_FALSE(eng_input_bind_button(&in, FASTER, ENG_BIND_MOUSE_WHEEL, 2));
    EXPECT_FALSE(eng_input_bind_button(&in, FASTER, ENG_BIND_MOUSE_WHEEL, -1));
    EXPECT_FALSE(eng_input_bind_button(&in, FASTER, (EngBindKind)(ENG_BIND_MOUSE_WHEEL + 1), 0));
    EXPECT_FALSE(eng_input_bind_button(&in, FASTER, (EngBindKind)-1, 0));

    /* reliées ailleurs, les deux actions libèrent la molette : plus de bit, plus d'impulsion */
    eng_input_clear_action(&in, FASTER);
    eng_input_clear_action(&in, SLOWER);
    ASSERT_TRUE(eng_input_bind_button(&in, FASTER, ENG_BIND_KEY, SDL_SCANCODE_UP));
    eng_input_mouse_wheel(&in, 2.0F);
    s = tick(&in);
    EXPECT_EQ(0U, s.wheel_bound);
    EXPECT_EQ(0U, s.pressed);
    EXPECT_NEAR(2.0F, s.wheel, 1e-6F); /* la molette brute reste disponible (zoom) */
    /* n'importe quelle action peut aller sur la molette */
    ASSERT_TRUE(
        eng_input_bind_button(&in, ENG_ACTION_SWITCH_WEAPON, ENG_BIND_MOUSE_WHEEL, ENG_WHEEL_DOWN));
    s = tick(&in); /* (le cran encore en attente part ici, vers le haut : sans effet) */
    s = tick(&in);
    eng_input_mouse_wheel(&in, -1.0F);
    s = tick(&in);
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_SWITCH_WEAPON));
    const uint32_t weapon_bit = 1U << ENG_ACTION_SWITCH_WEAPON;
    EXPECT_EQ(weapon_bit, s.wheel_bound);
}

UTEST(eng_input, unbound_input_does_nothing)
{
    EngInput in;
    eng_input_init(&in);
    eng_input_key(&in, SDL_SCANCODE_W, true);
    eng_input_mouse_button(&in, SDL_BUTTON_LEFT, true);
    EngInputState s = tick(&in);
    EXPECT_EQ(s.pressed | s.held, (uint32_t)0);
    EXPECT_EQ(s.axis[ENG_AXIS_MOVE].y, 0.0F);
}

/* ── Touches de menu (fixes : flèches, Entrée, Échap, croix de la manette) ── */

UTEST(eng_input, menu_keys_are_fixed_latched_once_and_held_while_down)
{
    EngInput in;
    eng_input_init(&in); /* aucune liaison : les touches de menu marchent quand même */
    eng_input_key(&in, ENG_KEY_DOWN, true);
    eng_input_key(&in, ENG_KEY_RETURN, true);
    eng_input_key(&in, ENG_KEY_RETURN, false); /* pression plus courte qu'un pas : gardée */
    EngInputState s = tick(&in);
    EXPECT_EQ(s.menu_pressed, (uint32_t)(ENG_MENU_KEY_DOWN | ENG_MENU_KEY_OK));
    EXPECT_EQ(s.menu_held, (uint32_t)ENG_MENU_KEY_DOWN);
    s = tick(&in); /* tenue : plus « pressée », toujours « enfoncée » */
    EXPECT_EQ(s.menu_pressed, 0U);
    EXPECT_EQ(s.menu_held, (uint32_t)ENG_MENU_KEY_DOWN);
    eng_input_key(&in, SDL_SCANCODE_SPACE, true); /* un autre événement ne la « presse » pas */
    eng_input_mouse_button(&in, SDL_BUTTON_LEFT, true);
    s = tick(&in);
    EXPECT_EQ(s.menu_pressed, 0U);
    EXPECT_EQ(s.menu_held, (uint32_t)ENG_MENU_KEY_DOWN);
    eng_input_key(&in, SDL_SCANCODE_SPACE, false);
    eng_input_mouse_button(&in, SDL_BUTTON_LEFT, false);
    eng_input_key(&in, ENG_KEY_DOWN, false);
    s = tick(&in);
    EXPECT_EQ(s.menu_held, 0U);
    /* chaque touche a son bit ; les deux Entrée valident ; Échap = retour */
    eng_input_key(&in, ENG_KEY_UP, true);
    eng_input_key(&in, ENG_KEY_LEFT, true);
    eng_input_key(&in, ENG_KEY_RIGHT, true);
    eng_input_key(&in, ENG_KEY_KP_ENTER, true);
    eng_input_key(&in, ENG_KEY_ESCAPE, true);
    s = tick(&in);
    EXPECT_EQ(s.menu_pressed, (uint32_t)(ENG_MENU_KEY_UP | ENG_MENU_KEY_LEFT | ENG_MENU_KEY_RIGHT |
                                         ENG_MENU_KEY_OK | ENG_MENU_KEY_BACK));
    /* fenêtre qui perd le focus : tout est relâché */
    eng_input_release_all(&in);
    s = tick(&in);
    EXPECT_EQ(s.menu_held, 0U);
    EXPECT_EQ(s.menu_pressed, 0U);
    /* la croix de la manette choisit aussi ; une touche ordinaire n'est pas une touche de menu */
    eng_input_pad_button(&in, ENG_PAD_DPAD_UP, true);
    eng_input_pad_button(&in, ENG_PAD_DPAD_RIGHT, true);
    eng_input_key(&in, SDL_SCANCODE_SPACE, true);
    s = tick(&in);
    EXPECT_EQ(s.menu_pressed, (uint32_t)(ENG_MENU_KEY_UP | ENG_MENU_KEY_RIGHT));
    eng_input_pad_reset(&in);
    s = tick(&in);
    EXPECT_EQ(s.menu_held, 0U);
}
