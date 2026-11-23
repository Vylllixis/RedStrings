/* Navigation des menus du jeu : voir game_menu.h. Les lignes de chaque écran sont dans
 * game_menu_items.c, les options dans game_options.c. */
#include "engine/game_menu.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game_menu_internal.h"

#define NAV_DEAD 0.5F
#define NAV_REPEAT 12U /* ticks (200 ms) entre deux pas quand la direction reste tenue */

void eng_menu_init(EngMenu *m, EngGameOptions *options, EngInput *input)
{
    if (m == NULL) {
        return;
    }
    memset(m, 0, sizeof *m);
    m->options = options;
    m->input = input;
    m->remap = -1;
    m->hover_item = -1;
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
    m->hover_item = -1;
    m->hover_dir = 0;
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
        m->status_serial++;
    }
}

const char *eng_menu_title(const EngMenu *m)
{
    static const char *const titles[ENG_MENU_SCREEN_COUNT] = {
        "",        "Red Strings",      "Pause",
        "Options", "Contrôles",        "Parties",
        "Partie",  "Nom de la partie", "Supprimer cette partie ?"};
    if (m == NULL || (unsigned)m->screen >= (unsigned)ENG_MENU_SCREEN_COUNT) {
        return "";
    }
    if (m->screen == ENG_MENU_NAME && m->name_purpose == ENG_MENU_NAME_RENAME) {
        return "Renommer la partie";
    }
    return titles[m->screen];
}

void eng_menu_set_slot(EngMenu *m, uint32_t index, const EngMenuSlot *slot)
{
    if (m == NULL || index >= ENG_MENU_SLOTS) {
        return;
    }
    if (slot == NULL || !slot->used) {
        memset(&m->slots[index], 0, sizeof m->slots[index]);
        return;
    }
    m->slots[index] = *slot;
    m->slots[index].name[sizeof m->slots[index].name - 1U] = '\0';
    m->slots[index].summary[sizeof m->slots[index].summary - 1U] = '\0';
    m->slots[index].place[sizeof m->slots[index].place - 1U] = '\0';
    m->slots[index].date[sizeof m->slots[index].date - 1U] = '\0';
}

void eng_menu_default_name(uint32_t index, char *out, size_t capacity)
{
    if (out != NULL && capacity > 0U) {
        (void)snprintf(out, capacity, "Partie %u", (unsigned)(index + 1U));
    }
}

int32_t eng_menu_valid_slot(int32_t slot)
{
    return slot < 0 ? 0 : (slot >= (int32_t)ENG_MENU_SLOTS ? (int32_t)ENG_MENU_SLOTS - 1 : slot);
}

int32_t eng_menu_free_slot(const EngMenu *m)
{
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        if (!m->slots[i].used) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* ── saisie du nom ── */

static uint32_t utf8_chars(const char *s)
{
    uint32_t n = 0;
    for (; *s != '\0'; s++) {
        n += ((unsigned char)*s & 0xC0U) != 0x80U ? 1U : 0U;
    }
    return n;
}

/* longueur (1 à 4) du caractère UTF-8 valide qui commence en `s`, 0 s'il est invalide */
static uint32_t utf8_length(const char *s)
{
    const unsigned char c = (unsigned char)s[0];
    const uint32_t len = c < 0x80U           ? 1U
                         : (c >> 5) == 0x6U  ? 2U
                         : (c >> 4) == 0xEU  ? 3U
                         : (c >> 3) == 0x1EU ? 4U
                                             : 0U;
    for (uint32_t i = 1; i < len; i++) {
        if (((unsigned char)s[i] & 0xC0U) != 0x80U) {
            return 0;
        }
    }
    return len;
}

static void text_erase_last(EngMenu *m)
{
    size_t n = strlen(m->text);
    while (n > 0U) {
        n--;
        if (((unsigned char)m->text[n] & 0xC0U) != 0x80U) {
            break;
        }
    }
    m->text[n] = '\0';
}

void eng_menu_text_input(EngMenu *m, const char *utf8)
{
    if (m == NULL || utf8 == NULL || m->screen != ENG_MENU_NAME) {
        return;
    }
    const char *p = utf8;
    while (*p != '\0') {
        if (*p == '\b') {
            if (m->text_fresh) {
                m->text[0] = '\0'; /* le nom proposé s'efface d'un coup */
            } else {
                text_erase_last(m);
            }
            m->text_fresh = false;
            m->text_serial++;
            p++;
            continue;
        }
        const uint32_t len = utf8_length(p);
        if (len == 0U) {
            p++; /* octet invalide : ignoré */
            continue;
        }
        const unsigned char c = (unsigned char)*p;
        const bool control = c < 0x20U || c == 0x7FU;
        if (!control) {
            if (m->text_fresh) { /* la première frappe remplace le nom proposé */
                m->text[0] = '\0';
                m->text_fresh = false;
            }
            const size_t used = strlen(m->text);
            const bool leading_space = used == 0U && c == ' ';
            if (!leading_space && utf8_chars(m->text) < ENG_MENU_NAME_CHARS &&
                used + len < sizeof m->text) {
                memcpy(m->text + used, p, len);
                m->text[used + len] = '\0';
                m->text_serial++;
            } else if (!leading_space) {
                m->deny_serial++; /* nom plein */
            }
        }
        p += len;
    }
}

static void open_name(EngMenu *m, int32_t slot, EngMenuNamePurpose purpose)
{
    m->slot = eng_menu_valid_slot(slot);
    eng_menu_open(m, ENG_MENU_NAME);
    m->name_purpose = purpose;
    if (purpose == ENG_MENU_NAME_RENAME) {
        (void)snprintf(m->text, sizeof m->text, "%s", m->slots[m->slot].name);
    } else {
        eng_menu_default_name((uint32_t)m->slot, m->text, sizeof m->text);
        m->creative = false; /* aventure, sauf choix contraire sur la première ligne */
        m->selected = 1; /* sur « Créer la partie » : Entrée crée, sans passer par le mode */
    }
    m->text_fresh = true;
    m->text_serial++;
}

/* valide le nom saisi : espaces de fin retirés ; vide : refusé */
static bool name_ready(EngMenu *m)
{
    size_t n = strlen(m->text);
    while (n > 0U && m->text[n - 1U] == ' ') {
        m->text[--n] = '\0';
    }
    return n > 0U;
}

/* ── réglages ── */

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static bool same_options(const EngGameOptions *a, const EngGameOptions *b)
{
    bool same = a->master == b->master && a->sensitivity == b->sensitivity &&
                a->invert_y == b->invert_y && a->text_scale == b->text_scale &&
                a->high_contrast == b->high_contrast && a->fxaa == b->fxaa &&
                a->bloom == b->bloom && a->vsync == b->vsync && a->fullscreen == b->fullscreen &&
                a->shadow_quality == b->shadow_quality && a->view_distance == b->view_distance &&
                a->ssao == b->ssao && a->render_scale == b->render_scale;
    for (uint32_t i = 0; i < ENG_BUS_COUNT; i++) {
        same = same && a->volume[i] == b->volume[i];
    }
    return same;
}

/* réglage gauche / droite d'une ligne d'Options ; true si une valeur a changé */
static bool adjust(EngMenu *m, int dir)
{
    EngGameOptions *o = m->options;
    if (o == NULL) {
        return false;
    }
    const EngGameOptions before = *o;
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
    } else if (s == (int32_t)ENG_BUS_COUNT + 7) {
        o->vsync = !o->vsync;
    } else if (s == (int32_t)ENG_BUS_COUNT + 8) {
        o->fullscreen = !o->fullscreen;
    } else if (s == (int32_t)ENG_BUS_COUNT + 9) {
        o->shadow_quality = (uint32_t)((int32_t)o->shadow_quality + dir + 3) % 3U;
    } else if (s == (int32_t)ENG_BUS_COUNT + 10) {
        o->view_distance = (uint32_t)((int32_t)o->view_distance + dir + 3) % 3U;
    } else if (s == (int32_t)ENG_BUS_COUNT + 11) {
        o->ssao = !o->ssao;
    } else if (s == (int32_t)ENG_BUS_COUNT + 12) { /* 50, 75, 100 % sans boucler */
        o->render_scale =
            clampf(roundf((o->render_scale + (0.25F * (float)dir)) * 4.0F) / 4.0F, 0.5F, 1.0F);
    } else {
        return false;
    }
    m->adjust_dir = dir < 0 ? -1 : 1;
    m->adjust_serial++; /* même en butée : la ligne réagit */
    return !same_options(&before, o);
}

/* création d'une partie : aventure <-> créatif */
static void toggle_mode(EngMenu *m, int dir)
{
    m->creative = !m->creative;
    m->adjust_dir = dir < 0 ? -1 : 1;
    m->adjust_serial++;
}

/* ── valider, revenir ── */

static void back_to(EngMenu *m, EngMenuScreen screen, int32_t selected)
{
    eng_menu_open(m, screen);
    m->selected = selected;
}

static EngMenuAction confirm(EngMenu *m, const EngMenuItem *items, uint32_t n, int dir)
{
    if (m->selected < 0 || (uint32_t)m->selected >= n || !items[m->selected].enabled) {
        m->deny_serial++;
        return ENG_MENU_ACT_NONE;
    }
    const int32_t s = m->selected;
    m->confirm_serial++;
    switch (m->screen) {
    case ENG_MENU_MAIN:
        if (s == 0 && m->has_save) { /* la partie la plus récente */
            m->slot = eng_menu_valid_slot(m->last_slot);
            m->screen = ENG_MENU_CLOSED;
            return ENG_MENU_ACT_CONTINUE;
        }
        if (s == 0) { /* aucune partie : en créer une dans le premier emplacement libre */
            const int32_t free_slot = eng_menu_free_slot(m);
            if (free_slot >= 0) {
                open_name(m, free_slot, ENG_MENU_NAME_CREATE);
            } else {
                eng_menu_open(m, ENG_MENU_SAVES);
            }
            return ENG_MENU_ACT_NONE;
        }
        if (s == 1) {
            back_to(m, ENG_MENU_SAVES, m->has_save ? eng_menu_valid_slot(m->last_slot) : 0);
            return ENG_MENU_ACT_NONE;
        }
        if (s == 2) {
            eng_menu_open(m, ENG_MENU_OPTIONS);
            return ENG_MENU_ACT_NONE;
        }
        return ENG_MENU_ACT_QUIT;
    case ENG_MENU_SAVES:
        if (s >= (int32_t)ENG_MENU_SLOTS) {
            back_to(m, ENG_MENU_MAIN, 1);
            return ENG_MENU_ACT_NONE;
        }
        m->slot = s;
        if (m->slots[s].used) {
            /* fichier illisible : seule la suppression est proposée */
            back_to(m, ENG_MENU_SLOT, m->slots[s].unreadable ? 3 : 0);
        } else {
            open_name(m, s, ENG_MENU_NAME_CREATE);
        }
        return ENG_MENU_ACT_NONE;
    case ENG_MENU_SLOT:
        if (s == 0) {
            m->screen = ENG_MENU_CLOSED;
            return ENG_MENU_ACT_CONTINUE;
        }
        if (s == 1) {
            open_name(m, m->slot, ENG_MENU_NAME_RENAME);
            return ENG_MENU_ACT_NONE;
        }
        if (s == 2) {
            m->slot_target = eng_menu_free_slot(m);
            back_to(m, ENG_MENU_SAVES, m->slot_target >= 0 ? m->slot_target : m->slot);
            return m->slot_target >= 0 ? ENG_MENU_ACT_COPY_SLOT : ENG_MENU_ACT_NONE;
        }
        if (s == 3) {
            eng_menu_open(m, ENG_MENU_DELETE);
            return ENG_MENU_ACT_NONE;
        }
        back_to(m, ENG_MENU_SAVES, eng_menu_valid_slot(m->slot));
        return ENG_MENU_ACT_NONE;
    case ENG_MENU_NAME: {
        /* création : la première ligne est le mode de jeu ; puis valider, annuler */
        const int32_t first = m->name_purpose == ENG_MENU_NAME_CREATE ? 1 : 0;
        if (s < first) {
            toggle_mode(m, dir);
            m->confirm_serial--;
            return ENG_MENU_ACT_NONE;
        }
        if (s == first + 1) { /* Annuler */
            if (m->name_purpose == ENG_MENU_NAME_RENAME) {
                back_to(m, ENG_MENU_SLOT, 1);
            } else {
                back_to(m, ENG_MENU_SAVES, eng_menu_valid_slot(m->slot));
            }
            return ENG_MENU_ACT_NONE;
        }
        if (!name_ready(m)) {
            m->confirm_serial--;
            m->deny_serial++;
            return ENG_MENU_ACT_NONE;
        }
        if (m->name_purpose == ENG_MENU_NAME_RENAME) {
            back_to(m, ENG_MENU_SLOT, 1);
            return ENG_MENU_ACT_RENAME_SLOT;
        }
        m->screen = ENG_MENU_CLOSED;
        return ENG_MENU_ACT_NEW_GAME;
    }
    case ENG_MENU_DELETE:
        if (s == 1) {
            back_to(m, ENG_MENU_SAVES, eng_menu_valid_slot(m->slot));
            return ENG_MENU_ACT_DELETE_SLOT;
        }
        back_to(m, ENG_MENU_SLOT, 3);
        return ENG_MENU_ACT_NONE;
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
        return ENG_MENU_ACT_TO_MAIN; /* l'appelant enregistre, puis ouvre le menu principal */
    case ENG_MENU_OPTIONS:
        if (s == (int32_t)n - 2) {
            eng_menu_open(m, ENG_MENU_CONTROLS);
            return ENG_MENU_ACT_NONE;
        }
        if (s == (int32_t)n - 1) {
            back_to(m, m->options_return != ENG_MENU_CLOSED ? m->options_return : ENG_MENU_PAUSE,
                    2);
            return ENG_MENU_ACT_NONE;
        }
        return adjust(m, dir) ? ENG_MENU_ACT_OPTIONS_CHANGED : ENG_MENU_ACT_NONE;
    case ENG_MENU_CONTROLS:
        if (s < (int32_t)eng_menu_remap_count()) {
            m->remap = s;
            m->remap_serial = m->input != NULL ? m->input->last_serial : 0U;
            return ENG_MENU_ACT_NONE;
        }
        if (s == (int32_t)eng_menu_remap_count()) {
            if (m->input != NULL) {
                eng_input_set_default_bindings(m->input);
            }
            eng_menu_set_status(m, "Contrôles par défaut rétablis");
            return ENG_MENU_ACT_REMAPPED;
        }
        back_to(m, ENG_MENU_OPTIONS, (int32_t)ENG_BUS_COUNT + 13); /* sur « Contrôles... » */
        return ENG_MENU_ACT_NONE;
    default:
        return ENG_MENU_ACT_NONE;
    }
}

/* un niveau en arrière ; à la racine de la pause : la partie reprend */
static EngMenuAction go_back(EngMenu *m)
{
    switch (m->screen) {
    case ENG_MENU_PAUSE:
        m->screen = ENG_MENU_CLOSED;
        return ENG_MENU_ACT_RESUME;
    case ENG_MENU_OPTIONS:
        back_to(m, m->options_return != ENG_MENU_CLOSED ? m->options_return : ENG_MENU_PAUSE, 2);
        return ENG_MENU_ACT_NONE;
    case ENG_MENU_CONTROLS:
        back_to(m, ENG_MENU_OPTIONS, (int32_t)ENG_BUS_COUNT + 13);
        return ENG_MENU_ACT_NONE;
    case ENG_MENU_SAVES:
        back_to(m, ENG_MENU_MAIN, 1);
        return ENG_MENU_ACT_NONE;
    case ENG_MENU_SLOT:
        back_to(m, ENG_MENU_SAVES, eng_menu_valid_slot(m->slot));
        return ENG_MENU_ACT_NONE;
    case ENG_MENU_NAME:
        if (m->name_purpose == ENG_MENU_NAME_RENAME) {
            back_to(m, ENG_MENU_SLOT, 1);
        } else {
            back_to(m, ENG_MENU_SAVES, eng_menu_valid_slot(m->slot));
        }
        return ENG_MENU_ACT_NONE;
    case ENG_MENU_DELETE: /* rien n'est supprimé */
        back_to(m, ENG_MENU_SLOT, 3);
        return ENG_MENU_ACT_NONE;
    default: /* menu principal : Échap n'y fait rien */
        return ENG_MENU_ACT_NONE;
    }
}

void eng_menu_hover(EngMenu *m, int32_t item, int32_t dir, bool moved)
{
    if (m == NULL || m->screen == ENG_MENU_CLOSED) {
        return;
    }
    m->hover_item = item;
    m->hover_dir = dir;
    if (!moved || m->remap >= 0 || item < 0) {
        return;
    }
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    const uint32_t n = eng_menu_items(m, items, ENG_MENU_MAX_ITEMS);
    if ((uint32_t)item < n && items[item].enabled) {
        m->selected = item;
    }
}

/* pas de sélection : `step` vaut -1 ou +1 ; les lignes grisées sont sautées */
static void move_selection(EngMenu *m, const EngMenuItem *items, uint32_t first, uint32_t count,
                           int step, bool wrap)
{
    if (count == 0U) {
        return;
    }
    int32_t at = m->selected - (int32_t)first;
    for (uint32_t tries = 0; tries < count; tries++) {
        at += step;
        if (at < 0 || at >= (int32_t)count) {
            if (!wrap) {
                return;
            }
            at = (at + (int32_t)count) % (int32_t)count;
        }
        if (items[first + (uint32_t)at].enabled) {
            m->selected = (int32_t)first + at;
            return;
        }
    }
}

/* la dernière pression vient-elle de ce genre d'entrée ? (sans `input` : inconnu, donc non) */
static bool last_press_is(const EngMenu *m, EngBindKind a, EngBindKind b)
{
    return m->input != NULL && (m->input->last_kind == a || m->input->last_kind == b);
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
    const bool back_key = (in->menu_pressed & ENG_MENU_KEY_BACK) != 0U;
    if (m->screen == ENG_MENU_CLOSED) {
        if (eng_action_pressed(in, ENG_ACTION_PAUSE) || back_key) {
            eng_menu_open(m, ENG_MENU_PAUSE);
        }
        return ENG_MENU_ACT_NONE;
    }
    /* remappage : la prochaine pression (autre que celle qui a validé) devient la liaison ;
     * Échap annule */
    if (m->remap >= 0) {
        if (back_key) {
            m->remap = -1;
            eng_menu_set_status(m, "Touche inchangée");
            return ENG_MENU_ACT_NONE;
        }
        if (m->input != NULL && m->input->last_serial != m->remap_serial) {
            const EngAction a = eng_menu_remap_action((uint32_t)m->remap);
            eng_input_clear_action(m->input, a);
            (void)eng_input_bind_button(m->input, a, m->input->last_kind, m->input->last_code);
            m->remap = -1;
            /* L'entrée choisie ne doit pas agir aussitôt sur le menu : une flèche (touche de
             * menu, encore enfoncée) ne déplace pas la sélection tant qu'elle n'est pas relâchée,
             * et le cran de molette qui a servi à choisir ne presse pas l'action. */
            m->nav_wait_release = true;
            eng_input_drop_wheel(m->input);
            eng_menu_set_status(m, "Touche enregistrée");
            return ENG_MENU_ACT_REMAPPED;
        }
        return ENG_MENU_ACT_NONE;
    }
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    const uint32_t n = eng_menu_items(m, items, ENG_MENU_MAX_ITEMS);
    if (n > 0U && (m->selected < 0 || (uint32_t)m->selected >= n || !items[m->selected].enabled)) {
        for (uint32_t k = 0; k < n; k++) { /* jamais sur une ligne grisée */
            if (items[k].enabled) {
                m->selected = (int32_t)k;
                break;
            }
        }
    }
    /* Écran Nom : le clavier écrit. Seules comptent les touches de menu, la manette et la
     * souris ; ZQSD, Espace, P... sont du texte (eng_menu_text_input). */
    const bool typing = m->screen == ENG_MENU_NAME;
    const bool pad = last_press_is(m, ENG_BIND_PAD_BUTTON, ENG_BIND_PAD_TRIGGER);
    const bool mouse = last_press_is(m, ENG_BIND_MOUSE_BUTTON, ENG_BIND_MOUSE_BUTTON);
    const bool back_action =
        eng_action_pressed(in, ENG_ACTION_DODGE) || eng_action_pressed(in, ENG_ACTION_PAUSE);
    if (back_key || (back_action && (!typing || pad))) {
        return go_back(m);
    }
    /* navigation : un pas par pression, puis répétition toutes les 200 ms */
    float y = typing ? 0.0F : in->axis[ENG_AXIS_MOVE].y;
    float x = typing ? 0.0F : in->axis[ENG_AXIS_MOVE].x;
    if ((in->menu_held & ENG_MENU_KEY_UP) != 0U) {
        y = 1.0F;
    } else if ((in->menu_held & ENG_MENU_KEY_DOWN) != 0U) {
        y = -1.0F;
    }
    if ((in->menu_held & ENG_MENU_KEY_LEFT) != 0U) {
        x = -1.0F;
    } else if ((in->menu_held & ENG_MENU_KEY_RIGHT) != 0U) {
        x = 1.0F;
    }
    if (m->nav_wait_release) { /* après un remappage : attendre que tout soit relâché */
        if (fabsf(x) > NAV_DEAD || fabsf(y) > NAV_DEAD) {
            x = 0.0F;
            y = 0.0F;
        } else {
            m->nav_wait_release = false;
        }
    }
    const bool fresh_y = fabsf(y) > NAV_DEAD && fabsf(m->nav_prev_y) <= NAV_DEAD;
    const bool fresh_x = fabsf(x) > NAV_DEAD && fabsf(m->nav_prev_x) <= NAV_DEAD;
    m->nav_prev_y = y;
    m->nav_prev_x = x;
    if (m->nav_cooldown > 0U) {
        m->nav_cooldown--;
    }
    const bool step_y = fabsf(y) > NAV_DEAD && (fresh_y || m->nav_cooldown == 0U);
    const bool step_x = fabsf(x) > NAV_DEAD && (fresh_x || m->nav_cooldown == 0U);
    const int dir_y = y > 0.0F ? -1 : 1; /* avant (haut) = ligne précédente */
    const int dir_x = x > 0.0F ? 1 : -1;
    EngMenuAction act = ENG_MENU_ACT_NONE;
    if (m->screen == ENG_MENU_MAIN) { /* la barre du bas : gauche / droite (haut / bas aussi) */
        if (step_x || step_y) {
            move_selection(m, items, 0, n, step_x ? dir_x : dir_y, true);
            m->nav_cooldown = NAV_REPEAT;
        }
    } else if (m->screen == ENG_MENU_SAVES) { /* trois cartes côte à côte, « Retour » dessous */
        if (step_x && m->selected < (int32_t)ENG_MENU_SLOTS) {
            move_selection(m, items, 0, ENG_MENU_SLOTS, dir_x, false);
            m->nav_cooldown = NAV_REPEAT;
        } else if (step_y) {
            if (dir_y > 0 && m->selected < (int32_t)ENG_MENU_SLOTS) {
                m->slot = m->selected; /* on y revient en remontant */
                m->selected = (int32_t)ENG_MENU_SLOTS;
            } else if (dir_y < 0 && m->selected >= (int32_t)ENG_MENU_SLOTS) {
                m->selected = eng_menu_valid_slot(m->slot);
            }
            m->nav_cooldown = NAV_REPEAT;
        }
    } else if (step_y) {
        move_selection(m, items, 0, n, dir_y, true);
        m->nav_cooldown = NAV_REPEAT;
    } else if (m->screen == ENG_MENU_OPTIONS && step_x) {
        if (adjust(m, dir_x)) {
            act = ENG_MENU_ACT_OPTIONS_CHANGED;
        }
        m->nav_cooldown = NAV_REPEAT;
    } else if (m->screen == ENG_MENU_NAME && step_x) {
        if (m->name_purpose == ENG_MENU_NAME_CREATE && m->selected == 0) {
            toggle_mode(m, dir_x); /* gauche / droite sur « Mode de jeu » */
        }
        m->nav_cooldown = NAV_REPEAT;
    }
    if (act != ENG_MENU_ACT_NONE) {
        return act;
    }
    const bool ok_key = (in->menu_pressed & ENG_MENU_KEY_OK) != 0U;
    const bool jump = eng_action_pressed(in, ENG_ACTION_JUMP) && (!typing || pad);
    bool attack = eng_action_pressed(in, ENG_ACTION_ATTACK) && (!typing || pad || mouse);
    int dir = 1;
    if (attack && mouse) { /* un clic ne valide que la ligne sous la souris */
        if (m->hover_item < 0 || (uint32_t)m->hover_item >= n) {
            attack = false;
        } else {
            m->selected = m->hover_item;
            dir = m->hover_dir < 0 ? -1 : 1;
        }
    }
    if (ok_key || jump || attack) {
        act = confirm(m, items, n, dir);
    }
    return act;
}
