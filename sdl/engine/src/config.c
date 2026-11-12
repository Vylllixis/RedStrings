#include "engine/config.h"

#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "engine/files.h"
#include "engine/log.h"

#define WIDTH_MIN 320
#define HEIGHT_MIN 200
#define SIZE_MAX_PIXELS 16384
#define LINE_MAX_LENGTH 1024

void eng_config_defaults(EngConfig *config)
{
    *config = (EngConfig){
        .window_width = 1280,
        .window_height = 720,
        .fullscreen = false,
        .vsync = true,
        .mouse_sensitivity = 0.002F,
        .stick_deadzone = 0.2F,
    };
}

/* ── Petits outils de texte ── */

static char *trim(char *text)
{
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        end--;
    }
    *end = '\0';
    return text;
}

static bool parse_bool(const char *text, bool *out)
{
    static const char *const true_words[] = {"true", "yes", "on", "1"};
    static const char *const false_words[] = {"false", "no", "off", "0"};
    for (size_t i = 0; i < sizeof true_words / sizeof true_words[0]; i++) {
        if (SDL_strcasecmp(text, true_words[i]) == 0) {
            *out = true;
            return true;
        }
        if (SDL_strcasecmp(text, false_words[i]) == 0) {
            *out = false;
            return true;
        }
    }
    return false;
}

static bool parse_float(const char *text, float *out)
{
    char *end = NULL;
    double value = SDL_strtod(text, &end); /* indépendant de la langue du système (« 0.5 ») */
    if (end == text || *end != '\0' || !(value == value)) {
        return false;
    }
    *out = (float)value;
    return true;
}

static bool parse_int(const char *text, int *out)
{
    char *end = NULL;
    long value = SDL_strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < -1000000 || value > 1000000) {
        return false;
    }
    *out = (int)value;
    return true;
}

/* ── Noms de liaisons ── */

typedef struct NamedCode {
    const char *name;
    int code;
} NamedCode;

static const NamedCode MOUSE_NAMES[] = {
    {"left", SDL_BUTTON_LEFT}, {"middle", SDL_BUTTON_MIDDLE}, {"right", SDL_BUTTON_RIGHT},
    {"x1", SDL_BUTTON_X1},     {"x2", SDL_BUTTON_X2},
};

static const NamedCode PAD_NAMES[] = {
    {"south", SDL_GAMEPAD_BUTTON_SOUTH},
    {"a", SDL_GAMEPAD_BUTTON_SOUTH},
    {"east", SDL_GAMEPAD_BUTTON_EAST},
    {"b", SDL_GAMEPAD_BUTTON_EAST},
    {"west", SDL_GAMEPAD_BUTTON_WEST},
    {"x", SDL_GAMEPAD_BUTTON_WEST},
    {"north", SDL_GAMEPAD_BUTTON_NORTH},
    {"y", SDL_GAMEPAD_BUTTON_NORTH},
    {"back", SDL_GAMEPAD_BUTTON_BACK},
    {"guide", SDL_GAMEPAD_BUTTON_GUIDE},
    {"start", SDL_GAMEPAD_BUTTON_START},
    {"left_stick", SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {"right_stick", SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {"left_shoulder", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {"right_shoulder", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {"dpad_up", SDL_GAMEPAD_BUTTON_DPAD_UP},
    {"dpad_down", SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {"dpad_left", SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {"dpad_right", SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
};

static const NamedCode TRIGGER_NAMES[] = {
    {"left", ENG_PAD_AXIS_LEFT_TRIGGER},
    {"right", ENG_PAD_AXIS_RIGHT_TRIGGER},
};

static const char *const ACTION_NAMES[ENG_ACTION_COUNT] = {
    [ENG_ACTION_ATTACK] = "attack",     [ENG_ACTION_HEAVY_ATTACK] = "heavy_attack",
    [ENG_ACTION_DODGE] = "dodge",       [ENG_ACTION_JUMP] = "jump",
    [ENG_ACTION_INTERACT] = "interact", [ENG_ACTION_SPRINT] = "sprint",
    [ENG_ACTION_LOCK_ON] = "lock_on",   [ENG_ACTION_PAUSE] = "pause",
};

static bool lookup(const NamedCode *table, size_t count, const char *name, int *code)
{
    for (size_t i = 0; i < count; i++) {
        if (SDL_strcasecmp(table[i].name, name) == 0) {
            *code = table[i].code;
            return true;
        }
    }
    return false;
}

bool eng_config_parse_binding(const char *text, EngButtonBinding *out)
{
    char buffer[64];
    if (SDL_strlcpy(buffer, text, sizeof buffer) >= sizeof buffer) {
        return false;
    }
    char *colon = strchr(buffer, ':');
    if (colon == NULL) {
        return false;
    }
    *colon = '\0';
    const char *kind = trim(buffer);
    const char *name = trim(colon + 1);
    int code = 0;

    if (SDL_strcasecmp(kind, "key") == 0) {
        SDL_Scancode scancode = SDL_GetScancodeFromName(name);
        if (scancode == SDL_SCANCODE_UNKNOWN || (int)scancode >= ENG_KEY_CODE_COUNT) {
            return false;
        }
        *out = (EngButtonBinding){ENG_BIND_KEY, (int)scancode};
        return true;
    }
    if (SDL_strcasecmp(kind, "mouse") == 0 &&
        lookup(MOUSE_NAMES, sizeof MOUSE_NAMES / sizeof MOUSE_NAMES[0], name, &code)) {
        *out = (EngButtonBinding){ENG_BIND_MOUSE_BUTTON, code};
        return true;
    }
    if (SDL_strcasecmp(kind, "pad") == 0 &&
        lookup(PAD_NAMES, sizeof PAD_NAMES / sizeof PAD_NAMES[0], name, &code)) {
        *out = (EngButtonBinding){ENG_BIND_PAD_BUTTON, code};
        return true;
    }
    if (SDL_strcasecmp(kind, "trigger") == 0 &&
        lookup(TRIGGER_NAMES, sizeof TRIGGER_NAMES / sizeof TRIGGER_NAMES[0], name, &code)) {
        *out = (EngButtonBinding){ENG_BIND_PAD_TRIGGER, code};
        return true;
    }
    return false;
}

/* ── Lecture d'une ligne ── */

typedef struct Parser {
    EngConfig *config;
    const char *source;
    int line;
    int issues;
} Parser;

static void issue(Parser *parser, const char *fmt, ...) ENG_PRINTF_FORMAT(2, 3);
static void issue(Parser *parser, const char *fmt, ...)
{
    char message[256];
    va_list args;
    va_start(args, fmt);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized): faux positif, args est initialisé
    vsnprintf(message, sizeof message, fmt, args);
    va_end(args);
    ENG_LOG_WARN("%s:%d : %s (ligne ignorée)", parser->source, parser->line, message);
    parser->issues++;
}

static void parse_bind_list(Parser *parser, EngAction action, char *value)
{
    EngConfig *config = parser->config;
    EngButtonBinding parsed[ENG_MAX_BUTTON_BINDINGS] = {0};
    int count = 0;

    if (SDL_strcasecmp(value, "none") != 0) {
        char *cursor = value;
        while (cursor != NULL) {
            char *comma = strchr(cursor, ',');
            if (comma != NULL) {
                *comma = '\0';
            }
            char *token = trim(cursor);
            cursor = comma != NULL ? comma + 1 : NULL;
            if (*token == '\0') {
                continue; /* virgule en trop */
            }
            if (count >= ENG_MAX_BUTTON_BINDINGS) {
                issue(parser, "au plus %d liaisons par action", ENG_MAX_BUTTON_BINDINGS);
                return;
            }
            if (!eng_config_parse_binding(token, &parsed[count])) {
                issue(parser, "liaison illisible « %s »", token);
                return; /* tout ou rien : pas d'action à moitié reliée */
            }
            count++;
        }
        if (count == 0) {
            issue(parser, "aucune liaison (écrire « none » pour désactiver l'action)");
            return;
        }
    }
    memset(config->action_bindings[action], 0, sizeof config->action_bindings[action]);
    memcpy(config->action_bindings[action], parsed, sizeof(EngButtonBinding) * (size_t)count);
    config->action_overridden[action] = true;
}

static void parse_move_keys(Parser *parser, char *value)
{
    int keys[4];
    int count = 0;
    char *cursor = value;
    while (cursor != NULL) {
        char *comma = strchr(cursor, ',');
        if (comma != NULL) {
            *comma = '\0';
        }
        char *token = trim(cursor);
        cursor = comma != NULL ? comma + 1 : NULL;
        SDL_Scancode scancode = SDL_GetScancodeFromName(token);
        if (count >= 4 || scancode == SDL_SCANCODE_UNKNOWN || (int)scancode >= ENG_KEY_CODE_COUNT) {
            issue(parser, "bind.move.keys attend 4 touches : gauche, droite, bas, haut");
            return;
        }
        keys[count++] = (int)scancode;
    }
    if (count != 4) {
        issue(parser, "bind.move.keys attend 4 touches : gauche, droite, bas, haut");
        return;
    }
    memcpy(parser->config->move_keys, keys, sizeof keys);
    parser->config->move_keys_set = true;
}

static void parse_setting(Parser *parser, const char *key, char *value)
{
    EngConfig *config = parser->config;
    bool flag;
    float number;

    if (SDL_strcasecmp(key, "resolution") == 0) {
        char *x = strchr(value, 'x');
        if (x == NULL) {
            x = strchr(value, 'X');
        }
        int width;
        int height;
        if (x == NULL) {
            issue(parser, "resolution attend « LARGEURxHAUTEUR », par exemple 1920x1080");
            return;
        }
        *x = '\0';
        if (!parse_int(trim(value), &width) || !parse_int(trim(x + 1), &height)) {
            issue(parser, "resolution illisible");
            return;
        }
        if (width < WIDTH_MIN || width > SIZE_MAX_PIXELS || height < HEIGHT_MIN ||
            height > SIZE_MAX_PIXELS) {
            issue(parser, "resolution %dx%d hors limites (%dx%d à %dx%d)", width, height, WIDTH_MIN,
                  HEIGHT_MIN, SIZE_MAX_PIXELS, SIZE_MAX_PIXELS);
            return;
        }
        config->window_width = width;
        config->window_height = height;
    } else if (SDL_strcasecmp(key, "fullscreen") == 0) {
        if (!parse_bool(value, &flag)) {
            issue(parser, "fullscreen attend true/false");
            return;
        }
        config->fullscreen = flag;
    } else if (SDL_strcasecmp(key, "vsync") == 0) {
        if (!parse_bool(value, &flag)) {
            issue(parser, "vsync attend true/false");
            return;
        }
        config->vsync = flag;
    } else if (SDL_strcasecmp(key, "mouse_sensitivity") == 0) {
        if (!parse_float(value, &number) || !(number > 0.0F) || number > 0.1F) {
            issue(parser, "mouse_sensitivity attend un nombre dans ]0 ; 0.1]");
            return;
        }
        config->mouse_sensitivity = number;
    } else if (SDL_strcasecmp(key, "stick_deadzone") == 0) {
        if (!parse_float(value, &number) || !(number >= 0.0F) || number > 0.95F) {
            issue(parser, "stick_deadzone attend un nombre dans [0 ; 0.95]");
            return;
        }
        config->stick_deadzone = number;
    } else if (SDL_strcasecmp(key, "bind.move.keys") == 0) {
        parse_move_keys(parser, value);
    } else if (SDL_strncasecmp(key, "bind.", 5) == 0) {
        for (int a = 0; a < ENG_ACTION_COUNT; a++) {
            if (SDL_strcasecmp(key + 5, ACTION_NAMES[a]) == 0) {
                parse_bind_list(parser, (EngAction)a, value);
                return;
            }
        }
        issue(parser, "action inconnue « %s »", key + 5);
    } else {
        issue(parser, "clé inconnue « %s »", key);
    }
}

int eng_config_parse(EngConfig *config, const char *text, const char *source)
{
    Parser parser = {.config = config, .source = source, .line = 0, .issues = 0};

    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF) {
        text += 3; /* BOM UTF-8 ajouté par certains éditeurs Windows */
    }

    while (*text != '\0') {
        const char *newline = strchr(text, '\n');
        size_t length = newline != NULL ? (size_t)(newline - text) : strlen(text);
        parser.line++;

        char line[LINE_MAX_LENGTH];
        if (length >= sizeof line) {
            issue(&parser, "ligne trop longue (plus de %d caractères)", LINE_MAX_LENGTH - 1);
        } else {
            memcpy(line, text, length);
            line[length] = '\0';

            char *comment = strpbrk(line, "#;");
            if (comment != NULL) {
                *comment = '\0';
            }
            char *content = trim(line);
            if (*content != '\0') {
                char *equals = strchr(content, '=');
                if (equals == NULL) {
                    issue(&parser, "« clé = valeur » attendu");
                } else {
                    *equals = '\0';
                    char *key = trim(content);
                    char *value = trim(equals + 1);
                    if (*key == '\0') {
                        issue(&parser, "clé manquante avant « = »");
                    } else if (*value == '\0') {
                        issue(&parser, "valeur manquante pour « %s »", key);
                    } else {
                        parse_setting(&parser, key, value);
                    }
                }
            }
        }
        text += length;
        if (*text == '\n') {
            text++;
        }
    }
    config->issue_count += parser.issues;
    return parser.issues;
}

bool eng_config_load(EngConfig *config, EngArena *scratch, const char *path)
{
    eng_config_defaults(config);

    EngArenaMark mark = eng_arena_mark(scratch);
    EngFileData file;
    bool loaded = false;
    if (eng_file_exists(path) && eng_file_read_all(scratch, path, &file)) {
        if (file.size > ENG_CONFIG_MAX_FILE_SIZE) {
            ENG_LOG_WARN("%s : fichier trop gros (%zu octets) : valeurs par défaut", path,
                         file.size);
        } else {
            (void)eng_config_parse(config, (const char *)file.data, path);
            loaded = true;
        }
    }
    eng_arena_reset_to(scratch, mark);
    return loaded;
}

void eng_config_apply_input(const EngConfig *config, EngInput *input)
{
    for (int a = 0; a < ENG_ACTION_COUNT; a++) {
        if (!config->action_overridden[a]) {
            continue;
        }
        eng_input_clear_action(input, (EngAction)a);
        for (int i = 0; i < ENG_MAX_BUTTON_BINDINGS; i++) {
            const EngButtonBinding *binding = &config->action_bindings[a][i];
            if (binding->kind != ENG_BIND_NONE) {
                (void)eng_input_bind_button(input, (EngAction)a, binding->kind, binding->code);
            }
        }
    }

    EngAxisBinding move = input->axes[ENG_AXIS_MOVE];
    EngAxisBinding look = input->axes[ENG_AXIS_LOOK];
    if (config->move_keys_set) {
        move.key_left = config->move_keys[0];
        move.key_right = config->move_keys[1];
        move.key_down = config->move_keys[2];
        move.key_up = config->move_keys[3];
    }
    move.deadzone = config->stick_deadzone;
    look.deadzone = config->stick_deadzone;
    look.mouse_sensitivity = config->mouse_sensitivity;
    (void)eng_input_bind_axis(input, ENG_AXIS_MOVE, &move);
    (void)eng_input_bind_axis(input, ENG_AXIS_LOOK, &look);
}
