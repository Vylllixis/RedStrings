/* Options du jeu (M8-10) : valeurs par défaut, validation, texte « clé = valeur » enregistré dans
 * options.txt avec les liaisons des touches. La navigation des menus est dans game_menu.c. */
#include "engine/game_menu.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/log.h"

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
    o->ssao = true;
    o->render_scale = 1.0F;
    o->bloom = 0.5F;
    o->vsync = true;
    o->fullscreen = false;
    o->shadow_quality = 2U;
    o->view_distance = 1U;
}

static bool unit(float v)
{
    return isfinite(v) && v >= 0.0F && v <= 1.0F;
}

bool eng_options_valid(const EngGameOptions *o)
{
    if (o == NULL || !unit(o->master) || !isfinite(o->sensitivity) || o->sensitivity <= 0.0F ||
        o->sensitivity > 4.0F || !isfinite(o->text_scale) || o->text_scale < 0.75F ||
        o->text_scale > 2.0F || !unit(o->bloom) || o->shadow_quality > 2U ||
        o->view_distance > 2U) {
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
        const int emitted_ = snprintf(out + n, capacity - n, __VA_ARGS__);                         \
        if (emitted_ < 0 || (size_t)emitted_ >= capacity - n) {                                    \
            out[0] = '\0';                                                                         \
            return 0;                                                                              \
        }                                                                                          \
        n += (size_t)emitted_;                                                                     \
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
    EMIT("graphics.vsync = %d\n", o->vsync ? 1 : 0);
    EMIT("graphics.ssao = %d\n", o->ssao ? 1 : 0);
    EMIT("graphics.render_scale = %.2f\n", (double)o->render_scale);
    EMIT("graphics.fullscreen = %d\n", o->fullscreen ? 1 : 0);
    EMIT("graphics.shadows = %u\n", (unsigned)o->shadow_quality);
    EMIT("graphics.view_distance = %u\n", (unsigned)o->view_distance);
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
            /* une ligne illisible (action, genre ou sens de molette inconnus) est ignorée sans
             * toucher aux liaisons de l'action */
            const bool wheel_ok =
                kind != ENG_BIND_MOUSE_WHEEL || code == ENG_WHEEL_UP || code == ENG_WHEEL_DOWN;
            if (input != NULL && a < ENG_ACTION_COUNT && kind > ENG_BIND_NONE &&
                kind <= ENG_BIND_MOUSE_WHEEL && wheel_ok) {
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
        } else if (strcmp(key, "graphics.ssao") == 0) {
            t.ssao = v != 0.0F;
        } else if (strcmp(key, "graphics.render_scale") == 0) {
            if (v >= 0.5F && v <= 1.0F) { /* hors bornes : ignoré */
                t.render_scale = v;
            }
        } else if (strcmp(key, "graphics.vsync") == 0) {
            t.vsync = v != 0.0F;
        } else if (strcmp(key, "graphics.fullscreen") == 0) {
            t.fullscreen = v != 0.0F;
        } else if (strcmp(key, "graphics.shadows") == 0) {
            t.shadow_quality = v >= 0.0F && v <= 2.0F ? (uint32_t)v : 99U; /* 99 : refusé */
        } else if (strcmp(key, "graphics.view_distance") == 0) {
            t.view_distance = v >= 0.0F && v <= 2.0F ? (uint32_t)v : 99U;
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
