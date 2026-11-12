#include <SDL3/SDL.h>
#include <string.h>

#include "engine/arena.h"
#include "engine/config.h"
#include "engine/files.h"
#include "test_util.h"
#include "utest.h"

#define DIR TEST_TMP_DIR "/config"

static EngConfig parsed(const char *text, int *issues)
{
    EngConfig config;
    eng_config_defaults(&config);
    int n = eng_config_parse(&config, text, "test.ini");
    if (issues != NULL) {
        *issues = n;
    }
    return config;
}

/* Comparaison champ par champ (un memcmp lirait les octets de remplissage). */
static bool same_bindings(const EngInput *a, const EngInput *b)
{
    for (int act = 0; act < ENG_ACTION_COUNT; act++) {
        for (int i = 0; i < ENG_MAX_BUTTON_BINDINGS; i++) {
            if (a->buttons[act][i].kind != b->buttons[act][i].kind ||
                a->buttons[act][i].code != b->buttons[act][i].code) {
                return false;
            }
        }
    }
    for (int ax = 0; ax < ENG_AXIS_COUNT; ax++) {
        const EngAxisBinding *x = &a->axes[ax];
        const EngAxisBinding *y = &b->axes[ax];
        if (x->key_left != y->key_left || x->key_right != y->key_right ||
            x->key_down != y->key_down || x->key_up != y->key_up || x->stick != y->stick ||
            x->deadzone != y->deadzone || x->mouse_sensitivity != y->mouse_sensitivity) {
            return false;
        }
    }
    return true;
}

UTEST(eng_config, defaults)
{
    EngConfig c;
    eng_config_defaults(&c);
    EXPECT_EQ(c.window_width, 1280);
    EXPECT_EQ(c.window_height, 720);
    EXPECT_FALSE(c.fullscreen);
    EXPECT_TRUE(c.vsync);
    EXPECT_NEAR(c.mouse_sensitivity, 0.002, 1e-6);
    EXPECT_NEAR(c.stick_deadzone, 0.2, 1e-6);
    EXPECT_EQ(c.issue_count, 0);
}

UTEST(eng_config, reads_every_setting)
{
    int issues = -1;
    EngConfig c = parsed("resolution = 1920x1080\n"
                         "fullscreen = true\n"
                         "vsync = off\n"
                         "mouse_sensitivity = 0.005\n"
                         "stick_deadzone = 0.35\n",
                         &issues);
    EXPECT_EQ(issues, 0);
    EXPECT_EQ(c.window_width, 1920);
    EXPECT_EQ(c.window_height, 1080);
    EXPECT_TRUE(c.fullscreen);
    EXPECT_FALSE(c.vsync);
    EXPECT_NEAR(c.mouse_sensitivity, 0.005, 1e-7);
    EXPECT_NEAR(c.stick_deadzone, 0.35, 1e-7);
}

UTEST(eng_config, absent_keys_keep_their_defaults)
{
    EngConfig c = parsed("fullscreen = yes\n", NULL);
    EXPECT_TRUE(c.fullscreen);
    EXPECT_EQ(c.window_width, 1280); /* non mentionnée : défaut */
    EXPECT_TRUE(c.vsync);
}

UTEST(eng_config, tolerates_comments_whitespace_crlf_bom_and_case)
{
    int issues = -1;
    EngConfig c = parsed("\xEF\xBB\xBF# commentaire\r\n"
                         "; autre commentaire\r\n"
                         "\r\n"
                         "   RESOLUTION\t=\t 800 X 600   # fin de ligne\r\n"
                         "Fullscreen=TRUE ; autre\r\n"
                         "vsync = 0",
                         &issues); /* pas de saut de ligne final */
    EXPECT_EQ(issues, 0);
    EXPECT_EQ(c.window_width, 800);
    EXPECT_EQ(c.window_height, 600);
    EXPECT_TRUE(c.fullscreen);
    EXPECT_FALSE(c.vsync);
}

UTEST(eng_config, last_duplicate_wins)
{
    EngConfig c = parsed("resolution = 800x600\nresolution = 1024x768\n", NULL);
    EXPECT_EQ(c.window_width, 1024);
}

/* Une faute de frappe ne doit jamais empêcher de jouer : ligne ignorée, le reste est lu. */
UTEST(eng_config, invalid_lines_are_reported_and_ignored_without_stopping)
{
    int issues = -1;
    EngConfig c = parsed("resolutoin = 800x600\n"            /* 1 : clé inconnue */
                         "fullscreen = peut-etre\n"          /* 2 : booléen illisible */
                         "resolution = 99999x600\n"          /* 3 : hors limites */
                         "resolution = 800\n"                /* 4 : pas de « x » */
                         "ceci n'a pas de signe egal\n"      /* 5 */
                         "vsync =\n"                         /* 6 : valeur vide */
                         "= 12\n"                            /* 7 : clé vide */
                         "mouse_sensitivity = 0,005\n"       /* 8 : virgule décimale */
                         "mouse_sensitivity = -1\n"          /* 9 : négative */
                         "stick_deadzone = 1.5\n"            /* 10 : hors limites */
                         "bind.danse = key:W\n"              /* 11 : action inconnue */
                         "bind.jump = key:TouchePasReelle\n" /* 12 : liaison illisible */
                         "vsync = false\n"      /* valide : doit être lue malgré tout */
                         "fullscreen = true\n", /* valide */
                         &issues);
    EXPECT_EQ(issues, 12);
    EXPECT_EQ(c.issue_count, 12);
    EXPECT_FALSE(c.vsync); /* lignes valides appliquées */
    EXPECT_TRUE(c.fullscreen);
    EXPECT_EQ(c.window_width, 1280); /* lignes invalides : défauts conservés */
    EXPECT_NEAR(c.mouse_sensitivity, 0.002, 1e-6);
    EXPECT_NEAR(c.stick_deadzone, 0.2, 1e-6);
    EXPECT_FALSE(c.action_overridden[ENG_ACTION_JUMP]);
}

UTEST(eng_config, overlong_line_is_skipped)
{
    char text[3000];
    memset(text, 'a', sizeof text);
    text[2000] = '\n';
    SDL_strlcpy(text + 2001, "fullscreen = true\n", sizeof text - 2001);
    int issues = -1;
    EngConfig c = parsed(text, &issues);
    EXPECT_EQ(issues, 1);
    EXPECT_TRUE(c.fullscreen);
}

UTEST(eng_config, parses_binding_sources)
{
    EngButtonBinding b;
    ASSERT_TRUE(eng_config_parse_binding("key:Space", &b));
    EXPECT_EQ(b.kind, ENG_BIND_KEY);
    EXPECT_EQ(b.code, (int)SDL_SCANCODE_SPACE);
    ASSERT_TRUE(eng_config_parse_binding("  key : Left Ctrl ", &b));
    EXPECT_EQ(b.code, (int)SDL_SCANCODE_LCTRL);
    ASSERT_TRUE(eng_config_parse_binding("KEY:w", &b)); /* insensible à la casse */
    EXPECT_EQ(b.code, (int)SDL_SCANCODE_W);
    ASSERT_TRUE(eng_config_parse_binding("mouse:right", &b));
    EXPECT_EQ(b.kind, ENG_BIND_MOUSE_BUTTON);
    EXPECT_EQ(b.code, (int)SDL_BUTTON_RIGHT);
    ASSERT_TRUE(eng_config_parse_binding("pad:west", &b));
    EXPECT_EQ(b.kind, ENG_BIND_PAD_BUTTON);
    EXPECT_EQ(b.code, (int)SDL_GAMEPAD_BUTTON_WEST);
    ASSERT_TRUE(eng_config_parse_binding("pad:x", &b)); /* alias : x = west */
    EXPECT_EQ(b.code, (int)SDL_GAMEPAD_BUTTON_WEST);
    ASSERT_TRUE(eng_config_parse_binding("trigger:left", &b));
    EXPECT_EQ(b.kind, ENG_BIND_PAD_TRIGGER);
    EXPECT_EQ(b.code, ENG_PAD_AXIS_LEFT_TRIGGER);

    EXPECT_FALSE(eng_config_parse_binding("Space", &b));        /* pas de « : » */
    EXPECT_FALSE(eng_config_parse_binding("joystick:1", &b));   /* type inconnu */
    EXPECT_FALSE(eng_config_parse_binding("mouse:gauche", &b)); /* nom inconnu */
    EXPECT_FALSE(eng_config_parse_binding("pad:", &b));
    EXPECT_FALSE(eng_config_parse_binding("key:", &b));
}

static EngInputState press_and_tick(EngInput *input, void (*apply)(EngInput *))
{
    apply(input);
    EngInputState s;
    eng_input_begin_tick(input, &s);
    return s;
}

static void press_f(EngInput *input)
{
    eng_input_key(input, SDL_SCANCODE_F, true);
}

static void press_space(EngInput *input)
{
    eng_input_key(input, SDL_SCANCODE_SPACE, true);
}

UTEST(eng_config, bind_replaces_the_default_bindings_of_that_action_only)
{
    EngConfig c = parsed("bind.jump = key:F\n", NULL);
    EngInput input;
    eng_input_set_default_bindings(&input);
    eng_config_apply_input(&c, &input);

    EngInputState s = press_and_tick(&input, press_f);
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_JUMP)); /* nouvelle touche */

    EngInput other;
    eng_input_set_default_bindings(&other);
    eng_config_apply_input(&c, &other);
    s = press_and_tick(&other, press_space);
    EXPECT_FALSE(eng_action_pressed(&s, ENG_ACTION_JUMP)); /* l'ancienne ne marche plus */

    eng_input_key(&other, SDL_SCANCODE_E, true); /* les autres actions gardent leurs défauts */
    eng_input_begin_tick(&other, &s);
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_INTERACT));
}

UTEST(eng_config, several_sources_and_none)
{
    EngConfig c = parsed("bind.attack = key:J, mouse:middle, pad:a, trigger:left\n"
                         "bind.pause = none\n",
                         NULL);
    EngInput input;
    eng_input_set_default_bindings(&input);
    eng_config_apply_input(&c, &input);
    eng_input_key(&input, SDL_SCANCODE_J, true);
    eng_input_key(&input, SDL_SCANCODE_P, true); /* pause désactivée */
    EngInputState s;
    eng_input_begin_tick(&input, &s);
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_ATTACK));
    EXPECT_FALSE(eng_action_pressed(&s, ENG_ACTION_PAUSE));
    eng_input_key(&input, SDL_SCANCODE_J, false);
    eng_input_mouse_button(&input, SDL_BUTTON_MIDDLE, true);
    eng_input_begin_tick(&input, &s);
    EXPECT_TRUE(eng_action_held(&s, ENG_ACTION_ATTACK)); /* 2e source */
}

UTEST(eng_config, a_repeated_bind_line_replaces_the_previous_one_entirely)
{
    EngConfig c = parsed("bind.attack = key:J, key:K, key:L\n"
                         "bind.attack = key:M\n",
                         NULL);
    EngInput input;
    eng_input_set_default_bindings(&input);
    eng_config_apply_input(&c, &input);
    eng_input_key(&input, SDL_SCANCODE_K, true); /* source de la 1re ligne : ne doit plus agir */
    EngInputState s;
    eng_input_begin_tick(&input, &s);
    EXPECT_FALSE(eng_action_pressed(&s, ENG_ACTION_ATTACK));
    eng_input_key(&input, SDL_SCANCODE_K, false);
    eng_input_key(&input, SDL_SCANCODE_M, true);
    eng_input_begin_tick(&input, &s);
    EXPECT_TRUE(eng_action_pressed(&s, ENG_ACTION_ATTACK));
}

UTEST(eng_config, too_many_or_bad_sources_leave_the_action_unchanged)
{
    int issues = -1;
    EngConfig c = parsed("bind.attack = key:A, key:B, key:C, key:D, key:E\n"
                         "bind.dodge = key:Space, pad:nimporte\n"
                         "bind.jump = ,\n",
                         &issues);
    EXPECT_EQ(issues, 3);
    EXPECT_FALSE(c.action_overridden[ENG_ACTION_ATTACK]);
    EXPECT_FALSE(c.action_overridden[ENG_ACTION_DODGE]); /* tout ou rien */
    EXPECT_FALSE(c.action_overridden[ENG_ACTION_JUMP]);
}

UTEST(eng_config, move_keys_sensitivity_and_deadzone_reach_the_input_system)
{
    EngConfig c = parsed("bind.move.keys = Left, Right, Down, Up\n"
                         "mouse_sensitivity = 0.01\n"
                         "stick_deadzone = 0.5\n",
                         NULL);
    EngInput input;
    eng_input_set_default_bindings(&input);
    eng_config_apply_input(&c, &input);

    eng_input_key(&input, SDL_SCANCODE_UP, true);
    EngInputState s;
    eng_input_begin_tick(&input, &s);
    EXPECT_NEAR(s.axis[ENG_AXIS_MOVE].y, 1.0, 1e-6);

    eng_input_key(&input, SDL_SCANCODE_UP, false);
    eng_input_key(&input, SDL_SCANCODE_W, true); /* l'ancienne touche ne marche plus */
    eng_input_begin_tick(&input, &s);
    EXPECT_EQ(s.axis[ENG_AXIS_MOVE].y, 0.0F);

    eng_input_mouse_motion(&input, 10.0F, 0.0F);
    eng_input_begin_tick(&input, &s);
    EXPECT_NEAR(s.axis[ENG_AXIS_LOOK].x, 0.1, 1e-6); /* 10 px x 0,01 */

    eng_input_pad_axis(&input, ENG_PAD_AXIS_LEFT_X, 12000); /* ~0,37 : sous la zone morte 0,5 */
    eng_input_begin_tick(&input, &s);
    EXPECT_EQ(s.axis[ENG_AXIS_MOVE].x, 0.0F);
}

UTEST(eng_config, invalid_move_keys_are_rejected)
{
    int issues = -1;
    EngConfig c = parsed("bind.move.keys = A, D, S\n" /* 3 touches */
                         "bind.move.keys = A, D, S, Zorglub\n"
                         "bind.move.keys = A, D, S, W, X\n",
                         &issues);
    EXPECT_EQ(issues, 3);
    EXPECT_FALSE(c.move_keys_set);
}

UTEST(eng_config, applying_an_untouched_config_changes_nothing)
{
    EngConfig c;
    eng_config_defaults(&c);
    EngInput defaults;
    EngInput applied;
    eng_input_set_default_bindings(&defaults);
    eng_input_set_default_bindings(&applied);
    eng_config_apply_input(&c, &applied);
    EXPECT_TRUE(same_bindings(&defaults, &applied));
}

/* ── Chargement ── */

UTEST(eng_config, missing_file_gives_defaults_silently)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 4096));
    EngConfig c;
    EXPECT_FALSE(eng_config_load(&c, &arena, DIR "/n_existe_pas.ini"));
    EXPECT_EQ(c.window_width, 1280);
    EXPECT_EQ(c.issue_count, 0);
    EXPECT_EQ(arena.used, (size_t)0);
    eng_arena_shutdown(&arena);
}

UTEST(eng_config, values_are_read_from_a_file_and_the_scratch_arena_is_restored)
{
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    (void)SDL_CreateDirectory(DIR);
    const char text[] = "resolution = 640x480\nvsync = false\n";
    ASSERT_TRUE(SDL_SaveFile(DIR "/lecture.ini", text, sizeof text - 1));

    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", 4096));
    (void)eng_arena_alloc(&arena, 100, 1); /* de la place déjà occupée : doit être respectée */
    size_t used = arena.used;
    EngConfig c;
    EXPECT_TRUE(eng_config_load(&c, &arena, DIR "/lecture.ini"));
    EXPECT_EQ(c.window_width, 640);
    EXPECT_EQ(c.window_height, 480);
    EXPECT_FALSE(c.vsync);
    EXPECT_EQ(arena.used, used);
    eng_arena_shutdown(&arena);
    (void)SDL_RemovePath(DIR "/lecture.ini");
}

UTEST(eng_config, a_file_that_does_not_fit_in_the_scratch_arena_falls_back_to_defaults)
{
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    (void)SDL_CreateDirectory(DIR);
    char big[2000];
    memset(big, '#', sizeof big);
    ASSERT_TRUE(SDL_SaveFile(DIR "/gros.ini", big, sizeof big));
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "tiny", 256));
    EngConfig c;
    EXPECT_FALSE(eng_config_load(&c, &arena, DIR "/gros.ini"));
    EXPECT_EQ(c.window_width, 1280);
    eng_arena_shutdown(&arena);
    (void)SDL_RemovePath(DIR "/gros.ini");
}

/* Le fichier d'exemple livré doit rester cohérent avec le code : sinon la documentation ment. */
UTEST(eng_config, shipped_example_file_matches_the_defaults)
{
    EngArena arena;
    ASSERT_TRUE(eng_arena_init(&arena, "test", (size_t)64 * 1024));
    EngFileData file;
    ASSERT_TRUE(eng_file_read_all(&arena, OW_CONFIG_EXAMPLE, &file));

    EngConfig defaults;
    eng_config_defaults(&defaults);
    EngConfig c;
    eng_config_defaults(&c);
    EXPECT_EQ(eng_config_parse(&c, (const char *)file.data, "config.ini"), 0);
    EXPECT_EQ(c.window_width, defaults.window_width);
    EXPECT_EQ(c.window_height, defaults.window_height);
    EXPECT_EQ(c.fullscreen, defaults.fullscreen);
    EXPECT_EQ(c.vsync, defaults.vsync);
    EXPECT_NEAR(c.mouse_sensitivity, defaults.mouse_sensitivity, 1e-6);
    EXPECT_NEAR(c.stick_deadzone, defaults.stick_deadzone, 1e-6);

    /* Les lignes « # bind.xxx = ... » du fichier annoncent les liaisons par défaut : une fois
     * décommentées, elles doivent donner exactement les liaisons par défaut du moteur. */
    char *uncommented = SDL_malloc(file.size + 2);
    ASSERT_TRUE(uncommented != NULL);
    size_t out = 0;
    const char *line = (const char *)file.data;
    while (*line != '\0') {
        const char *end = strchr(line, '\n');
        size_t length = end != NULL ? (size_t)(end - line) : strlen(line);
        size_t skip = strncmp(line, "# bind.", 7) == 0 ? 2 : 0; /* retire « # » */
        memcpy(uncommented + out, line + skip, length - skip);
        out += length - skip;
        uncommented[out++] = '\n';
        line += length + (end != NULL ? 1 : 0);
    }
    uncommented[out] = '\0';

    EngConfig full;
    eng_config_defaults(&full);
    EXPECT_EQ(eng_config_parse(&full, uncommented, "config.ini"), 0);
    EngInput expected;
    EngInput actual;
    eng_input_set_default_bindings(&expected);
    eng_input_set_default_bindings(&actual);
    eng_config_apply_input(&full, &actual);
    EXPECT_TRUE(same_bindings(&expected, &actual));
    EXPECT_TRUE(full.action_overridden[ENG_ACTION_ATTACK]);
    EXPECT_TRUE(full.move_keys_set);

    SDL_free(uncommented);
    eng_arena_shutdown(&arena);
}
