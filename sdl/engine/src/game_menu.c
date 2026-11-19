#include "engine/game_menu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/log.h"

#define NAV_DEAD 0.5F
#define NAV_REPEAT 12U /* ticks (200 ms) entre deux pas quand le stick reste penché */

static const EngAction REMAPPABLE[] = {
    ENG_ACTION_ATTACK,   ENG_ACTION_HEAVY_ATTACK, ENG_ACTION_DODGE,   ENG_ACTION_JUMP,
    ENG_ACTION_INTERACT, ENG_ACTION_SPRINT,       ENG_ACTION_LOCK_ON, ENG_ACTION_PAUSE};
#define REMAP_COUNT (sizeof REMAPPABLE / sizeof REMAPPABLE[0])

const char *eng_action_label(EngAction a)
{
    switch (a) {
    case ENG_ACTION_ATTACK:
        return "Attaque légère";
    case ENG_ACTION_HEAVY_ATTACK:
        return "Attaque lourde";
    case ENG_ACTION_DODGE:
        return "Esquive";
    case ENG_ACTION_JUMP:
        return "Saut";
    case ENG_ACTION_INTERACT:
        return "Garde / interagir";
    case ENG_ACTION_SPRINT:
        return "Sprint";
    case ENG_ACTION_LOCK_ON:
        return "Verrouillage";
    case ENG_ACTION_PAUSE:
        return "Pause";
    default:
        return "?";
    }
}

void eng_options_default(EngGameOptions *o)
{
    if (o == NULL) {
        return;
    }
    memset(o, 0, sizeof *o);
    o->master = 0.8F;
    for (uint32_t b = 0; b < ENG_BUS_COUNT; b++) {
        o->volume[b] = 1.0F;
    }
    o->volume[ENG_BUS_MUSIC] = 0.6F;
    o->sensitivity = 1.0F;
    o->text_scale = 1.0F;
    o->fxaa = true;
    o->bloom = 0.5F;
}

static bool unit(float v)
{
    return isfinite(v) && v >= 0.0F && v <= 1.0F;
}

bool eng_options_valid(const EngGameOptions *o)
{
    if (o == NULL || !unit(o->master) || !isfinite(o->sensitivity) || o->sensitivity <= 0.0F ||
        o->sensitivity > 4.0F || !isfinite(o->text_scale) || o->text_scale < 0.75F ||
        o->text_scale > 2.0F || !unit(o->bloom)) {
        return false;
    }
    for (uint32_t b = 0; b < ENG_BUS_COUNT; b++) {
        if (!unit(o->volume[b])) {
            return false;
        }
    }
    return true;
}

static const char *const BUS_KEYS[ENG_BUS_COUNT] = {"volume.sfx", "volume.music", "volume.ambience",
                                                    "volume.ui"};

size_t eng_options_save(const EngGameOptions *o, const EngInput *input, char *out, size_t capacity)
{
    if (o == NULL || out == NULL || capacity == 0U) {
        return 0;
    }
    size_t n = 0;
#define EMIT(...)                                                                                  \
    do {                                                                                           \
        const int k = snprintf(out + n, capacity - n, __VA_ARGS__);                                \
        if (k < 0 || (size_t)k >= capacity - n) {                                                  \
            out[0] = '\0';                                                                         \
            return 0;                                                                              \
        }                                                                                          \
        n += (size_t)k;                                                                            \
    } while (0)
    EMIT("# options du jeu (écrites par le menu Options)\n");
    EMIT("volume.master = %.3f\n", (double)o->master);
    for (uint32_t b = 0; b < ENG_BUS_COUNT; b++) {
        EMIT("%s = %.3f\n", BUS_KEYS[b], (double)o->volume[b]);
    }
    EMIT("camera.sensitivity = %.3f\n", (double)o->sensitivity);
    EMIT("camera.invert_y = %d\n", o->invert_y ? 1 : 0);
    EMIT("ui.text_scale = %.3f\n", (double)o->text_scale);
    EMIT("ui.high_contrast = %d\n", o->high_contrast ? 1 : 0);
    EMIT("graphics.fxaa = %d\n", o->fxaa ? 1 : 0);
    EMIT("graphics.bloom = %.3f\n", (double)o->bloom);
    if (input != NULL) {
        for (uint32_t a = 0; a < ENG_ACTION_COUNT; a++) {
            for (uint32_t k = 0; k < ENG_MAX_BUTTON_BINDINGS; k++) {
                const EngButtonBinding *b = &input->buttons[a][k];
                if (b->kind != ENG_BIND_NONE) {
                    EMIT("bind %u %d %d\n", a, (int)b->kind, b->code);
                }
            }
        }
    }
#undef EMIT
    return n;
}

bool eng_options_load(EngGameOptions *o, EngInput *input, const char *text)
{
    if (o == NULL || text == NULL) {
        return false;
    }
    bool cleared[ENG_ACTION_COUNT] = {false};
    const char *p = text;
    while (*p != '\0') {
        char line[160];
        size_t n = 0;
        while (*p != '\0' && *p != '\n' && n + 1U < sizeof line) {
            line[n++] = *p++;
        }
        while (*p != '\0' && *p != '\n') {
            p++;
        }
        if (*p == '\n') {
            p++;
        }
        line[n] = '\0';
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        unsigned a = 0;
        int kind = 0;
        int code = 0;
        if (sscanf(line, "bind %u %d %d", &a, &kind, &code) == 3) {
            if (input != NULL && a < ENG_ACTION_COUNT && kind > ENG_BIND_NONE &&
                kind <= ENG_BIND_PAD_TRIGGER) {
                if (!cleared[a]) {
                    eng_input_clear_action(input, (EngAction)a);
                    cleared[a] = true;
                }
                (void)eng_input_bind_button(input, (EngAction)a, (EngBindKind)kind, code);
            }
            continue;
        }
        char key[48];
        float v = 0.0F;
        if (sscanf(line, "%47s = %f", key, &v) != 2 || !isfinite(v)) {
            ENG_LOG_WARN("options : ligne ignorée « %s »", line);
            continue;
        }
        EngGameOptions t = *o;
        if (strcmp(key, "volume.master") == 0) {
            t.master = v;
        } else if (strcmp(key, "camera.sensitivity") == 0) {
            t.sensitivity = v;
        } else if (strcmp(key, "camera.invert_y") == 0) {
            t.invert_y = v != 0.0F;
        } else if (strcmp(key, "ui.text_scale") == 0) {
            t.text_scale = v;
        } else if (strcmp(key, "ui.high_contrast") == 0) {
            t.high_contrast = v != 0.0F;
        } else if (strcmp(key, "graphics.fxaa") == 0) {
            t.fxaa = v != 0.0F;
        } else if (strcmp(key, "graphics.bloom") == 0) {
            t.bloom = v;
        } else {
            bool found = false;
            for (uint32_t b = 0; b < ENG_BUS_COUNT; b++) {
                if (strcmp(key, BUS_KEYS[b]) == 0) {
                    t.volume[b] = v;
                    found = true;
                }
            }
            if (!found) {
                ENG_LOG_WARN("options : clé inconnue « %s »", key);
                continue;
            }
        }
        if (eng_options_valid(&t)) {
            *o = t;
        } else {
            ENG_LOG_WARN("options : valeur hors bornes ignorée « %s »", line);
        }
    }
    return true;
}

void eng_menu_init(EngMenu *m, EngGameOptions *options, EngInput *input)
{
    if (m == NULL) {
        return;
    }
    memset(m, 0, sizeof *m);
    m->options = options;
    m->input = input;
    m->remap = -1;
}

void eng_menu_open(EngMenu *m, EngMenuScreen screen)
{
    if (m == NULL || (unsigned)screen >= (unsigned)ENG_MENU_SCREEN_COUNT) {
        return;
    }
    if (screen == ENG_MENU_OPTIONS && m->screen != ENG_MENU_OPTIONS &&
        m->screen != ENG_MENU_CONTROLS) {
        m->options_return = m->screen;
    }
    m->screen = screen;
    m->selected = 0;
    m->remap = -1;
    m->nav_cooldown = 0;
    m->nav_prev_x = 0.0F;
    m->nav_prev_y = 0.0F;
}

bool eng_menu_is_open(const EngMenu *m)
{
    return m != NULL && m->screen != ENG_MENU_CLOSED;
}

void eng_menu_set_status(EngMenu *m, const char *text)
{
    if (m != NULL && text != NULL) {
        (void)snprintf(m->status, sizeof m->status, "%s", text);
        m->status_ticks = 180U;
    }
}

const char *eng_menu_title(const EngMenu *m)
{
    static const char *const titles[ENG_MENU_SCREEN_COUNT] = {"", "RedStrings", "Pause", "Options",
                                                              "Contrôles"};
    return m != NULL && (unsigned)m->screen < (unsigned)ENG_MENU_SCREEN_COUNT ? titles[m->screen]
                                                                              : "";
}

static void set_item(EngMenuItem *it, const char *label, const char *value, bool enabled)
{
    (void)snprintf(it->label, sizeof it->label, "%s", label);
    (void)snprintf(it->value, sizeof it->value, "%s", value != NULL ? value : "");
    it->enabled = enabled;
}

static void binding_text(const EngInput *in, EngAction a, char *out, size_t cap)
{
    out[0] = '\0';
    if (in == NULL) {
        return;
    }
    size_t n = 0;
    for (uint32_t k = 0; k < ENG_MAX_BUTTON_BINDINGS; k++) {
        const EngButtonBinding *b = &in->buttons[a][k];
        if (b->kind == ENG_BIND_NONE) {
            continue;
        }
        const char *kind = b->kind == ENG_BIND_KEY            ? "touche"
                           : b->kind == ENG_BIND_MOUSE_BUTTON ? "souris"
                           : b->kind == ENG_BIND_PAD_BUTTON   ? "manette"
                                                              : "gâchette";
        const int k2 = snprintf(out + n, cap - n, "%s%s %d", n > 0U ? ", " : "", kind, b->code);
        if (k2 < 0 || (size_t)k2 >= cap - n) {
            break;
        }
        n += (size_t)k2;
    }
}

uint32_t eng_menu_items(const EngMenu *m, EngMenuItem *out, uint32_t max)
{
    if (m == NULL || out == NULL || max < ENG_MENU_MAX_ITEMS) {
        return 0;
    }
    uint32_t n = 0;
    char v[ENG_MENU_LABEL];
    const EngGameOptions *o = m->options;
    switch (m->screen) {
    case ENG_MENU_MAIN:
        set_item(&out[n++], "Continuer", NULL, m->has_save);
        set_item(&out[n++], "Nouvelle partie", NULL, true);
        set_item(&out[n++], "Options", NULL, o != NULL);
        set_item(&out[n++], "Quitter", NULL, true);
        break;
    case ENG_MENU_PAUSE:
        set_item(&out[n++], "Reprendre", NULL, true);
        set_item(&out[n++], "Sauvegarder", NULL, true);
        set_item(&out[n++], "Options", NULL, o != NULL);
        set_item(&out[n++], "Menu principal", NULL, true);
        break;
    case ENG_MENU_OPTIONS: {
        static const char *const names[ENG_BUS_COUNT] = {
            "Volume des effets", "Volume de la musique", "Volume de l'ambiance",
            "Volume de l'interface"};
        (void)snprintf(v, sizeof v, "%d %%", (int)lroundf(o->master * 100.0F));
        set_item(&out[n++], "Volume général", v, true);
        for (uint32_t b = 0; b < ENG_BUS_COUNT; b++) {
            (void)snprintf(v, sizeof v, "%d %%", (int)lroundf(o->volume[b] * 100.0F));
            set_item(&out[n++], names[b], v, true);
        }
        (void)snprintf(v, sizeof v, "%.1f", (double)o->sensitivity);
        set_item(&out[n++], "Sensibilité de la caméra", v, true);
        set_item(&out[n++], "Inverser l'axe vertical", o->invert_y ? "oui" : "non", true);
        (void)snprintf(v, sizeof v, "%d %%", (int)lroundf(o->text_scale * 100.0F));
        set_item(&out[n++], "Taille du texte", v, true);
        set_item(&out[n++], "Contraste élevé", o->high_contrast ? "oui" : "non", true);
        set_item(&out[n++], "Anticrénelage (FXAA)", o->fxaa ? "oui" : "non", true);
        (void)snprintf(v, sizeof v, "%d %%", (int)lroundf(o->bloom * 100.0F));
        set_item(&out[n++], "Halo lumineux (bloom)", v, true);
        set_item(&out[n++], "Contrôles...", NULL, m->input != NULL);
        set_item(&out[n++], "Retour", NULL, true);
        break;
    }
    case ENG_MENU_CONTROLS:
        for (uint32_t i = 0; i < REMAP_COUNT; i++) {
            if (m->remap == (int32_t)i) {
                set_item(&out[n++], eng_action_label(REMAPPABLE[i]), "appuie sur une touche...",
                         true);
            } else {
                binding_text(m->input, REMAPPABLE[i], v, sizeof v);
                set_item(&out[n++], eng_action_label(REMAPPABLE[i]), v, true);
            }
        }
        set_item(&out[n++], "Par défaut", NULL, true);
        set_item(&out[n++], "Retour", NULL, true);
        break;
    default:
        break;
    }
    return n;
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* réglage gauche / droite d'une ligne d'Options ; true si une valeur a changé */
static bool adjust(EngMenu *m, int dir)
{
    EngGameOptions *o = m->options;
    const float step = 0.1F * (float)dir;
    const int32_t s = m->selected;
    if (s == 0) {
        o->master = clampf(roundf((o->master + step) * 10.0F) / 10.0F, 0.0F, 1.0F);
    } else if (s >= 1 && s <= (int32_t)ENG_BUS_COUNT) {
        float *v = &o->volume[s - 1];
        *v = clampf(roundf((*v + step) * 10.0F) / 10.0F, 0.0F, 1.0F);
    } else if (s == (int32_t)ENG_BUS_COUNT + 1) {
        o->sensitivity = clampf(roundf((o->sensitivity + step) * 10.0F) / 10.0F, 0.1F, 4.0F);
    } else if (s == (int32_t)ENG_BUS_COUNT + 2) {
        o->invert_y = !o->invert_y;
    } else if (s == (int32_t)ENG_BUS_COUNT + 3) {
        o->text_scale =
            clampf(roundf((o->text_scale + 0.25F * (float)dir) * 4.0F) / 4.0F, 0.75F, 2.0F);
    } else if (s == (int32_t)ENG_BUS_COUNT + 4) {
        o->high_contrast = !o->high_contrast;
    } else if (s == (int32_t)ENG_BUS_COUNT + 5) {
        o->fxaa = !o->fxaa;
    } else if (s == (int32_t)ENG_BUS_COUNT + 6) {
        o->bloom = clampf(roundf((o->bloom + step) * 10.0F) / 10.0F, 0.0F, 1.0F);
    } else {
        return false;
    }
    return true;
}

static EngMenuAction confirm(EngMenu *m, const EngMenuItem *items, uint32_t n)
{
    if (m->selected < 0 || (uint32_t)m->selected >= n || !items[m->selected].enabled) {
        return ENG_MENU_ACT_NONE;
    }
    const int32_t s = m->selected;
    switch (m->screen) {
    case ENG_MENU_MAIN:
        if (s == 0) {
            m->screen = ENG_MENU_CLOSED;
            return ENG_MENU_ACT_CONTINUE;
        }
        if (s == 1) {
            m->screen = ENG_MENU_CLOSED;
            return ENG_MENU_ACT_NEW_GAME;
        }
        if (s == 2) {
            eng_menu_open(m, ENG_MENU_OPTIONS);
            return ENG_MENU_ACT_NONE;
        }
        return ENG_MENU_ACT_QUIT;
    case ENG_MENU_PAUSE:
        if (s == 0) {
            m->screen = ENG_MENU_CLOSED;
            return ENG_MENU_ACT_RESUME;
        }
        if (s == 1) {
            return ENG_MENU_ACT_SAVE;
        }
        if (s == 2) {
            eng_menu_open(m, ENG_MENU_OPTIONS);
            return ENG_MENU_ACT_NONE;
        }
        eng_menu_open(m, ENG_MENU_MAIN);
        return ENG_MENU_ACT_TO_MAIN;
    case ENG_MENU_OPTIONS:
        if (s == (int32_t)n - 2) {
            eng_menu_open(m, ENG_MENU_CONTROLS);
            return ENG_MENU_ACT_NONE;
        }
        if (s == (int32_t)n - 1) {
            eng_menu_open(m, m->options_return != ENG_MENU_CLOSED ? m->options_return
                                                                  : ENG_MENU_PAUSE);
            return ENG_MENU_ACT_NONE;
        }
        return adjust(m, 1) ? ENG_MENU_ACT_OPTIONS_CHANGED : ENG_MENU_ACT_NONE;
    case ENG_MENU_CONTROLS:
        if (s < (int32_t)REMAP_COUNT) {
            m->remap = s;
            m->remap_serial = m->input != NULL ? m->input->last_serial : 0U;
            return ENG_MENU_ACT_NONE;
        }
        if (s == (int32_t)REMAP_COUNT) {
            if (m->input != NULL) {
                eng_input_set_default_bindings(m->input);
            }
            eng_menu_set_status(m, "Contrôles par défaut rétablis");
            return ENG_MENU_ACT_REMAPPED;
        }
        eng_menu_open(m, ENG_MENU_OPTIONS);
        m->selected = (int32_t)ENG_BUS_COUNT + 7; /* revient sur « Contrôles... » */
        return ENG_MENU_ACT_NONE;
    default:
        return ENG_MENU_ACT_NONE;
    }
}

EngMenuAction eng_menu_update(EngMenu *m, const EngInputState *in)
{
    if (m == NULL || in == NULL) {
        return ENG_MENU_ACT_NONE;
    }
    if (m->status_ticks > 0U) {
        m->status_ticks--;
        if (m->status_ticks == 0U) {
            m->status[0] = '\0';
        }
    }
    if (m->screen == ENG_MENU_CLOSED) {
        if (eng_action_pressed(in, ENG_ACTION_PAUSE)) {
            eng_menu_open(m, ENG_MENU_PAUSE);
        }
        return ENG_MENU_ACT_NONE;
    }
    /* remappage : la prochaine pression (autre que celle qui a validé) devient la liaison */
    if (m->remap >= 0) {
        if (m->input != NULL && m->input->last_serial != m->remap_serial) {
            const EngAction a = REMAPPABLE[m->remap];
            eng_input_clear_action(m->input, a);
            (void)eng_input_bind_button(m->input, a, m->input->last_kind, m->input->last_code);
            m->remap = -1;
            eng_menu_set_status(m, "Touche enregistrée");
            return ENG_MENU_ACT_REMAPPED;
        }
        return ENG_MENU_ACT_NONE;
    }
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    const uint32_t n = eng_menu_items(m, items, ENG_MENU_MAX_ITEMS);
    if (n > 0U && (m->selected < 0 || (uint32_t)m->selected >= n || !items[m->selected].enabled)) {
        for (uint32_t k = 0; k < n;
             k++) { /* jamais sur une ligne grisée (« Continuer » sans sauvegarde) */
            if (items[k].enabled) {
                m->selected = (int32_t)k;
                break;
            }
        }
    }
    if (eng_action_pressed(in, ENG_ACTION_PAUSE)) {
        if (m->screen == ENG_MENU_PAUSE) {
            m->screen = ENG_MENU_CLOSED;
            return ENG_MENU_ACT_RESUME;
        }
        if (m->screen == ENG_MENU_OPTIONS || m->screen == ENG_MENU_CONTROLS) {
            eng_menu_open(m, ENG_MENU_PAUSE);
            return ENG_MENU_ACT_NONE;
        }
    }
    if (eng_action_pressed(in, ENG_ACTION_DODGE)) { /* retour */
        switch (m->screen) {
        case ENG_MENU_PAUSE:
            m->screen = ENG_MENU_CLOSED;
            return ENG_MENU_ACT_RESUME;
        case ENG_MENU_OPTIONS:
            eng_menu_open(m, m->options_return != ENG_MENU_CLOSED ? m->options_return
                                                                  : ENG_MENU_PAUSE);
            return ENG_MENU_ACT_NONE;
        case ENG_MENU_CONTROLS:
            eng_menu_open(m, ENG_MENU_OPTIONS);
            return ENG_MENU_ACT_NONE;
        default:
            return ENG_MENU_ACT_NONE;
        }
    }
    /* navigation : un pas par penchée du stick, puis répétition toutes les 200 ms */
    const float y = in->axis[ENG_AXIS_MOVE].y;
    const float x = in->axis[ENG_AXIS_MOVE].x;
    const bool fresh_y = fabsf(y) > NAV_DEAD && fabsf(m->nav_prev_y) <= NAV_DEAD;
    const bool fresh_x = fabsf(x) > NAV_DEAD && fabsf(m->nav_prev_x) <= NAV_DEAD;
    m->nav_prev_y = y;
    m->nav_prev_x = x;
    if (m->nav_cooldown > 0U) {
        m->nav_cooldown--;
    }
    EngMenuAction act = ENG_MENU_ACT_NONE;
    if (n > 0U && fabsf(y) > NAV_DEAD && (fresh_y || m->nav_cooldown == 0U)) {
        const int dir = y > 0.0F ? -1 : 1; /* avant (haut) = ligne précédente */
        for (uint32_t tries = 0; tries < n; tries++) {
            m->selected = (int32_t)(((uint32_t)(m->selected + dir + (int32_t)n)) % n);
            if (items[m->selected].enabled) {
                break;
            }
        }
        m->nav_cooldown = NAV_REPEAT;
    } else if (m->screen == ENG_MENU_OPTIONS && fabsf(x) > NAV_DEAD &&
               (fresh_x || m->nav_cooldown == 0U)) {
        if (adjust(m, x > 0.0F ? 1 : -1)) {
            act = ENG_MENU_ACT_OPTIONS_CHANGED;
        }
        m->nav_cooldown = NAV_REPEAT;
    }
    if (act == ENG_MENU_ACT_NONE &&
        (eng_action_pressed(in, ENG_ACTION_JUMP) || eng_action_pressed(in, ENG_ACTION_ATTACK))) {
        act = confirm(m, items, n);
    }
    return act;
}
