#include "game_flow.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/log.h"
#include "engine/save_game.h"
#include "engine/ui_game.h"
#include "engine/ui_menu.h"
#include "game_slots.h"

#define NEW_GAME_HOUR 9.0 /* une nouvelle partie commence le matin */
#define FADE_IN_MS 700U
/* fondus de sortie, en ticks de 1/60 s */
#define COVER_TO_GAME 44U  /* la scène du titre éclate puis s'éteint */
#define COVER_TO_TITLE 24U /* le jeu s'assombrit */
#define COVER_QUIT 22U

static void write_options(const GameFlow *f)
{
    static char text[8192];
    const size_t n = eng_options_save(&f->options, f->input, text, sizeof text);
    if (n == 0U || !SDL_SaveFile(f->options_path, text, n)) {
        ENG_LOG_WARN("options : écriture de %s impossible", f->options_path);
    }
}

static void apply_options(GameFlow *f, PlayMode *play)
{
    if (play->combat != NULL) {
        play_combat_apply_options(play->combat, &f->options);
    }
    (void)eng_orbit_camera_set_sensitivity(&play->orbit, f->options.sensitivity);
    play->orbit.invert_y = f->options.invert_y;
    f->graphics_dirty = true;
}

static bool file_exists(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

/* Relit les trois emplacements et trouve la partie la plus récente (celle de « Continuer »). Un
 * fichier illisible compte comme occupé : il n'est pas proposé comme emplacement libre, donc
 * jamais écrasé ; il ne peut qu'être supprimé. */
static void refresh_slots(GameFlow *f)
{
    int64_t newest = 0;
    bool any = false;
    f->menu.last_slot = 0;
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        static EngMenuSlot slot;
        int64_t when = 0;
        game_slot_describe(f->slot_path[i], i, f->world.boss_count, &slot, &when);
        eng_menu_set_slot(&f->menu, i, slot.used ? &slot : NULL);
        /* à date égale (même seconde), la partie en cours est la plus récente */
        const bool current = f->started && (int32_t)i == f->slot;
        if (slot.used && !slot.unreadable &&
            (!any || when > newest || (when == newest && current))) {
            newest = when;
            f->menu.last_slot = (int32_t)i;
            any = true;
        }
    }
    f->menu.has_save = any;
}

/* la partie en cours est celle de l'emplacement `slot` */
static void use_slot(GameFlow *f, int32_t slot)
{
    f->slot = slot < 0 ? 0 : (slot >= (int32_t)ENG_MENU_SLOTS ? (int32_t)ENG_MENU_SLOTS - 1 : slot);
    f->menu.slot = f->slot;
    (void)snprintf(f->save_path, sizeof f->save_path, "%s", f->slot_path[f->slot]);
}

bool game_flow_saved_fullscreen(bool fallback)
{
    char *pref = SDL_GetPrefPath("RedStrings", "game");
    if (pref == NULL) {
        return fallback;
    }
    char path[1024];
    (void)snprintf(path, sizeof path, "%soptions.txt", pref);
    SDL_free(pref);
    char *text = SDL_LoadFile(path, NULL);
    if (text == NULL) {
        return fallback;
    }
    EngGameOptions options;
    eng_options_default(&options);
    options.fullscreen = fallback;
    (void)eng_options_load(&options, NULL, text);
    SDL_free(text);
    return options.fullscreen;
}

bool game_flow_init(GameFlow *f, PlayMode *play, EngInput *input, const char *dir,
                    bool fullscreen_default)
{
    if (f == NULL || play == NULL || !play->ready) {
        return false;
    }
    memset(f, 0, sizeof *f);
    f->input = input;
    eng_options_default(&f->options);
    f->options.fullscreen = fullscreen_default;
    eng_daynight_init(&f->daynight, NEW_GAME_HOUR);
    char *pref = NULL;
    if (dir == NULL) {
        pref = SDL_GetPrefPath("RedStrings", "game");
    }
    const char *base = dir != NULL ? dir : (pref != NULL ? pref : "./");
    const char *sep = (base[0] != '\0' && base[strlen(base) - 1U] != '/') ? "/" : "";
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        (void)snprintf(f->slot_path[i], sizeof f->slot_path[i], "%s%spartie%u.rssv", base, sep,
                       (unsigned)(i + 1U));
    }
    /* avant les emplacements, il n'y avait qu'une sauvegarde : elle devient la partie 1 (copie) */
    char old_save[1024];
    (void)snprintf(old_save, sizeof old_save, "%s%ssavegame.rssv", base, sep);
    if (file_exists(old_save) && !file_exists(f->slot_path[0]) &&
        SDL_CopyFile(old_save, f->slot_path[0])) {
        ENG_LOG_INFO("parties : ancienne sauvegarde copiée dans l'emplacement 1 (%s)",
                     f->slot_path[0]);
    }
    (void)snprintf(f->options_path, sizeof f->options_path, "%s%soptions.txt", base, sep);
    SDL_free(pref);
    size_t size = 0;
    char *text = SDL_LoadFile(f->options_path, &size);
    if (text != NULL) {
        (void)eng_options_load(&f->options, input, text);
        SDL_free(text);
        ENG_LOG_INFO("options lues dans %s", f->options_path);
    }
    apply_options(f, play);
    eng_menu_init(&f->menu, &f->options, input);
    eng_menu_motion_init(&f->motion);
    eng_menu_open(&f->menu, ENG_MENU_MAIN);
    f->in_title = true;
    refresh_slots(f);
    use_slot(f, f->menu.last_slot);
    eng_menu_default_name((uint32_t)f->slot, f->name, sizeof f->name);
    uint32_t used = 0;
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        used += f->menu.slots[i].used ? 1U : 0U;
    }
    /* (`base` a été libéré avec `pref` : le dossier se lit dans le chemin du premier emplacement)
     */
    ENG_LOG_INFO("parties : %u emplacement(s) occupé(s) sur %u (%s et suivants)", used,
                 (unsigned)ENG_MENU_SLOTS, f->slot_path[0]);
    return true;
}

void game_flow_fade_in(GameFlow *f)
{
    if (f != NULL) {
        f->fade = 1.0F;
        f->fade_last_ms = 0U;
    }
}

void game_flow_set_world(GameFlow *f, const GameFlowWorld *world)
{
    if (f != NULL) {
        f->world = world != NULL ? *world : (GameFlowWorld){0};
        refresh_slots(f); /* les boss de la carte apparaissent sur les cartes des parties */
    }
}

void game_flow_set_transitions(GameFlow *f, bool transitions, bool title_scene)
{
    if (f != NULL) {
        f->transitions = transitions;
        f->title_scene = title_scene;
    }
}

bool game_flow_in_title(const GameFlow *f)
{
    return f != NULL && f->in_title;
}

float game_flow_title_outro(const GameFlow *f)
{
    if (f == NULL || !f->in_title || f->cover_total == 0U ||
        (f->pending != ENG_MENU_ACT_NEW_GAME && f->pending != ENG_MENU_ACT_CONTINUE &&
         f->pending != ENG_MENU_ACT_QUIT)) {
        return 0.0F;
    }
    const float p = (float)f->cover_ticks / (float)f->cover_total;
    return p > 1.0F ? 1.0F : p;
}

static void new_game(GameFlow *f, PlayMode *play)
{
    float feet[3];
    float yaw = 0.0F;
    play_mode_spawn_point(play, feet, &yaw);
    bool placed = false;
    if (f->world.to_world != NULL && f->world.place != NULL) {
        /* monde ouvert : le départ peut être loin (partie recommencée depuis la pause) */
        double spawn[3];
        f->world.to_world(f->world.user, feet, spawn);
        placed = f->world.place(f->world.user, spawn, yaw);
    }
    if (!placed) {
        play_mode_teleport(play, feet, yaw);
    }
    if (play->combat != NULL) {
        play_combat_reset(play->combat);
    }
    f->play_time = 0.0;
    (void)eng_daynight_set_hour(&f->daynight, NEW_GAME_HOUR);
    f->started = true;
    f->in_title = false;
}

void game_flow_start_new(GameFlow *f, PlayMode *play)
{
    if (f == NULL || play == NULL) {
        return;
    }
    eng_menu_open(&f->menu, ENG_MENU_CLOSED);
    /* rien n'est écrit : « Sauvegarder » visera le premier emplacement libre (le dernier s'ils
     * sont tous pris) */
    int32_t slot = (int32_t)ENG_MENU_SLOTS - 1;
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        if (!f->menu.slots[i].used) {
            slot = (int32_t)i;
            break;
        }
    }
    use_slot(f, slot);
    eng_menu_default_name((uint32_t)f->slot, f->name, sizeof f->name);
    new_game(f, play);
}

static bool save_game(GameFlow *f, PlayMode *play)
{
    static EngSaveGame s;
    eng_save_defaults(&s);
    float feet[3];
    (void)eng_character_position(play->character, feet);
    s.player_pos[0] = feet[0];
    s.player_pos[1] = feet[1];
    s.player_pos[2] = feet[2];
    if (f->world.to_world != NULL) { /* monde ouvert : la position MONDE */
        f->world.to_world(f->world.user, feet, s.player_pos);
    }
    s.player_yaw = play->loco.facing;
    s.play_time = f->play_time;
    s.has_time = true;
    s.hour = f->daynight.hour;
    if (play->combat != NULL) {
        play_combat_snapshot(play->combat, &s);
    }
    /* gestion des parties : le nom, le lieu et la date */
    s.has_meta = true;
    (void)snprintf(s.name, sizeof s.name, "%s", f->name);
    s.saved_at = game_slot_now();
    if (f->world.place_name != NULL) {
        f->world.place_name(f->world.user, s.player_pos, s.place, sizeof s.place);
    }
    const bool ok = eng_save_write_file(f->save_path, &s, NULL);
    char status[64];
    (void)snprintf(status, sizeof status, "Partie sauvegardée : %.31s", f->name);
    eng_menu_set_status(&f->menu, ok ? status : "Échec de la sauvegarde");
    refresh_slots(f);
    return ok;
}

static bool load_game(GameFlow *f, PlayMode *play)
{
    static EngSaveGame s;
    char err[128];
    use_slot(f, f->menu.slot);
    if (!eng_save_read_file(f->save_path, &s, NULL, err, sizeof err)) {
        ENG_LOG_WARN("%s", err);
        eng_menu_open(&f->menu, ENG_MENU_MAIN);
        eng_menu_set_status(&f->menu, "Sauvegarde illisible");
        return false;
    }
    if (f->world.place != NULL) {
        /* monde ouvert : relief chargé autour de la position enregistrée ; les ennemis ne sont pas
         * replacés (leurs positions étaient locales) : chaque zone renaît à l'approche */
        s.has_enemies = false;
        if (!f->world.place(f->world.user, s.player_pos, s.player_yaw)) {
            eng_menu_open(&f->menu, ENG_MENU_MAIN);
            eng_menu_set_status(&f->menu, "Reprise impossible : relief non chargé");
            return false;
        }
    } else {
        const float feet[3] = {(float)s.player_pos[0], (float)s.player_pos[1],
                               (float)s.player_pos[2]};
        play_mode_teleport(play, feet, s.player_yaw);
    }
    play->loco.facing = s.player_yaw;
    if (play->combat != NULL) {
        play_combat_restore(play->combat, &s);
    }
    f->play_time = s.play_time;
    (void)eng_daynight_set_hour(&f->daynight, s.has_time ? s.hour : NEW_GAME_HOUR);
    if (s.has_meta && s.name[0] != '\0') {
        (void)snprintf(f->name, sizeof f->name, "%s", s.name);
    } else {
        eng_menu_default_name((uint32_t)f->slot, f->name, sizeof f->name);
    }
    f->started = true;
    f->in_title = false;
    ENG_LOG_INFO("partie %d reprise : « %s » (version %u, %.0f s de jeu)", (int)f->slot + 1,
                 f->name, s.version_read, s.play_time);
    return true;
}

bool game_flow_save_now(GameFlow *f, PlayMode *play)
{
    return f != NULL && play != NULL && save_game(f, play);
}

bool game_flow_load_slot(GameFlow *f, PlayMode *play, int32_t slot)
{
    if (f == NULL || play == NULL || slot < 0 || slot >= (int32_t)ENG_MENU_SLOTS) {
        return false;
    }
    f->menu.slot = slot;
    eng_menu_open(&f->menu, ENG_MENU_CLOSED);
    return load_game(f, play);
}

/* l'action, pour de bon (après son fondu de sortie s'il y en a un) */
static void run_action(GameFlow *f, PlayMode *play, EngMenuAction act)
{
    switch (act) {
    case ENG_MENU_ACT_NEW_GAME: /* dans l'emplacement choisi, enregistrée tout de suite */
        use_slot(f, f->menu.slot);
        if (f->menu.text[0] != '\0') {
            (void)snprintf(f->name, sizeof f->name, "%s", f->menu.text);
        } else {
            eng_menu_default_name((uint32_t)f->slot, f->name, sizeof f->name);
        }
        new_game(f, play);
        (void)save_game(f, play);
        f->menu.status[0] = '\0'; /* pas de message « sauvegardée » au début d'une partie */
        f->menu.status_ticks = 0;
        ENG_LOG_INFO("nouvelle partie « %s » dans l'emplacement %d", f->name, (int)f->slot + 1);
        break;
    case ENG_MENU_ACT_CONTINUE:
        (void)load_game(f, play);
        break;
    case ENG_MENU_ACT_TO_MAIN: /* la partie vient d'être enregistrée : retour à l'écran-titre */
        eng_menu_open(&f->menu, ENG_MENU_MAIN);
        f->in_title = true;
        refresh_slots(f);
        eng_menu_motion_init(&f->motion);
        f->motion.title_menu = 1.0F; /* le titre est déjà à sa place de menu */
        break;
    case ENG_MENU_ACT_QUIT:
        f->quit = true;
        break;
    default:
        break;
    }
}

/* l'action attend la fin de son fondu de sortie ; sans fondus : tout de suite */
static void begin(GameFlow *f, PlayMode *play, EngMenuAction act, uint32_t ticks)
{
    if (!f->transitions || ticks == 0U) {
        run_action(f, play, act);
        return;
    }
    f->pending = act;
    f->cover_ticks = 0;
    f->cover_total = ticks;
}

static void slot_status(GameFlow *f, bool ok, const char *done, const char *failed)
{
    refresh_slots(f);
    eng_menu_set_status(&f->menu, ok ? done : failed);
}

static void handle_action(GameFlow *f, PlayMode *play, EngMenuAction act)
{
    const int32_t slot =
        f->menu.slot < 0 ? 0 : (f->menu.slot >= (int32_t)ENG_MENU_SLOTS ? 0 : f->menu.slot);
    char status[64];
    switch (act) {
    case ENG_MENU_ACT_NEW_GAME:
    case ENG_MENU_ACT_CONTINUE:
        begin(f, play, act, COVER_TO_GAME);
        break;
    case ENG_MENU_ACT_SAVE:
        (void)save_game(f, play);
        break;
    case ENG_MENU_ACT_TO_MAIN: /* « Sauvegarder et quitter » ; échec : la pause reste ouverte */
        if (save_game(f, play)) {
            begin(f, play, act, COVER_TO_TITLE);
        }
        break;
    case ENG_MENU_ACT_QUIT:
        begin(f, play, act, COVER_QUIT);
        break;
    case ENG_MENU_ACT_OPTIONS_CHANGED:
        apply_options(f, play);
        write_options(f);
        break;
    case ENG_MENU_ACT_REMAPPED:
        write_options(f);
        break;
    case ENG_MENU_ACT_RENAME_SLOT: {
        const bool ok = game_slot_rename(f->slot_path[slot], f->menu.text);
        if (ok && f->started && slot == f->slot) { /* la partie en cours change de nom aussi */
            (void)snprintf(f->name, sizeof f->name, "%s", f->menu.text);
        }
        slot_status(f, ok, "Partie renommée", "Renommage impossible");
        break;
    }
    case ENG_MENU_ACT_DELETE_SLOT:
        slot_status(f, game_slot_delete(f->slot_path[slot]), "Partie supprimée",
                    "Suppression impossible");
        break;
    case ENG_MENU_ACT_COPY_SLOT: {
        const int32_t to = f->menu.slot_target;
        const bool ok = to >= 0 && to < (int32_t)ENG_MENU_SLOTS && to != slot &&
                        !file_exists(f->slot_path[to]) &&
                        game_slot_copy(f->slot_path[slot], f->slot_path[to]);
        (void)snprintf(status, sizeof status, "Partie copiée dans l'emplacement %d", (int)to + 1);
        slot_status(f, ok, status, "Copie impossible");
        break;
    }
    default:
        break;
    }
}

bool game_flow_tick(GameFlow *f, PlayMode *play, const EngInputState *in, float dt)
{
    if (f == NULL || play == NULL || in == NULL) {
        return true;
    }
    if (f->pending != ENG_MENU_ACT_NONE) { /* fondu de sortie : plus rien ne répond */
        f->cover_ticks++;
        if (f->cover_ticks >= f->cover_total) {
            const EngMenuAction act = f->pending;
            f->pending = ENG_MENU_ACT_NONE;
            run_action(f, play, act);
            if (!f->quit) {
                game_flow_fade_in(f); /* la suite sort du noir */
            }
        }
        return false;
    }
    const bool was_open = eng_menu_is_open(&f->menu);
    const EngMenuAction act = eng_menu_update(&f->menu, in);
    handle_action(f, play, act);
    if (eng_menu_is_open(&f->menu) || was_open || f->pending != ENG_MENU_ACT_NONE) {
        return false; /* le tick qui ferme le menu ne rejoue pas la touche de validation */
    }
    f->play_time += (double)dt;
    eng_daynight_tick(&f->daynight, (double)dt);
    return true;
}

bool game_flow_wants_text(const GameFlow *f)
{
    return f != NULL && f->menu.screen == ENG_MENU_NAME && f->pending == ENG_MENU_ACT_NONE;
}

void game_flow_text(GameFlow *f, const char *utf8)
{
    if (game_flow_wants_text(f) && utf8 != NULL) {
        eng_menu_text_input(&f->menu, utf8);
    }
}

void game_flow_draw(GameFlow *f, EngUi *ui, const PlayMode *play, const EngCamera *camera,
                    float aspect, int width, int height, float dt)
{
    if (f == NULL || ui == NULL || play == NULL || camera == NULL) {
        return;
    }
    eng_menu_motion_update(&f->motion, &f->menu, dt, aspect > 0.0F ? aspect : 1.0F);
    if (!eng_menu_is_open(&f->menu) && !f->in_title && play->combat != NULL) {
        static EngHud hud;
        memset(&hud, 0, sizeof hud);
        hud.text_scale = f->options.text_scale;
        hud.high_contrast = f->options.high_contrast;
        mat4 view;
        mat4 proj;
        mat4 vp;
        eng_camera_view(camera, view);
        eng_camera_projection(camera, aspect, proj);
        glm_mat4_mul(proj, view, vp);
        play_combat_fill_hud(play->combat, play, vp, width, height, &hud);
        (void)snprintf(hud.hint, sizeof hud.hint, "%s",
                       "clic : attaque   clic droit : lourde   Ctrl : esquive   E : garde   A/Q : "
                       "cible   R : arme   , : créatif   Échap : menu");
        (void)eng_daynight_format(&f->daynight, hud.clock, sizeof hud.clock);
        eng_ui_draw_hud(ui, &hud, width, height);
    }
    const EngMenuView view = {.width = width,
                              .height = height,
                              .text_scale = f->options.text_scale,
                              .high_contrast = f->options.high_contrast,
                              .on_title = f->in_title && f->title_scene};
    const EngMenuHover hover = eng_ui_draw_menu(ui, &f->menu, &f->motion, &view);
    /* la souris ne choisit une ligne que lorsqu'elle bouge : immobile, elle laisse le clavier et
     * la manette choisir */
    float at[2];
    eng_ui_mouse_pos(ui, at); /* la même position que celle qui désigne les lignes */
    const bool moved = at[0] != f->mouse[0] || at[1] != f->mouse[1];
    f->mouse[0] = at[0];
    f->mouse[1] = at[1];
    eng_menu_hover(&f->menu, hover.item, hover.dir, moved && f->pending == ENG_MENU_ACT_NONE);
    if (f->pending != ENG_MENU_ACT_NONE && f->cover_total > 0U) {
        /* fondu de sortie : la scène du titre s'éteint d'elle-même (le voile ne fait que finir le
         * noir) ; ailleurs le voile fait tout le fondu */
        const float p = fminf((float)f->cover_ticks / (float)f->cover_total, 1.0F);
        const bool scene = view.on_title && f->pending != ENG_MENU_ACT_TO_MAIN;
        const float t = scene ? fmaxf((p - 0.72F) / 0.28F, 0.0F) : p;
        eng_ui_draw_fade(ui, t * t * (3.0F - (2.0F * t)), width, height);
    } else if (f->fade > 0.0F) {
        /* fondu d'entrée : il avance d'au plus 34 ms par image, pour que les premières images du
         * jeu (lentes : création des pipelines) ne le consomment pas d'un coup */
        const uint64_t now = SDL_GetTicks();
        const uint64_t step = f->fade_last_ms == 0U ? 0U : now - f->fade_last_ms;
        f->fade_last_ms = now > 0U ? now : 1U;
        f->fade -= (float)(step > 34U ? 34U : step) / (float)FADE_IN_MS;
        if (f->fade > 0.0F) {
            eng_ui_draw_fade(ui, f->fade * f->fade, width, height);
        }
    }
}
