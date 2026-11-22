/* Lignes de chaque écran des menus et noms lisibles des touches. Voir game_menu.h. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "game_menu_internal.h"

static const EngAction REMAPPABLE[] = {
    ENG_ACTION_ATTACK,  ENG_ACTION_HEAVY_ATTACK, ENG_ACTION_DODGE,
    ENG_ACTION_JUMP,    ENG_ACTION_INTERACT,     ENG_ACTION_SPRINT,
    ENG_ACTION_LOCK_ON, ENG_ACTION_PAUSE,        ENG_ACTION_SWITCH_WEAPON};
#define REMAP_COUNT (sizeof REMAPPABLE / sizeof REMAPPABLE[0])

uint32_t eng_menu_remap_count(void)
{
    return (uint32_t)REMAP_COUNT;
}

EngAction eng_menu_remap_action(uint32_t index)
{
    return REMAPPABLE[index < REMAP_COUNT ? index : 0U];
}

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
    case ENG_ACTION_SWITCH_WEAPON:
        return "Changer d'arme";
    case ENG_ACTION_PAUSE:
        return "Pause";
    default:
        return "?";
    }
}

/* ── noms des touches ── */

static const char *special_key(int scancode)
{
    switch (scancode) {
    case SDL_SCANCODE_SPACE:
        return "Espace";
    case SDL_SCANCODE_LCTRL:
        return "Ctrl gauche";
    case SDL_SCANCODE_RCTRL:
        return "Ctrl droit";
    case SDL_SCANCODE_LSHIFT:
        return "Maj gauche";
    case SDL_SCANCODE_RSHIFT:
        return "Maj droite";
    case SDL_SCANCODE_LALT:
        return "Alt gauche";
    case SDL_SCANCODE_RALT:
        return "Alt droit";
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        return "Entrée";
    case SDL_SCANCODE_ESCAPE:
        return "Échap";
    case SDL_SCANCODE_TAB:
        return "Tab";
    case SDL_SCANCODE_BACKSPACE:
        return "Retour arrière";
    case SDL_SCANCODE_UP:
        return "Flèche haut";
    case SDL_SCANCODE_DOWN:
        return "Flèche bas";
    case SDL_SCANCODE_LEFT:
        return "Flèche gauche";
    case SDL_SCANCODE_RIGHT:
        return "Flèche droite";
    default:
        return NULL;
    }
}

static const char *pad_button(int code)
{
    static const char *const names[] = {
        "manette A",  "manette B",  "manette X",    "manette Y",    "Select",
        "Guide",      "Start",      "clic stick G", "clic stick D", "manette LB",
        "manette RB", "croix haut", "croix bas",    "croix gauche", "croix droite"};
    return code >= 0 && code < (int)(sizeof names / sizeof names[0]) ? names[code] : NULL;
}

void eng_binding_name(EngBindKind kind, int code, char *out, size_t capacity)
{
    if (out == NULL || capacity == 0U) {
        return;
    }
    const char *name = NULL;
    switch (kind) {
    case ENG_BIND_KEY:
        name = special_key(code);
        if (name == NULL && code > 0 && code < SDL_SCANCODE_COUNT) {
            /* la lettre de la disposition du clavier (AZERTY : « Z » pour la position de W) */
            const SDL_Keycode key =
                SDL_GetKeyFromScancode((SDL_Scancode)code, SDL_KMOD_NONE, false);
            const char *sdl_name = key != SDLK_UNKNOWN ? SDL_GetKeyName(key) : "";
            if (sdl_name == NULL || sdl_name[0] == '\0') {
                sdl_name = SDL_GetScancodeName((SDL_Scancode)code);
            }
            if (sdl_name != NULL && sdl_name[0] != '\0') {
                (void)snprintf(out, capacity, "%s", sdl_name);
                return;
            }
        }
        if (name == NULL) {
            (void)snprintf(out, capacity, "touche %d", code);
            return;
        }
        break;
    case ENG_BIND_MOUSE_BUTTON:
        name = code == SDL_BUTTON_LEFT     ? "clic gauche"
               : code == SDL_BUTTON_RIGHT  ? "clic droit"
               : code == SDL_BUTTON_MIDDLE ? "clic molette"
                                           : NULL;
        if (name == NULL) {
            (void)snprintf(out, capacity, "souris %d", code);
            return;
        }
        break;
    case ENG_BIND_PAD_BUTTON:
        name = pad_button(code);
        if (name == NULL) {
            (void)snprintf(out, capacity, "manette %d", code);
            return;
        }
        break;
    case ENG_BIND_PAD_TRIGGER:
        name = code == ENG_PAD_AXIS_LEFT_TRIGGER ? "gâchette gauche" : "gâchette droite";
        break;
    default:
        name = "";
        break;
    }
    (void)snprintf(out, capacity, "%s", name);
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
        char name[32];
        eng_binding_name(b->kind, b->code, name, sizeof name);
        const int k2 = snprintf(out + n, cap - n, "%s%s", n > 0U ? ", " : "", name);
        if (k2 < 0 || (size_t)k2 >= cap - n) {
            out[n] = '\0'; /* plus de place : on s'arrête à la liaison précédente */
            break;
        }
        n += (size_t)k2;
    }
}

/* ── lignes ── */

static EngMenuItem *add(EngMenuItem *out, uint32_t *n, const char *label, const char *value,
                        bool enabled)
{
    EngMenuItem *it = &out[(*n)++];
    (void)snprintf(it->label, sizeof it->label, "%s", label);
    (void)snprintf(it->value, sizeof it->value, "%s", value != NULL ? value : "");
    it->enabled = enabled;
    it->kind = ENG_MENU_ITEM_PLAIN;
    it->frac = 0.0F;
    return it;
}

static float unit(float v)
{
    return v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v);
}

static void add_slider(EngMenuItem *out, uint32_t *n, const char *label, const char *value,
                       float frac)
{
    EngMenuItem *it = add(out, n, label, value, true);
    it->kind = ENG_MENU_ITEM_SLIDER;
    it->frac = unit(frac);
}

static void add_toggle(EngMenuItem *out, uint32_t *n, const char *label, bool on)
{
    EngMenuItem *it = add(out, n, label, on ? "oui" : "non", true);
    it->kind = ENG_MENU_ITEM_TOGGLE;
    it->frac = on ? 1.0F : 0.0F;
}

static void add_choice(EngMenuItem *out, uint32_t *n, const char *label, const char *value,
                       float frac)
{
    EngMenuItem *it = add(out, n, label, value, true);
    it->kind = ENG_MENU_ITEM_CHOICE;
    it->frac = unit(frac);
}

static uint32_t used_slots(const EngMenu *m)
{
    uint32_t used = 0;
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        used += m->slots[i].used ? 1U : 0U;
    }
    return used;
}

static void options_items(const EngMenu *m, EngMenuItem *out, uint32_t *n)
{
    static const char *const names[ENG_BUS_COUNT] = {"Volume des effets", "Volume de la musique",
                                                     "Volume de l'ambiance",
                                                     "Volume de l'interface"};
    static const char *const quality[3] = {"bas", "moyen", "haut"};
    static const char *const distance[3] = {"proche", "moyenne", "loin"};
    const EngGameOptions *o = m->options;
    char v[ENG_MENU_LABEL];
    (void)snprintf(v, sizeof v, "%d %%", (int)lroundf(o->master * 100.0F));
    add_slider(out, n, "Volume général", v, o->master);
    for (uint32_t b = 0; b < ENG_BUS_COUNT; b++) {
        (void)snprintf(v, sizeof v, "%d %%", (int)lroundf(o->volume[b] * 100.0F));
        add_slider(out, n, names[b], v, o->volume[b]);
    }
    (void)snprintf(v, sizeof v, "%.1f", (double)o->sensitivity);
    add_slider(out, n, "Sensibilité de la caméra", v, (o->sensitivity - 0.1F) / 3.9F);
    add_toggle(out, n, "Inverser l'axe vertical", o->invert_y);
    (void)snprintf(v, sizeof v, "%d %%", (int)lroundf(o->text_scale * 100.0F));
    add_slider(out, n, "Taille du texte", v, (o->text_scale - 0.75F) / 1.25F);
    add_toggle(out, n, "Contraste élevé", o->high_contrast);
    add_toggle(out, n, "Anticrénelage (FXAA)", o->fxaa);
    (void)snprintf(v, sizeof v, "%d %%", (int)lroundf(o->bloom * 100.0F));
    add_slider(out, n, "Halo lumineux (bloom)", v, o->bloom);
    add_toggle(out, n, "Synchronisation verticale", o->vsync);
    add_toggle(out, n, "Plein écran", o->fullscreen);
    add_choice(out, n, "Qualité des ombres", quality[o->shadow_quality % 3U],
               (float)(o->shadow_quality % 3U) / 2.0F);
    add_choice(out, n, "Distance de vue", distance[o->view_distance % 3U],
               (float)(o->view_distance % 3U) / 2.0F);
    add_toggle(out, n, "Occlusion ambiante (SSAO)", o->ssao);
    (void)snprintf(v, sizeof v, "%d %%", (int)lroundf(o->render_scale * 100.0F));
    add_slider(out, n, "Échelle de résolution", v, (o->render_scale - 0.5F) / 0.5F);
    (void)add(out, n, "Contrôles...", NULL, m->input != NULL);
    (void)add(out, n, "Retour", NULL, true);
}

uint32_t eng_menu_items(const EngMenu *m, EngMenuItem *out, uint32_t max)
{
    if (m == NULL || out == NULL || max < ENG_MENU_MAX_ITEMS) {
        return 0;
    }
    uint32_t n = 0;
    char v[ENG_MENU_LABEL];
    const int32_t slot = eng_menu_valid_slot(m->slot);
    switch (m->screen) {
    case ENG_MENU_MAIN: {
        const uint32_t used = used_slots(m);
        if (m->has_save) { /* la partie que « Continuer » reprend */
            const EngMenuSlot *last = &m->slots[eng_menu_valid_slot(m->last_slot)];
            (void)snprintf(v, sizeof v, "%.31s%s%.24s", last->name,
                           last->summary[0] != '\0' ? " · " : "", last->summary);
            (void)add(out, &n, "Continuer", v, true);
        } else {
            (void)add(out, &n, "Nouvelle partie", "Commencer l'aventure", true);
        }
        if (used == 0U) {
            (void)snprintf(v, sizeof v, "Aucune partie enregistrée");
        } else {
            (void)snprintf(v, sizeof v, "%u partie%s sur %u", (unsigned)used, used > 1U ? "s" : "",
                           (unsigned)ENG_MENU_SLOTS);
        }
        (void)add(out, &n, "Parties", v, true);
        (void)add(out, &n, "Options", "Son, image, commandes", m->options != NULL);
        (void)add(out, &n, "Quitter", "Fermer le jeu", true);
        break;
    }
    case ENG_MENU_SAVES:
        for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
            const EngMenuSlot *s = &m->slots[i];
            if (s->used) {
                (void)add(out, &n, s->name, s->unreadable ? "illisible" : s->summary, true);
            } else {
                (void)add(out, &n, "Emplacement libre", "Créer une partie", true);
            }
        }
        (void)add(out, &n, "Retour", NULL, true);
        break;
    case ENG_MENU_SLOT: {
        const EngMenuSlot *s = &m->slots[slot];
        const int32_t free_slot = eng_menu_free_slot(m);
        const bool playable = s->used && !s->unreadable;
        (void)add(out, &n, "Jouer", NULL, playable);
        (void)add(out, &n, "Renommer", NULL, playable);
        if (free_slot >= 0) {
            (void)snprintf(v, sizeof v, "vers l'emplacement %d", (int)free_slot + 1);
        } else {
            (void)snprintf(v, sizeof v, "aucun emplacement libre");
        }
        (void)add(out, &n, "Dupliquer", v, playable && free_slot >= 0);
        (void)add(out, &n, "Supprimer", NULL, s->used);
        (void)add(out, &n, "Retour", NULL, true);
        break;
    }
    case ENG_MENU_NAME:
        if (m->name_purpose == ENG_MENU_NAME_CREATE) { /* choisi ici, pour toute la partie */
            add_choice(out, &n, "Mode de jeu", m->creative ? "Créatif" : "Aventure",
                       m->creative ? 1.0F : 0.0F);
        }
        (void)add(out, &n, m->name_purpose == ENG_MENU_NAME_RENAME ? "Renommer" : "Créer la partie",
                  NULL, true);
        (void)add(out, &n, "Annuler", NULL, true);
        break;
    case ENG_MENU_DELETE:
        (void)add(out, &n, "Non, garder la partie", NULL, true);
        (void)add(out, &n, "Oui, supprimer", NULL, true);
        break;
    case ENG_MENU_PAUSE:
        (void)add(out, &n, "Reprendre", NULL, true);
        (void)add(out, &n, "Sauvegarder", NULL, true);
        (void)add(out, &n, "Options", NULL, m->options != NULL);
        (void)add(out, &n, "Sauvegarder et quitter", NULL, true);
        break;
    case ENG_MENU_OPTIONS:
        if (m->options != NULL) {
            options_items(m, out, &n);
        }
        break;
    case ENG_MENU_CONTROLS:
        for (uint32_t i = 0; i < REMAP_COUNT; i++) {
            if (m->remap == (int32_t)i) {
                (void)add(out, &n, eng_action_label(REMAPPABLE[i]), "appuie sur une touche...",
                          true);
            } else {
                binding_text(m->input, REMAPPABLE[i], v, sizeof v);
                (void)add(out, &n, eng_action_label(REMAPPABLE[i]), v, true);
            }
        }
        (void)add(out, &n, "Par défaut", NULL, true);
        (void)add(out, &n, "Retour", NULL, true);
        break;
    default:
        break;
    }
    return n;
}
