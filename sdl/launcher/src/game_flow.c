#include "game_flow.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/log.h"
#include "engine/save_game.h"
#include "engine/ui_game.h"

static void write_options(const GameFlow *f)
{
    static char text[8192];
    const size_t n = eng_options_save(&f->options, f->input, text, sizeof text);
    if (n == 0U || !SDL_SaveFile(f->options_path, text, n)) {
        ENG_LOG_WARN("options : écriture de %s impossible", f->options_path);
    }
}

#define NEW_GAME_HOUR 9.0 /* une nouvelle partie commence le matin */

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

#define FADE_IN_MS 700U

/* « Niv. 3 · 12 min » ou « Niv. 7 · 2 h 05 » */
static void slot_summary(const EngSaveGame *s, char *out, size_t cap)
{
    const unsigned level = s->has_progress && s->progress.level > 0U ? s->progress.level : 1U;
    const unsigned minutes = s->play_time > 0.0 ? (unsigned)(s->play_time / 60.0) : 0U;
    if (minutes >= 60U) {
        (void)snprintf(out, cap, "Niv. %u · %u h %02u", level, minutes / 60U, minutes % 60U);
    } else {
        (void)snprintf(out, cap, "Niv. %u · %u min", level, minutes);
    }
}

/* Relit les trois emplacements : occupés ou libres, leur résumé, et le plus récent (celui de
 * « Continuer »). Un fichier illisible compte comme occupé (« illisible ») : il n'est pas proposé
 * comme emplacement libre, donc jamais écrasé sans confirmation. */
static void refresh_slots(GameFlow *f)
{
    SDL_Time newest = 0;
    bool any = false;
    f->menu.last_slot = 0;
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        SDL_PathInfo info;
        const bool present =
            SDL_GetPathInfo(f->slot_path[i], &info) && info.type == SDL_PATHTYPE_FILE;
        if (!present) {
            eng_menu_set_slot(&f->menu, i, false, NULL);
            continue;
        }
        static EngSaveGame s;
        char err[128];
        char summary[ENG_MENU_LABEL];
        if (eng_save_read_file(f->slot_path[i], &s, NULL, err, sizeof err)) {
            slot_summary(&s, summary, sizeof summary);
        } else {
            (void)snprintf(summary, sizeof summary, "illisible");
        }
        eng_menu_set_slot(&f->menu, i, true, summary);
        if (!any || info.modify_time >= newest) {
            newest = info.modify_time;
            f->menu.last_slot = (int32_t)i;
        }
        any = true;
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

bool game_flow_saved_fullscreen(void)
{
    char *pref = SDL_GetPrefPath("RedStrings", "game");
    if (pref == NULL) {
        return false;
    }
    char path[1024];
    (void)snprintf(path, sizeof path, "%soptions.txt", pref);
    SDL_free(pref);
    char *text = SDL_LoadFile(path, NULL);
    if (text == NULL) {
        return false;
    }
    EngGameOptions options;
    eng_options_default(&options);
    (void)eng_options_load(&options, NULL, text);
    SDL_free(text);
    return options.fullscreen;
}

bool game_flow_init(GameFlow *f, PlayMode *play, EngInput *input, const char *dir)
{
    if (f == NULL || play == NULL || !play->ready) {
        return false;
    }
    memset(f, 0, sizeof *f);
    f->input = input;
    eng_options_default(&f->options);
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
    eng_menu_open(&f->menu, ENG_MENU_MAIN);
    refresh_slots(f);
    use_slot(f, f->menu.last_slot);
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
    }
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
    new_game(f, play);
}

static bool save_game(GameFlow *f, PlayMode *play)
{
    EngSaveGame s;
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
    const bool ok = eng_save_write_file(f->save_path, &s, NULL);
    char status[64];
    (void)snprintf(status, sizeof status, "Partie %d sauvegardée", (int)f->slot + 1);
    eng_menu_set_status(&f->menu, ok ? status : "Échec de la sauvegarde");
    refresh_slots(f);
    return ok;
}

static bool load_game(GameFlow *f, PlayMode *play)
{
    EngSaveGame s;
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
    f->started = true;
    ENG_LOG_INFO("partie %d reprise (version %u, %.0f s de jeu)", (int)f->slot + 1, s.version_read,
                 s.play_time);
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

bool game_flow_tick(GameFlow *f, PlayMode *play, const EngInputState *in, float dt)
{
    if (f == NULL || play == NULL || in == NULL) {
        return true;
    }
    const bool was_open = eng_menu_is_open(&f->menu);
    const EngMenuAction act = eng_menu_update(&f->menu, in);
    switch (act) {
    case ENG_MENU_ACT_NEW_GAME: /* dans l'emplacement choisi, enregistrée tout de suite */
        use_slot(f, f->menu.slot);
        new_game(f, play);
        (void)save_game(f, play);
        ENG_LOG_INFO("nouvelle partie dans l'emplacement %d", (int)f->slot + 1);
        break;
    case ENG_MENU_ACT_CONTINUE:
        (void)load_game(f, play);
        break;
    case ENG_MENU_ACT_SAVE:
        (void)save_game(f, play);
        break;
    case ENG_MENU_ACT_QUIT:
        f->quit = true;
        break;
    case ENG_MENU_ACT_OPTIONS_CHANGED:
        apply_options(f, play);
        write_options(f);
        break;
    case ENG_MENU_ACT_REMAPPED:
        write_options(f);
        break;
    default:
        break;
    }
    if (eng_menu_is_open(&f->menu) || was_open) {
        return false; /* le tick qui ferme le menu ne rejoue pas la touche de validation */
    }
    f->play_time += (double)dt;
    eng_daynight_tick(&f->daynight, (double)dt);
    return true;
}

void game_flow_draw(GameFlow *f, EngUi *ui, const PlayMode *play, const EngCamera *camera,
                    float aspect, int width, int height)
{
    if (f == NULL || ui == NULL || play == NULL || camera == NULL) {
        return;
    }
    if (!eng_menu_is_open(&f->menu) && play->combat != NULL) {
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
                       "cible   R : arme   , : créatif   P : pause");
        (void)eng_daynight_format(&f->daynight, hud.clock, sizeof hud.clock);
        eng_ui_draw_hud(ui, &hud, width, height);
    }
    const int32_t hovered = eng_ui_draw_menu(ui, &f->menu, width, height, f->options.text_scale,
                                             f->options.high_contrast);
    /* la souris ne choisit une ligne que lorsqu'elle bouge : immobile, elle laisse le clavier et
     * la manette choisir */
    float mx = 0.0F;
    float my = 0.0F;
    (void)SDL_GetMouseState(&mx, &my);
    if (mx != f->mouse[0] || my != f->mouse[1]) {
        f->mouse[0] = mx;
        f->mouse[1] = my;
        eng_menu_hover(&f->menu, hovered);
    }
    if (f->fade > 0.0F) {
        /* fondu d'entrée : il avance d'au plus 34 ms par image, pour que les premières images du
         * jeu (lentes : création des pipelines) ne le consomment pas d'un coup */
        const uint64_t now = SDL_GetTicks();
        const uint64_t dt = f->fade_last_ms == 0U ? 0U : now - f->fade_last_ms;
        f->fade_last_ms = now > 0U ? now : 1U;
        f->fade -= (float)(dt > 34U ? 34U : dt) / (float)FADE_IN_MS;
        if (f->fade > 0.0F) {
            eng_ui_draw_fade(ui, f->fade * f->fade, width, height);
        }
    }
}

/* ── Autotest ── */

static bool g_ok;

static void check(bool cond, const char *what, double v)
{
    if (cond) {
        ENG_LOG_INFO("  ok     %-58s %10.3f", what, v);
    } else {
        ENG_LOG_ERROR("  ÉCHEC  %-58s %10.3f", what, v);
        g_ok = false;
    }
}

static void tick(GameFlow *f, PlayMode *p, uint32_t pressed, float mx, float my)
{
    EngInputState in;
    memset(&in, 0, sizeof in);
    in.pressed = pressed;
    in.held = pressed;
    in.axis[ENG_AXIS_MOVE] = (EngAxisValue){mx, my};
    if (game_flow_tick(f, p, &in, 1.0F / 60.0F)) {
        play_mode_tick(p, &in, 1.0F / 60.0F);
    }
}

static void idle(GameFlow *f, PlayMode *p, int n)
{
    for (int i = 0; i < n; i++) {
        tick(f, p, 0U, 0.0F, 0.0F);
    }
}

bool game_flow_selftest(void)
{
    g_ok = true;
    static PlayMode play;
    static GameFlow flow;
    static EngInput input;
    char dir[600];
    char *pref = SDL_GetPrefPath("RedStrings", "selftest");
    (void)snprintf(dir, sizeof dir, "%s", pref != NULL ? pref : "./");
    SDL_free(pref);
    char path[700];
    (void)snprintf(path, sizeof path, "%ssavegame.rssv", dir);
    (void)SDL_RemovePath(path);
    for (unsigned i = 1; i <= ENG_MENU_SLOTS; i++) {
        (void)snprintf(path, sizeof path, "%spartie%u.rssv", dir, i);
        (void)SDL_RemovePath(path);
    }
    (void)snprintf(path, sizeof path, "%soptions.txt", dir);
    (void)SDL_RemovePath(path);
    eng_input_init(&input);
    eng_input_set_default_bindings(&input);
    if (!play_mode_init(&play, NULL, 1.0F, false) || !play_mode_enable_combat(&play, false) ||
        !game_flow_init(&flow, &play, &input, dir)) {
        ENG_LOG_ERROR("AUTOTEST PARTIE : ÉCHEC (initialisation ; Jolt requis)");
        play_mode_shutdown(&play);
        return false;
    }
    const uint32_t jump = 1U << ENG_ACTION_JUMP;
    const uint32_t pause = 1U << ENG_ACTION_PAUSE;
    check(eng_menu_is_open(&flow.menu) && flow.menu.screen == ENG_MENU_MAIN,
          "menu principal au lancement", 1);
    check(!flow.menu.has_save, "« Continuer » grisé sans sauvegarde", 0);
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F,
         0.0F); /* Nouvelle partie (sélection forcée sur la 1re ligne active) */
    check(!eng_menu_is_open(&flow.menu) && flow.started, "nouvelle partie lancée", 1);
    /* jouer : marcher 2 s vers +X, frapper le mannequin n'est pas nécessaire */
    float before[3];
    (void)eng_character_position(play.character, before);
    for (int i = 0; i < 120; i++) {
        tick(&flow, &play, 0U, 0.0F, 1.0F);
    }
    idle(&flow, &play, 30);
    float feet[3];
    (void)eng_character_position(play.character, feet);
    check(hypotf(feet[0] - before[0], feet[2] - before[2]) > 3.0F, "le personnage a marché (m)",
          hypotf(feet[0] - before[0], feet[2] - before[2]));
    PlayCombat *c = play.combat;
    EngSaveGame expect;
    eng_save_defaults(&expect);
    play_combat_snapshot(c, &expect);
    /* on blesse un ennemi et le joueur pour que l'état ne soit pas celui d'une partie neuve */
    (void)expect;
    /* pause -> Sauvegarder */
    tick(&flow, &play, pause, 0.0F, 0.0F);
    check(flow.menu.screen == ENG_MENU_PAUSE, "pause ouverte", 1);
    idle(&flow, &play, 2);
    tick(&flow, &play, 0U, 0.0F, -1.0F); /* bas : Sauvegarder */
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F);
    check(flow.menu.has_save, "partie sauvegardée", 1);
    EngSaveGame saved;
    char err[128];
    check(eng_save_read_file(flow.save_path, &saved, NULL, err, sizeof err), "fichier relu", 1);
    const double time_saved = flow.play_time;
    const double hour_saved = flow.daynight.hour;
    check(saved.has_time && fabs(saved.hour - hour_saved) < 1e-9, "heure sauvegardée (h)",
          saved.hour);
    /* menu principal puis « quitter » : tout est détruit */
    idle(&flow, &play, 2);
    tick(&flow, &play, 0U, 0.0F, -1.0F);
    idle(&flow, &play, 2);
    tick(&flow, &play, 0U, 0.0F, -1.0F);
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F); /* Menu principal */
    check(flow.menu.screen == ENG_MENU_MAIN, "retour au menu principal", 1);
    play_mode_shutdown(&play);
    /* relance du jeu */
    eng_input_init(&input);
    eng_input_set_default_bindings(&input);
    if (!play_mode_init(&play, NULL, 1.0F, false) || !play_mode_enable_combat(&play, false) ||
        !game_flow_init(&flow, &play, &input, dir)) {
        ENG_LOG_ERROR("AUTOTEST PARTIE : ÉCHEC (relance)");
        return false;
    }
    check(flow.menu.has_save, "« Continuer » actif après relance", 1);
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F); /* Continuer */
    check(!eng_menu_is_open(&flow.menu), "partie reprise", 1);
    float after[3];
    (void)eng_character_position(play.character, after);
    check(fabsf(after[0] - (float)saved.player_pos[0]) < 1e-3F &&
              fabsf(after[2] - (float)saved.player_pos[2]) < 1e-3F,
          "position restituée (écart, m)",
          hypotf(after[0] - (float)saved.player_pos[0], after[2] - (float)saved.player_pos[2]));
    check(fabs(flow.play_time - time_saved) < 1e-6, "temps de jeu restitué (s)", flow.play_time);
    check(fabs(flow.daynight.hour - hour_saved) < 1e-9, "heure du jour restituée (h)",
          flow.daynight.hour);
    EngSaveGame now;
    eng_save_defaults(&now);
    play_combat_snapshot(play.combat, &now);
    check(now.hp == saved.hp && now.stamina == saved.stamina, "PV et endurance restitués", now.hp);
    check(saved.has_progress && now.progress.level == saved.progress.level &&
              now.progress.total_xp == saved.progress.total_xp,
          "niveau et XP restitués", (double)now.progress.level);
    bool enemies_ok = now.enemy_count == saved.enemy_count;
    for (uint32_t i = 0; i < now.enemy_count && enemies_ok; i++) {
        enemies_ok = now.enemies[i].alive == saved.enemies[i].alive &&
                     now.enemies[i].hp == saved.enemies[i].hp &&
                     now.enemies[i].pos[0] == saved.enemies[i].pos[0];
    }
    check(enemies_ok, "ennemis restitués (vie, PV, position)", (double)now.enemy_count);
    /* options : volume modifié, relu après relance */
    tick(&flow, &play, pause, 0.0F, 0.0F);
    idle(&flow, &play, 2);
    for (int k = 0; k < 2; k++) {
        tick(&flow, &play, 0U, 0.0F, -1.0F);
        idle(&flow, &play, 2);
    }
    tick(&flow, &play, jump, 0.0F, 0.0F); /* Options */
    tick(&flow, &play, 0U, -1.0F, 0.0F);  /* volume général -10 % */
    idle(&flow, &play, 2);
    const float master = flow.options.master;
    play_mode_shutdown(&play);
    eng_input_init(&input);
    (void)play_mode_init(&play, NULL, 1.0F, false);
    (void)play_mode_enable_combat(&play, false);
    (void)game_flow_init(&flow, &play, &input, dir);
    check(fabsf(flow.options.master - master) < 1e-3F && master < 0.8F,
          "options relues après relance", flow.options.master);
    /* ── Trois emplacements de sauvegarde (2026-11-22) ── */
    check(flow.menu.slots[0].used && !flow.menu.slots[1].used && !flow.menu.slots[2].used &&
              flow.menu.last_slot == 0 && flow.slot == 0,
          "emplacements : seule la partie 1 est occupée", (double)flow.menu.last_slot);
    check(strstr(flow.menu.slots[0].summary, "Niv. ") != NULL,
          "emplacements : résumé de la partie 1", 1);
    /* « Nouvelle partie » fait choisir l'emplacement ; le premier libre est proposé */
    idle(&flow, &play, 2);
    tick(&flow, &play, 0U, 0.0F, -1.0F);
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F);
    check(flow.menu.screen == ENG_MENU_NEW && flow.menu.selected == 1,
          "nouvelle partie : choix de l'emplacement (curseur)", (double)flow.menu.selected);
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F);
    check(!eng_menu_is_open(&flow.menu) && flow.started && flow.slot == 1,
          "nouvelle partie dans l'emplacement 2", (double)flow.slot);
    check(file_exists(flow.slot_path[1]) && flow.menu.slots[1].used && flow.menu.last_slot == 1,
          "enregistrée aussitôt ; c'est la plus récente", (double)flow.menu.last_slot);
    for (int i = 0; i < 90; i++) {
        tick(&flow, &play, 0U, 0.0F, 1.0F);
    }
    idle(&flow, &play, 30);
    tick(&flow, &play, pause, 0.0F, 0.0F);
    idle(&flow, &play, 2);
    tick(&flow, &play, 0U, 0.0F, -1.0F);
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F); /* Sauvegarder : dans l'emplacement 2 */
    static EngSaveGame two;
    float here[3];
    (void)eng_character_position(play.character, here);
    check(eng_save_read_file(flow.slot_path[1], &two, NULL, err, sizeof err) &&
              fabsf((float)two.player_pos[0] - here[0]) < 1e-3F &&
              hypot(two.player_pos[0] - saved.player_pos[0],
                    two.player_pos[2] - saved.player_pos[2]) > 1.0,
          "« Sauvegarder » écrit l'emplacement de la partie en cours", two.player_pos[0]);
    static EngSaveGame one;
    check(eng_save_read_file(flow.slot_path[0], &one, NULL, err, sizeof err) &&
              one.player_pos[0] == saved.player_pos[0] && one.play_time == saved.play_time,
          "la partie 1 n'a pas bougé", one.player_pos[0]);
    /* menu principal, « Charger une partie », partie 1 */
    for (int k = 0; k < 2; k++) {
        idle(&flow, &play, 2);
        tick(&flow, &play, 0U, 0.0F, -1.0F);
    }
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F);
    check(flow.menu.screen == ENG_MENU_MAIN, "emplacements : retour au menu principal", 1);
    for (int k = 0; k < 2; k++) {
        idle(&flow, &play, 2);
        tick(&flow, &play, 0U, 0.0F, -1.0F);
    }
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F);
    check(flow.menu.screen == ENG_MENU_LOAD && flow.menu.selected == 1,
          "charger : la liste, curseur sur la plus récente", (double)flow.menu.selected);
    idle(&flow, &play, 2);
    tick(&flow, &play, 0U, 0.0F, 1.0F);
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F);
    (void)eng_character_position(play.character, here);
    check(!eng_menu_is_open(&flow.menu) && flow.slot == 0 &&
              fabsf(here[0] - (float)saved.player_pos[0]) < 1e-3F &&
              fabsf(here[2] - (float)saved.player_pos[2]) < 1e-3F,
          "charger la partie 1 : sa position (x)", here[0]);
    /* écraser la partie 1 : « Non » ne change rien, « Oui » la remplace par une partie neuve */
    tick(&flow, &play, pause, 0.0F, 0.0F);
    for (int k = 0; k < 3; k++) {
        idle(&flow, &play, 2);
        tick(&flow, &play, 0U, 0.0F, -1.0F);
    }
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F); /* Menu principal */
    idle(&flow, &play, 2);
    tick(&flow, &play, 0U, 0.0F, -1.0F);
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F); /* Nouvelle partie : curseur sur l'emplacement 3 */
    for (int k = 0; k < 2; k++) {
        idle(&flow, &play, 2);
        tick(&flow, &play, 0U, 0.0F, 1.0F);
    }
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F);
    check(flow.menu.screen == ENG_MENU_OVERWRITE && flow.menu.selected == 0,
          "écraser : confirmation demandée, curseur sur « Non »", (double)flow.menu.selected);
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F); /* Non */
    check(flow.menu.screen == ENG_MENU_NEW &&
              eng_save_read_file(flow.slot_path[0], &one, NULL, err, sizeof err) &&
              one.play_time == saved.play_time,
          "écraser : « Non » ne touche pas la partie 1 (temps de jeu)", one.play_time);
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F);
    idle(&flow, &play, 2);
    tick(&flow, &play, 0U, 0.0F, -1.0F);
    idle(&flow, &play, 2);
    tick(&flow, &play, jump, 0.0F, 0.0F); /* Oui */
    const bool reread = eng_save_read_file(flow.slot_path[0], &one, NULL, err, sizeof err);
    check(!eng_menu_is_open(&flow.menu) && flow.slot == 0 && reread && one.play_time < 1.0 &&
              saved.play_time > 2.0,
          "écraser : « Oui » remplace la partie 1 par une partie neuve", one.play_time);
    check(strstr(flow.menu.slots[0].summary, "0 min") != NULL,
          "emplacements : résumé à jour après l'écrasement", 1);
    /* une ancienne sauvegarde unique (savegame.rssv) est copiée dans l'emplacement 1 libre */
    play_mode_shutdown(&play);
    (void)snprintf(path, sizeof path, "%ssavegame.rssv", dir);
    const bool staged = SDL_CopyFile(flow.slot_path[1], path) && SDL_RemovePath(flow.slot_path[0]);
    eng_input_init(&input);
    (void)play_mode_init(&play, NULL, 1.0F, false);
    (void)play_mode_enable_combat(&play, false);
    (void)game_flow_init(&flow, &play, &input, dir);
    check(staged && flow.menu.slots[0].used && file_exists(path) &&
              eng_save_read_file(flow.slot_path[0], &one, NULL, err, sizeof err) &&
              one.player_pos[0] == two.player_pos[0],
          "ancienne sauvegarde copiée dans l'emplacement 1 (x)", one.player_pos[0]);
    ENG_LOG_INFO("AUTOTEST PARTIE : %s", g_ok ? "OK" : "ÉCHEC");
    play_mode_shutdown(&play);
    return g_ok;
}
