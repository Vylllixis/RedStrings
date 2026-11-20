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
    (void)snprintf(f->save_path, sizeof f->save_path, "%s%ssavegame.rssv", base, sep);
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
    f->menu.has_save = file_exists(f->save_path);
    ENG_LOG_INFO("parties : %s", f->save_path);
    return true;
}

static void new_game(GameFlow *f, PlayMode *play)
{
    float feet[3];
    float yaw = 0.0F;
    play_mode_spawn_point(play, feet, &yaw);
    play_mode_teleport(play, feet, yaw);
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
    s.player_yaw = play->loco.facing;
    s.play_time = f->play_time;
    s.has_time = true;
    s.hour = f->daynight.hour;
    if (play->combat != NULL) {
        play_combat_snapshot(play->combat, &s);
    }
    const bool ok = eng_save_write_file(f->save_path, &s, NULL);
    eng_menu_set_status(&f->menu, ok ? "Partie sauvegardée" : "Échec de la sauvegarde");
    if (ok) {
        f->menu.has_save = true;
    }
    return ok;
}

static bool load_game(GameFlow *f, PlayMode *play)
{
    EngSaveGame s;
    char err[128];
    if (!eng_save_read_file(f->save_path, &s, NULL, err, sizeof err)) {
        ENG_LOG_WARN("%s", err);
        eng_menu_open(&f->menu, ENG_MENU_MAIN);
        eng_menu_set_status(&f->menu, "Sauvegarde illisible");
        return false;
    }
    const float feet[3] = {(float)s.player_pos[0], (float)s.player_pos[1], (float)s.player_pos[2]};
    play_mode_teleport(play, feet, s.player_yaw);
    play->loco.facing = s.player_yaw;
    if (play->combat != NULL) {
        play_combat_restore(play->combat, &s);
    }
    f->play_time = s.play_time;
    (void)eng_daynight_set_hour(&f->daynight, s.has_time ? s.hour : NEW_GAME_HOUR);
    f->started = true;
    ENG_LOG_INFO("partie reprise (version %u, %.0f s de jeu)", s.version_read, s.play_time);
    return true;
}

bool game_flow_tick(GameFlow *f, PlayMode *play, const EngInputState *in, float dt)
{
    if (f == NULL || play == NULL || in == NULL) {
        return true;
    }
    const bool was_open = eng_menu_is_open(&f->menu);
    const EngMenuAction act = eng_menu_update(&f->menu, in);
    switch (act) {
    case ENG_MENU_ACT_NEW_GAME:
        new_game(f, play);
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

void game_flow_draw(const GameFlow *f, EngUi *ui, const PlayMode *play, const EngCamera *camera,
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
                       "cible   R : arme   P : pause");
        (void)eng_daynight_format(&f->daynight, hud.clock, sizeof hud.clock);
        eng_ui_draw_hud(ui, &hud, width, height);
    }
    eng_ui_draw_menu(ui, &f->menu, width, height, f->options.text_scale, f->options.high_contrast);
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
    ENG_LOG_INFO("AUTOTEST PARTIE : %s", g_ok ? "OK" : "ÉCHEC");
    play_mode_shutdown(&play);
    return g_ok;
}
