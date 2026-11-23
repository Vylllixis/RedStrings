/* Autotest sans fenêtre du déroulé de partie (--game-selftest) : le menu est piloté comme par un
 * joueur (touches de menu, texte tapé), les fichiers sont dans un dossier à part. Voir game_flow.h.
 */
#include "game_flow.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/log.h"
#include "engine/save_game.h"
#include "game_slots.h"

static bool g_ok;

static void check(bool cond, const char *what, double v)
{
    if (cond) {
        ENG_LOG_INFO("  ok     %-62s %10.3f", what, v);
    } else {
        ENG_LOG_ERROR("  ÉCHEC  %-62s %10.3f", what, v);
        g_ok = false;
    }
}

static bool file_exists(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

/* un tick : actions pressées, stick, touches de menu */
static void tick(GameFlow *f, PlayMode *p, uint32_t pressed, float mx, float my, uint32_t keys)
{
    EngInputState in;
    memset(&in, 0, sizeof in);
    in.pressed = pressed;
    in.held = pressed;
    in.axis[ENG_AXIS_MOVE] = (EngAxisValue){mx, my};
    in.menu_pressed = keys;
    in.menu_held = keys;
    if (game_flow_tick(f, p, &in, 1.0F / 60.0F)) {
        play_mode_tick(p, &in, 1.0F / 60.0F);
    }
}

static void idle(GameFlow *f, PlayMode *p, int n)
{
    for (int i = 0; i < n; i++) {
        tick(f, p, 0U, 0.0F, 0.0F, 0U);
    }
}

/* une touche de menu, puis relâchée (la répétition ne se déclenche pas) */
static void key(GameFlow *f, PlayMode *p, uint32_t k)
{
    tick(f, p, 0U, 0.0F, 0.0F, k);
    idle(f, p, 2);
}

static void keys(GameFlow *f, PlayMode *p, uint32_t k, int times)
{
    for (int i = 0; i < times; i++) {
        key(f, p, k);
    }
}

static bool relaunch(GameFlow *f, PlayMode *p, EngInput *input, const char *dir)
{
    eng_input_init(input);
    eng_input_set_default_bindings(input);
    return play_mode_init(p, NULL, 1.0F, false) && play_mode_enable_combat(p, false) &&
           game_flow_init(f, p, input, dir, false);
}

#define OK ENG_MENU_KEY_OK
#define BACK ENG_MENU_KEY_BACK
#define UP ENG_MENU_KEY_UP
#define DOWN ENG_MENU_KEY_DOWN
#define LEFT ENG_MENU_KEY_LEFT
#define RIGHT ENG_MENU_KEY_RIGHT

bool game_flow_selftest(void)
{
    g_ok = true;
    static PlayMode play;
    static GameFlow flow;
    static EngInput input;
    static EngSaveGame saved;
    static EngSaveGame one;
    static EngSaveGame two;
    char dir[600];
    char *pref = SDL_GetPrefPath("RedStrings", "selftest");
    (void)snprintf(dir, sizeof dir, "%s", pref != NULL ? pref : "./");
    SDL_free(pref);
    char path[1100];
    (void)snprintf(path, sizeof path, "%ssavegame.rssv", dir);
    (void)SDL_RemovePath(path);
    for (unsigned i = 1; i <= ENG_MENU_SLOTS; i++) {
        (void)snprintf(path, sizeof path, "%spartie%u.rssv", dir, i);
        (void)SDL_RemovePath(path);
        (void)snprintf(path, sizeof path, "%spartie%u.rssv.supprimee", dir, i);
        (void)SDL_RemovePath(path);
    }
    (void)snprintf(path, sizeof path, "%soptions.txt", dir);
    (void)SDL_RemovePath(path);
    if (!relaunch(&flow, &play, &input, dir)) {
        ENG_LOG_ERROR("AUTOTEST PARTIE : ÉCHEC (initialisation ; Jolt requis)");
        play_mode_shutdown(&play);
        return false;
    }
    char err[128];
    EngMenuItem items[ENG_MENU_MAX_ITEMS];

    /* ── écran-titre sans aucune partie : en créer une, avec un nom ── */
    check(flow.menu.screen == ENG_MENU_MAIN && flow.in_title, "écran-titre au lancement", 1);
    check(!flow.menu.has_save && !flow.options.fullscreen,
          "aucune partie ; pas de plein écran par défaut pour un test", 0);
    (void)eng_menu_items(&flow.menu, items, ENG_MENU_MAX_ITEMS);
    check(strcmp(items[0].label, "Nouvelle partie") == 0, "sans partie : « Nouvelle partie »", 1);
    idle(&flow, &play, 2);
    key(&flow, &play, BACK);
    check(flow.menu.screen == ENG_MENU_MAIN && !flow.quit,
          "Échap à l'écran-titre : rien ne se ferme", 0);
    key(&flow, &play, OK);
    check(flow.menu.screen == ENG_MENU_NAME && strcmp(flow.menu.text, "Partie 1") == 0 &&
              game_flow_wants_text(&flow),
          "nouvelle partie : saisie du nom, « Partie 1 » proposé", 1);
    /* les touches du jeu sont du texte ici : Espace (Saut) et P (Pause) ne commandent rien */
    tick(&flow, &play, (1U << ENG_ACTION_JUMP) | (1U << ENG_ACTION_PAUSE), 0.0F, -1.0F, 0U);
    idle(&flow, &play, 2);
    check(flow.menu.screen == ENG_MENU_NAME && flow.menu.selected == 1,
          "saisie : Espace, P et ZQSD ne valident ni ne quittent", (double)flow.menu.selected);
    game_flow_text(&flow, "Théo");
    check(strcmp(flow.menu.text, "Théo") == 0,
          "saisie : la première frappe remplace le nom proposé", (double)strlen(flow.menu.text));
    game_flow_text(&flow, "x\b\b\b");
    check(strcmp(flow.menu.text, "Th") == 0, "saisie : effacer retire un caractère entier (é)",
          (double)strlen(flow.menu.text));
    game_flow_text(&flow, "éo");
    key(&flow, &play, OK);
    check(!eng_menu_is_open(&flow.menu) && flow.started && !flow.in_title && flow.slot == 0 &&
              strcmp(flow.name, "Théo") == 0,
          "partie « Théo » lancée dans l'emplacement 1", (double)flow.slot);
    check(file_exists(flow.slot_path[0]) && flow.menu.slots[0].used &&
              strcmp(flow.menu.slots[0].name, "Théo") == 0,
          "enregistrée aussitôt, avec son nom", 1);
    /* jouer : marcher 2 s */
    float before[3];
    (void)eng_character_position(play.character, before);
    for (int i = 0; i < 120; i++) {
        tick(&flow, &play, 0U, 0.0F, 1.0F, 0U);
    }
    idle(&flow, &play, 30);
    float feet[3];
    (void)eng_character_position(play.character, feet);
    check(hypotf(feet[0] - before[0], feet[2] - before[2]) > 3.0F, "le personnage a marché (m)",
          hypotf(feet[0] - before[0], feet[2] - before[2]));
    /* Échap ouvre la pause ; Sauvegarder */
    key(&flow, &play, BACK);
    check(flow.menu.screen == ENG_MENU_PAUSE, "en jeu : Échap ouvre la pause", 1);
    key(&flow, &play, BACK);
    check(!eng_menu_is_open(&flow.menu), "pause : Échap reprend la partie", 0);
    tick(&flow, &play, 1U << ENG_ACTION_PAUSE, 0.0F, 0.0F, 0U);
    idle(&flow, &play, 2);
    check(flow.menu.screen == ENG_MENU_PAUSE, "en jeu : P ouvre aussi la pause", 1);
    key(&flow, &play, DOWN);
    key(&flow, &play, OK);
    check(flow.menu.screen == ENG_MENU_PAUSE && strstr(flow.menu.status, "Théo") != NULL,
          "Sauvegarder : la pause reste ouverte, message affiché", 1);
    const bool reread = eng_save_read_file(flow.save_path, &saved, NULL, err, sizeof err);
    check(reread && saved.has_meta && strcmp(saved.name, "Théo") == 0 &&
              saved.saved_at > 1600000000,
          "fichier relu : nom et date enregistrés (année)",
          1970.0 + ((double)saved.saved_at / 31556952.0));
    double time_saved = flow.play_time;
    double hour_saved = flow.daynight.hour;
    check(saved.has_time && fabs(saved.hour - hour_saved) < 1e-9, "heure sauvegardée (h)",
          saved.hour);
    /* on rejoue un peu, puis « Sauvegarder et quitter » : c'est CET état qui est enregistré */
    key(&flow, &play, BACK);
    for (int i = 0; i < 60; i++) {
        tick(&flow, &play, 0U, 0.0F, 1.0F, 0U);
    }
    idle(&flow, &play, 30);
    key(&flow, &play, BACK);
    float quit_at[3];
    (void)eng_character_position(play.character, quit_at);
    const double first_x = saved.player_pos[0];
    const double first_time = time_saved;
    keys(&flow, &play, DOWN, 3);
    key(&flow, &play, OK);
    check(flow.menu.screen == ENG_MENU_MAIN && flow.in_title,
          "« Sauvegarder et quitter » : retour à l'écran-titre", 1);
    const bool rewritten = eng_save_read_file(flow.save_path, &saved, NULL, err, sizeof err);
    check(rewritten && fabsf((float)saved.player_pos[0] - quit_at[0]) < 1e-3F &&
              fabs(saved.player_pos[0] - first_x) > 1.0 && saved.play_time > first_time + 1.0,
          "« Sauvegarder et quitter » : l'état du moment est enregistré (x)", quit_at[0]);
    time_saved = saved.play_time; /* ce que « Continuer » doit restituer */
    hour_saved = saved.hour;
    play_mode_shutdown(&play);

    /* ── relance : Continuer ── */
    if (!relaunch(&flow, &play, &input, dir)) {
        ENG_LOG_ERROR("AUTOTEST PARTIE : ÉCHEC (relance)");
        return false;
    }
    (void)eng_menu_items(&flow.menu, items, ENG_MENU_MAX_ITEMS);
    check(flow.menu.has_save && strcmp(items[0].label, "Continuer") == 0 &&
              strstr(items[0].value, "Théo") != NULL,
          "après relance : « Continuer » nomme la partie", 1);
    idle(&flow, &play, 2);
    tick(&flow, &play, 0U, 0.0F, 0.0F, OK); /* Continuer ; aucun tick de jeu avant les mesures */
    check(!eng_menu_is_open(&flow.menu) && strcmp(flow.name, "Théo") == 0, "partie reprise", 1);
    float after[3];
    (void)eng_character_position(play.character, after);
    check(fabsf(after[0] - (float)saved.player_pos[0]) < 1e-3F &&
              fabsf(after[2] - (float)saved.player_pos[2]) < 1e-3F,
          "position restituée (écart, m)",
          hypotf(after[0] - (float)saved.player_pos[0], after[2] - (float)saved.player_pos[2]));
    check(fabs(flow.play_time - time_saved) < 1e-6, "temps de jeu restitué (s)", flow.play_time);
    check(fabs(flow.daynight.hour - hour_saved) < 1e-9, "heure du jour restituée (h)",
          flow.daynight.hour);
    static EngSaveGame now;
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
    key(&flow, &play, BACK);
    keys(&flow, &play, DOWN, 2);
    key(&flow, &play, OK); /* Options */
    check(flow.menu.screen == ENG_MENU_OPTIONS, "pause : Options", 1);
    key(&flow, &play, LEFT); /* volume général -10 % */
    const float master = flow.options.master;
    key(&flow, &play, BACK);
    check(flow.menu.screen == ENG_MENU_PAUSE && flow.menu.selected == 2,
          "options : Échap revient à la pause, sur « Options »", (double)flow.menu.selected);
    play_mode_shutdown(&play);
    (void)relaunch(&flow, &play, &input, dir);
    check(fabsf(flow.options.master - master) < 1e-3F && master < 0.8F,
          "options relues après relance", flow.options.master);

    /* ── Parties : créer dans un emplacement libre ── */
    check(flow.menu.slots[0].used && !flow.menu.slots[1].used && !flow.menu.slots[2].used &&
              flow.menu.last_slot == 0,
          "parties : seul l'emplacement 1 est occupé", (double)flow.menu.last_slot);
    check(strstr(flow.menu.slots[0].summary, "Niv. ") != NULL && flow.menu.slots[0].date[0] != '\0',
          "parties : résumé et date de la partie 1", 1);
    idle(&flow, &play, 2);
    key(&flow, &play, RIGHT);
    key(&flow, &play, OK);
    check(flow.menu.screen == ENG_MENU_SAVES && flow.menu.selected == 0,
          "« Parties » : les emplacements, curseur sur la plus récente",
          (double)flow.menu.selected);
    key(&flow, &play, RIGHT);
    key(&flow, &play, OK);
    check(flow.menu.screen == ENG_MENU_NAME && flow.menu.slot == 1 &&
              strcmp(flow.menu.text, "Partie 2") == 0,
          "emplacement libre : créer une partie, « Partie 2 » proposé", (double)flow.menu.slot);
    game_flow_text(&flow, "\b");
    key(&flow, &play, OK);
    check(flow.menu.screen == ENG_MENU_NAME && flow.menu.text[0] == '\0' &&
              !file_exists(flow.slot_path[1]),
          "nom vide : refusé, rien n'est créé", 0);
    game_flow_text(&flow, "Deux");
    /* le mode de jeu : première ligne de la création ; cette partie sera créative */
    key(&flow, &play, UP);
    key(&flow, &play, RIGHT);
    key(&flow, &play, DOWN);
    check(flow.menu.creative && flow.menu.selected == 1 && !play.creative,
          "création : mode créatif choisi (rien ne change avant de créer)",
          (double)flow.menu.creative);
    key(&flow, &play, OK);
    check(!eng_menu_is_open(&flow.menu) && flow.started && flow.slot == 1 &&
              strcmp(flow.name, "Deux") == 0,
          "partie « Deux » dans l'emplacement 2", (double)flow.slot);
    check(flow.creative && play.creative && flow.menu.slots[1].creative &&
              !flow.menu.slots[0].creative,
          "partie créative : vol et invincibilité dès le départ, mode enregistré",
          (double)play.creative);
    check(file_exists(flow.slot_path[1]) && flow.menu.slots[1].used && flow.menu.last_slot == 1,
          "enregistrée aussitôt ; c'est la plus récente", (double)flow.menu.last_slot);
    for (int i = 0; i < 90; i++) {
        tick(&flow, &play, 0U, 0.0F, 1.0F, 0U);
    }
    idle(&flow, &play, 30);
    key(&flow, &play, BACK);
    key(&flow, &play, DOWN);
    key(&flow, &play, OK); /* Sauvegarder : dans l'emplacement 2 */
    float here[3];
    (void)eng_character_position(play.character, here);
    check(eng_save_read_file(flow.slot_path[1], &two, NULL, err, sizeof err) &&
              fabsf((float)two.player_pos[0] - here[0]) < 1e-3F &&
              hypot(two.player_pos[0] - saved.player_pos[0],
                    two.player_pos[2] - saved.player_pos[2]) > 1.0,
          "« Sauvegarder » écrit l'emplacement de la partie en cours", two.player_pos[0]);
    check(eng_save_read_file(flow.slot_path[0], &one, NULL, err, sizeof err) &&
              one.player_pos[0] == saved.player_pos[0] && one.play_time == saved.play_time,
          "la partie 1 n'a pas bougé", one.player_pos[0]);
    keys(&flow, &play, DOWN, 2);
    key(&flow, &play, OK); /* Sauvegarder et quitter */
    (void)eng_menu_items(&flow.menu, items, ENG_MENU_MAX_ITEMS);
    check(flow.menu.screen == ENG_MENU_MAIN && strstr(items[0].value, "Deux") != NULL,
          "écran-titre : « Continuer » vise la partie la plus récente",
          (double)flow.menu.last_slot);

    /* ── Parties : renommer, dupliquer, supprimer ── */
    key(&flow, &play, RIGHT);
    key(&flow, &play, OK); /* Parties : curseur sur l'emplacement 2 */
    key(&flow, &play, OK);
    (void)eng_menu_items(&flow.menu, items, ENG_MENU_MAX_ITEMS);
    check(flow.menu.screen == ENG_MENU_SLOT && flow.menu.slot == 1 &&
              strcmp(items[0].label, "Jouer") == 0 && items[2].enabled,
          "emplacement occupé : Jouer, Renommer, Dupliquer, Supprimer", (double)flow.menu.slot);
    key(&flow, &play, DOWN);
    key(&flow, &play, OK);
    check(flow.menu.screen == ENG_MENU_NAME && strcmp(flow.menu.text, "Deux") == 0,
          "renommer : le nom actuel est proposé", 1);
    key(&flow, &play, BACK);
    check(flow.menu.screen == ENG_MENU_SLOT && strcmp(flow.menu.slots[1].name, "Deux") == 0,
          "renommer : Échap annule", 1);
    key(&flow, &play, OK);
    /* l'état du fichier juste avant de le renommer (il a été réécrit par « Sauvegarder et
     * quitter », peut-être à la seconde suivante : la date ne se compare qu'à celle-ci) */
    static EngSaveGame before_rename;
    const bool had_file =
        eng_save_read_file(flow.slot_path[1], &before_rename, NULL, err, sizeof err);
    game_flow_text(&flow, "Second souffle");
    key(&flow, &play, OK);
    static EngSaveGame renamed;
    check(flow.menu.screen == ENG_MENU_SLOT &&
              strcmp(flow.menu.slots[1].name, "Second souffle") == 0 &&
              eng_save_read_file(flow.slot_path[1], &renamed, NULL, err, sizeof err) &&
              strcmp(renamed.name, "Second souffle") == 0,
          "renommer : le fichier porte le nouveau nom", 1);
    check(had_file && strcmp(before_rename.name, "Deux") == 0 &&
              renamed.saved_at == before_rename.saved_at &&
              renamed.play_time == before_rename.play_time &&
              renamed.player_pos[0] == before_rename.player_pos[0] &&
              renamed.hp == before_rename.hp && renamed.play_time == two.play_time,
          "renommer : date, temps de jeu et état intacts", renamed.play_time);
    key(&flow, &play, DOWN);
    key(&flow, &play, OK); /* Dupliquer */
    static EngSaveGame copy;
    check(flow.menu.screen == ENG_MENU_SAVES && flow.menu.selected == 2 &&
              flow.menu.slots[2].used && flow.menu.slots[2].creative &&
              strcmp(flow.menu.slots[2].name, "Second souffle (copie)") == 0,
          "dupliquer : la copie est dans l'emplacement 3, nommée « (copie) »",
          (double)flow.menu.selected);
    check(eng_save_read_file(flow.slot_path[2], &copy, NULL, err, sizeof err) &&
              copy.play_time == renamed.play_time && copy.player_pos[0] == renamed.player_pos[0] &&
              flow.menu.last_slot == 1,
          "dupliquer : même état ; « Continuer » reste sur l'original",
          (double)flow.menu.last_slot);
    check(copy.saved_at == renamed.saved_at - 1,
          "dupliquer : la copie est datée une seconde avant l'original",
          (double)(renamed.saved_at - copy.saved_at));
    key(&flow, &play, OK); /* l'emplacement 3 */
    (void)eng_menu_items(&flow.menu, items, ENG_MENU_MAX_ITEMS);
    check(flow.menu.screen == ENG_MENU_SLOT && !items[2].enabled,
          "plus d'emplacement libre : « Dupliquer » est grisé", 0);
    keys(&flow, &play, DOWN, 2); /* Renommer, puis Supprimer (Dupliquer, grisé, est sauté) */
    check(flow.menu.selected == 3, "le curseur saute la ligne grisée", (double)flow.menu.selected);
    key(&flow, &play, OK);
    check(flow.menu.screen == ENG_MENU_DELETE && flow.menu.selected == 0,
          "supprimer : confirmation demandée, curseur sur « Non »", (double)flow.menu.selected);
    key(&flow, &play, OK);
    check(flow.menu.screen == ENG_MENU_SLOT && file_exists(flow.slot_path[2]),
          "supprimer : « Non » ne touche à rien", 1);
    key(&flow, &play, OK);
    key(&flow, &play, DOWN);
    key(&flow, &play, OK);
    (void)snprintf(path, sizeof path, "%s.supprimee", flow.slot_path[2]);
    check(flow.menu.screen == ENG_MENU_SAVES && !flow.menu.slots[2].used &&
              !file_exists(flow.slot_path[2]) && file_exists(path),
          "supprimer : l'emplacement 3 est libre, le fichier est mis de côté", 1);
    check(file_exists(flow.slot_path[0]) && file_exists(flow.slot_path[1]),
          "supprimer : les autres parties sont intactes", 1);
    /* jouer la partie 1 depuis la liste */
    keys(&flow, &play, LEFT, 2);
    key(&flow, &play, OK);
    tick(&flow, &play, 0U, 0.0F, 0.0F, OK); /* Jouer */
    (void)eng_character_position(play.character, here);
    check(!eng_menu_is_open(&flow.menu) && flow.slot == 0 && strcmp(flow.name, "Théo") == 0 &&
              fabsf(here[0] - (float)saved.player_pos[0]) < 1e-3F &&
              fabsf(here[2] - (float)saved.player_pos[2]) < 1e-3F,
          "jouer la partie 1 : son nom et sa position (x)", here[0]);
    check(!flow.creative && !play.creative, "jouer une partie d'aventure : plus de mode créatif",
          (double)play.creative);

    /* ── fondus : l'action attend la fin du fondu de sortie ── */
    key(&flow, &play, BACK);
    keys(&flow, &play, DOWN, 3);
    game_flow_set_transitions(&flow, true, false);
    key(&flow, &play, OK); /* Sauvegarder et quitter */
    check(flow.pending == ENG_MENU_ACT_TO_MAIN && !flow.in_title,
          "fondus : le retour au titre attend le fondu de sortie", (double)flow.cover_ticks);
    idle(&flow, &play, 30);
    check(flow.pending == ENG_MENU_ACT_NONE && flow.in_title && flow.menu.screen == ENG_MENU_MAIN &&
              flow.fade > 0.0F,
          "fondus : écran-titre atteint, fondu d'entrée lancé", (double)flow.fade);
    key(&flow, &play, OK); /* Continuer */
    check(flow.pending == ENG_MENU_ACT_CONTINUE && flow.in_title &&
              game_flow_title_outro(&flow) > 0.0F && game_flow_title_outro(&flow) < 0.2F,
          "fondus : la scène du titre commence à s'éteindre", (double)game_flow_title_outro(&flow));
    idle(&flow, &play, 60);
    check(flow.pending == ENG_MENU_ACT_NONE && !flow.in_title && flow.started,
          "fondus : la partie commence à la fin du fondu", (double)flow.cover_ticks);
    game_flow_set_transitions(&flow, false, false);
    /* le mode de jeu est celui de la partie : il revient avec elle, rien ne le change en jeu */
    const bool second = game_flow_load_slot(&flow, &play, 1);
    check(second && flow.creative && play.creative, "reprendre la partie créative : mode créatif",
          (double)play.creative);
    const bool first = game_flow_load_slot(&flow, &play, 0);
    check(first && !flow.creative && !play.creative, "reprendre la partie d'aventure : aventure",
          (double)play.creative);

    /* ── fichiers : sauvegarde illisible, ancienne sauvegarde unique ── */
    play_mode_shutdown(&play);
    const bool garbage = SDL_SaveFile(flow.slot_path[2], "pas une sauvegarde", 18);
    (void)relaunch(&flow, &play, &input, dir);
    check(garbage && flow.menu.slots[2].used && flow.menu.slots[2].unreadable &&
              flow.menu.has_save && flow.menu.last_slot != 2,
          "fichier illisible : occupé, jamais proposé à « Continuer »",
          (double)flow.menu.last_slot);
    key(&flow, &play, RIGHT);
    key(&flow, &play, OK);
    keys(&flow, &play, RIGHT, 2);
    key(&flow, &play, OK);
    (void)eng_menu_items(&flow.menu, items, ENG_MENU_MAX_ITEMS);
    check(flow.menu.screen == ENG_MENU_SLOT && !items[0].enabled && !items[1].enabled &&
              items[3].enabled && flow.menu.selected == 3,
          "fichier illisible : seule la suppression est proposée", (double)flow.menu.selected);
    play_mode_shutdown(&play);
    (void)snprintf(path, sizeof path, "%ssavegame.rssv", dir);
    const bool staged = SDL_CopyFile(flow.slot_path[1], path) && SDL_RemovePath(flow.slot_path[0]);
    (void)relaunch(&flow, &play, &input, dir);
    check(staged && flow.menu.slots[0].used && file_exists(path) &&
              eng_save_read_file(flow.slot_path[0], &one, NULL, err, sizeof err) &&
              one.player_pos[0] == two.player_pos[0],
          "ancienne sauvegarde copiée dans l'emplacement 1 (x)", one.player_pos[0]);
    /* il ne reste qu'un fichier illisible : rien à continuer, l'écran-titre propose une nouvelle
     * partie */
    play_mode_shutdown(&play);
    const bool alone = SDL_RemovePath(flow.slot_path[0]) && SDL_RemovePath(flow.slot_path[1]) &&
                       SDL_RemovePath(path);
    (void)relaunch(&flow, &play, &input, dir);
    (void)eng_menu_items(&flow.menu, items, ENG_MENU_MAX_ITEMS);
    check(alone && flow.menu.slots[2].unreadable && !flow.menu.has_save &&
              strcmp(items[0].label, "Nouvelle partie") == 0,
          "un fichier illisible seul : pas de « Continuer »", (double)flow.menu.has_save);
    (void)SDL_RemovePath(flow.slot_path[2]);
    /* ── la carte d'une partie : résumé, date, boss ; nom d'une copie ── */
    static EngSaveGame crafted;
    static EngMenuSlot card;
    int64_t when = 0;
    eng_save_defaults(&crafted);
    crafted.play_time = 7500.0; /* 2 h 05 */
    crafted.has_gear = true;
    crafted.gear_tier = 1;
    /* boss de la plaine (zone 2) et du désert (zone 8) vaincus ; la zone 3 n'est pas un boss */
    crafted.bosses_beaten = (1U << 2) | (1U << 8) | (1U << 3);
    (void)snprintf(path, sizeof path, "%sessai.rssv", dir);
    const bool crafted_ok = eng_save_write_file(path, &crafted, NULL);
    game_slot_describe(path, 2, 4, &card, &when);
    check(crafted_ok && card.used && !card.unreadable && strcmp(card.name, "Partie 3") == 0 &&
              strcmp(card.summary, "Niv. 1 \xC2\xB7 2 h 05") == 0,
          "carte : sans nom enregistré, « Partie 3 » ; résumé « Niv. 1 · 2 h 05 »", 1);
    check(card.boss_count == 4U && card.bosses == 0x5U,
          "carte : boss vaincus, un bit par boss (plaine et désert)", (double)card.bosses);
    check(when > 1600000000 && card.date[0] != '\0',
          "carte : sans date enregistrée, celle du fichier", 1970.0 + ((double)when / 31556952.0));
    game_slot_describe(path, 2, 0, &card, &when);
    check(card.boss_count == 0U && card.bosses == 0U,
          "carte : sans grande carte, aucun boss affiché", (double)card.bosses);
    crafted.has_meta = true;
    (void)snprintf(crafted.name, sizeof crafted.name, "ABCDEFGHIJKLMNOPQRSTUV"); /* 22 caractères */
    (void)snprintf(crafted.place, sizeof crafted.place, "Volcan");
    crafted.saved_at = 1795343215; /* le 22 novembre 2026 */
    (void)eng_save_write_file(path, &crafted, NULL);
    game_slot_describe(path, 0, 4, &card, &when);
    check(when == 1795343215 && strstr(card.date, "nov. 2026") != NULL &&
              strcmp(card.place, "Volcan") == 0 && !card.creative,
          "carte : nom, lieu et date enregistrés", (double)when);
    crafted.creative = true;
    (void)eng_save_write_file(path, &crafted, NULL);
    game_slot_describe(path, 0, 4, &card, &when);
    check(card.creative, "carte : partie en mode créatif", (double)card.creative);
    char copy_path[1100];
    (void)snprintf(copy_path, sizeof copy_path, "%sessai-copie.rssv", dir);
    const bool copied = game_slot_copy(path, copy_path);
    game_slot_describe(copy_path, 1, 4, &card, &when);
    check(copied && strcmp(card.name, "ABCDEFGHIJKLMN (copie)") == 0,
          "copie d'un nom de 22 caractères : raccourci pour garder « (copie) »",
          (double)strlen(card.name));
    /* 13 lettres puis cinq « é » (deux octets chacun) : la coupure tombe au milieu des accents */
    (void)snprintf(crafted.name, sizeof crafted.name,
                   "Aaaaaaaaaaaaa\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9");
    (void)eng_save_write_file(path, &crafted, NULL);
    (void)SDL_RemovePath(copy_path);
    const bool copied_accents = game_slot_copy(path, copy_path);
    game_slot_describe(copy_path, 1, 4, &card, &when);
    check(copied_accents && strcmp(card.name, "Aaaaaaaaaaaaa\xC3\xA9 (copie)") == 0,
          "copie d'un nom accentué : aucun caractère coupé", (double)strlen(card.name));
    char empty_date[8] = "x";
    game_slot_date(0, empty_date, sizeof empty_date);
    check(empty_date[0] == '\0', "date inconnue : chaîne vide", 0);
    (void)SDL_RemovePath(path);
    (void)SDL_RemovePath(copy_path);
    game_slot_describe(path, 0, 4, &card, &when);
    check(!card.used, "fichier absent : emplacement libre", 0);

    /* ── plein écran par défaut : seulement quand les options n'en disent rien ── */
    play_mode_shutdown(&play);
    (void)snprintf(path, sizeof path, "%soptions.txt", dir);
    (void)SDL_RemovePath(path);
    eng_input_init(&input);
    (void)play_mode_init(&play, NULL, 1.0F, false);
    (void)play_mode_enable_combat(&play, false);
    (void)game_flow_init(&flow, &play, &input, dir, true);
    check(flow.options.fullscreen, "sans options enregistrées : plein écran (lancement à la main)",
          (double)flow.options.fullscreen);
    play_mode_shutdown(&play);
    const bool wrote = SDL_SaveFile(path, "graphics.fullscreen = 0\n", 24);
    eng_input_init(&input);
    (void)play_mode_init(&play, NULL, 1.0F, false);
    (void)play_mode_enable_combat(&play, false);
    (void)game_flow_init(&flow, &play, &input, dir, true);
    check(wrote && !flow.options.fullscreen, "options enregistrées « fenêtre » : respectées",
          (double)flow.options.fullscreen);
    (void)SDL_RemovePath(path);

    /* ── rappel des commandes : les touches de la vitesse de vol, telles qu'elles sont reliées ──
     */
    char fly[72];
    eng_input_set_default_bindings(&input);
    game_flow_fly_keys(&input, fly, sizeof fly);
    check(strcmp(fly, "molette") == 0, "vitesse de vol : par défaut, « molette »",
          (double)strlen(fly));
    eng_input_clear_action(&input, ENG_ACTION_FLY_FASTER);
    (void)eng_input_bind_button(&input, ENG_ACTION_FLY_FASTER, ENG_BIND_KEY, ENG_KEY_UP);
    game_flow_fly_keys(&input, fly, sizeof fly);
    check(strcmp(fly, "Fl\xC3\xA8"
                      "che haut / molette bas") == 0,
          "vitesse de vol : une action sur une touche : les deux sont nommées",
          (double)strlen(fly));
    eng_input_clear_action(&input, ENG_ACTION_FLY_SLOWER);
    game_flow_fly_keys(&input, fly, sizeof fly);
    check(strcmp(fly, "Fl\xC3\xA8"
                      "che haut / aucune") == 0,
          "vitesse de vol : une action sans liaison : « aucune »", (double)strlen(fly));
    game_flow_fly_keys(NULL, fly, sizeof fly);
    check(fly[0] == '\0', "vitesse de vol : sans liaisons, rien", (double)strlen(fly));
    char untouched[4] = "abc";
    game_flow_fly_keys(&input, untouched, 0U);
    check(strcmp(untouched, "abc") == 0, "vitesse de vol : tampon vide laissé tel quel", 0);
    ENG_LOG_INFO("AUTOTEST PARTIE : %s", g_ok ? "OK" : "ÉCHEC");
    play_mode_shutdown(&play);
    return g_ok;
}
