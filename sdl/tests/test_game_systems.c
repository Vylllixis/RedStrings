/* Sauvegarde (M8-05), menus et options (M8-04, M8-10), bus audio et son 3D (M8-01). */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/game_menu.h"
#include "engine/mixer.h"
#include "engine/save_game.h"
#include "utest.h"

static EngWorldState g_world;
static EngWorldState g_world2;
static uint8_t g_buf[262144];

static EngSaveGame sample(void)
{
    EngSaveGame s;
    eng_save_defaults(&s);
    s.player_pos[0] = 10240.25;
    s.player_pos[1] = 3.5;
    s.player_pos[2] = -812.125;
    s.player_yaw = 1.25F;
    s.hp = 63.0F;
    s.hp_max = 120.0F;
    s.stamina = 40.0F;
    s.posture = 75.0F;
    s.play_time = 3605.5;
    s.has_enemies = true;
    s.enemy_count = 3;
    for (uint32_t i = 0; i < 3U; i++) {
        s.enemies[i].alive = i != 1U;
        s.enemies[i].hp = 10.0F * (float)(i + 1U);
        s.enemies[i].pos[0] = -14.0F + (float)i;
        s.enemies[i].pos[1] = -11.0F;
        s.enemies[i].yaw = 0.5F;
    }
    return s;
}

UTEST(save_game, a_save_round_trips_exactly)
{
    eng_wstate_init(&g_world);
    ASSERT_TRUE(eng_wstate_remove_object(&g_world, (EngCellCoord){40, -4}, 7));
    ASSERT_TRUE(eng_wstate_defeat_camp(&g_world, (EngCellCoord){40, -4}));
    ASSERT_TRUE(eng_wstate_remove_object(&g_world, (EngCellCoord){-3, 9}, 63));
    const EngSaveGame s = sample();
    const size_t n = eng_save_encode(&s, &g_world, g_buf, sizeof g_buf);
    ASSERT_GT(n, 16U);
    EngSaveGame r;
    char err[96];
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, &g_world2, err, sizeof err));
    EXPECT_EQ(r.version_read, ENG_SAVE_VERSION);
    EXPECT_EQ(r.player_pos[0], s.player_pos[0]); /* double exact : pas d'arrondi à 10 km */
    EXPECT_EQ(r.player_pos[2], s.player_pos[2]);
    EXPECT_EQ(r.player_yaw, s.player_yaw);
    EXPECT_EQ(r.hp, s.hp);
    EXPECT_EQ(r.hp_max, s.hp_max);
    EXPECT_EQ(r.posture, s.posture);
    EXPECT_EQ(r.play_time, s.play_time);
    EXPECT_TRUE(r.has_enemies);
    EXPECT_EQ(r.enemy_count, 3U);
    EXPECT_FALSE(r.enemies[1].alive);
    EXPECT_EQ(r.enemies[2].hp, 30.0F);
    EXPECT_TRUE(eng_wstate_object_removed(&g_world2, (EngCellCoord){40, -4}, 7));
    EXPECT_TRUE(eng_wstate_object_removed(&g_world2, (EngCellCoord){-3, 9}, 63));
    EXPECT_EQ(eng_wstate_camps_defeated(&g_world2, (EngCellCoord){40, -4}), 1U);
    EXPECT_EQ(g_world2.count, 2U);
}

UTEST(save_game, the_time_of_day_section_is_optional_and_validated)
{
    EngSaveGame s = sample();
    EngSaveGame r;
    char err[96];
    size_t n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_FALSE(r.has_time); /* section absente : l'appelant garde son heure par défaut */
    const size_t without = n;
    s.has_time = true;
    s.hour = 21.75;
    n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    EXPECT_EQ(n, without + 16U); /* étiquette + taille + f64 */
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_TRUE(r.has_time);
    EXPECT_EQ(r.hour, 21.75);
    EXPECT_EQ(r.hp, s.hp); /* le reste est intact */
    s.hour = 24.0;         /* hors de [0, 24[ : fichier refusé en entier */
    n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    EXPECT_FALSE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    s.hour = NAN;
    n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    EXPECT_FALSE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
}

UTEST(save_game, the_progress_section_round_trips_and_rejects_incoherent_values)
{
    EngSaveGame s = sample();
    EngSaveGame r;
    char err[96];
    size_t n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_FALSE(r.has_progress);
    EXPECT_EQ(r.progress.level, 1U); /* absente : niveau 1 */
    s.has_progress = true;
    s.progress = (EngProgress){7U, 42U, 2000U};
    s.has_time = true;
    s.hour = 6.5;
    n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_TRUE(r.has_progress);
    EXPECT_EQ(r.progress.level, 7U);
    EXPECT_EQ(r.progress.xp, 42U);
    EXPECT_EQ(r.progress.total_xp, 2000U);
    EXPECT_EQ(r.hour, 6.5);                       /* les deux sections facultatives ensemble */
    s.progress = (EngProgress){7U, 5000U, 6000U}; /* xp >= besoin du niveau 7 */
    n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    EXPECT_FALSE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    s.progress = (EngProgress){0U, 0U, 0U};
    n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    EXPECT_FALSE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
}

/* M8-08 : section EQUI : absente -> épée et bouclier ; aller-retour ; incohérente -> refusé. */
UTEST(save_game, the_equipment_section_round_trips_and_rejects_incoherent_values)
{
    EngSaveGame s = sample();
    EngSaveGame r;
    char err[96];
    size_t n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_FALSE(r.has_equipment);
    EXPECT_EQ(r.equipment.equipped, (uint32_t)ENG_WEAPON_SWORD);
    s.has_equipment = true;
    s.equipment = (EngEquipment){.owned = 3U, .equipped = ENG_WEAPON_GREATSWORD};
    n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_TRUE(r.has_equipment);
    EXPECT_EQ(r.equipment.owned, 3U);
    EXPECT_EQ(r.equipment.equipped, (uint32_t)ENG_WEAPON_GREATSWORD);
    s.equipment = (EngEquipment){.owned = 1U, .equipped = ENG_WEAPON_GREATSWORD}; /* non possédé */
    n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    EXPECT_FALSE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
}

/* Grande carte : section GEAR (palier de butin, boss vaincus) : absente -> palier 0, aucun boss ;
 * aller-retour ; palier hors bornes -> refusé. */
UTEST(save_game, the_gear_section_round_trips_and_rejects_an_unknown_tier)
{
    EngSaveGame s = sample();
    EngSaveGame r;
    char err[96];
    size_t n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_FALSE(r.has_gear);
    EXPECT_EQ(r.gear_tier, 0U);
    EXPECT_EQ(r.bosses_beaten, 0U);
    s.has_gear = true;
    s.gear_tier = 3U;
    const uint32_t two_bosses = (1U << 2U) | (1U << 8U); /* boss de la plaine et du désert */
    s.bosses_beaten = two_bosses;
    n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_TRUE(r.has_gear);
    EXPECT_EQ(r.gear_tier, 3U);
    EXPECT_EQ(r.bosses_beaten, two_bosses);
    s.gear_tier = 9U; /* palier inconnu */
    n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    EXPECT_FALSE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
}

UTEST(save_game, a_version_1_save_is_read_by_version_2_without_loss)
{
    eng_wstate_init(&g_world);
    ASSERT_TRUE(eng_wstate_remove_object(&g_world, (EngCellCoord){1, 2}, 3));
    const EngSaveGame s = sample();
    const size_t n = eng_save_encode_v1(&s, &g_world, g_buf, sizeof g_buf);
    ASSERT_GT(n, 16U);
    EngSaveGame r;
    char err[96];
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, &g_world2, err, sizeof err));
    EXPECT_EQ(r.version_read, 1U);
    /* tout ce que v1 contenait est relu à l'identique */
    EXPECT_EQ(r.player_pos[0], s.player_pos[0]);
    EXPECT_EQ(r.player_pos[1], s.player_pos[1]);
    EXPECT_EQ(r.player_yaw, s.player_yaw);
    EXPECT_EQ(r.hp, s.hp);
    EXPECT_EQ(r.stamina, s.stamina);
    EXPECT_TRUE(eng_wstate_object_removed(&g_world2, (EngCellCoord){1, 2}, 3));
    /* ce que v1 ne connaissait pas prend sa valeur par défaut */
    EXPECT_EQ(r.posture, 100.0F);
    EXPECT_EQ(r.hp_max, 100.0F);
    EXPECT_EQ(r.play_time, 0.0);
    EXPECT_FALSE(r.has_enemies);
    /* et une fois réécrit, c'est un fichier v2 */
    const size_t n2 = eng_save_encode(&r, &g_world2, g_buf, sizeof g_buf);
    EngSaveGame r2;
    ASSERT_TRUE(eng_save_decode(g_buf, n2, &r2, NULL, err, sizeof err));
    EXPECT_EQ(r2.version_read, 2U);
    EXPECT_EQ(r2.hp, s.hp);
}

UTEST(save_game, corrupted_truncated_future_and_unknown_sections)
{
    eng_wstate_init(&g_world);
    const EngSaveGame s = sample();
    size_t n = eng_save_encode(&s, &g_world, g_buf, sizeof g_buf);
    EngSaveGame r;
    eng_save_defaults(&r);
    r.hp = -1.0F;
    char err[96] = {0};
    g_buf[n - 3U] ^= 0x40U; /* un bit abîmé */
    EXPECT_FALSE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_NE(strstr(err, "contrôle"), NULL);
    EXPECT_EQ(r.hp, -1.0F); /* rien n'a été écrit */
    n = eng_save_encode(&s, &g_world, g_buf, sizeof g_buf);
    EXPECT_FALSE(eng_save_decode(g_buf, n - 5U, &r, NULL, err, sizeof err));
    g_buf[4] = 9; /* version 9 : plus récente que le jeu */
    EXPECT_FALSE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_NE(strstr(err, "récente"), NULL);
    EXPECT_FALSE(eng_save_decode("RSXX", 4, &r, NULL, err, sizeof err));
    EXPECT_EQ(eng_save_encode(&s, &g_world, g_buf, 20), 0U); /* place insuffisante */
    /* section inconnue ajoutée par une version future : ignorée */
    n = eng_save_encode(&s, &g_world, g_buf, sizeof g_buf);
    const uint8_t extra[12] = {'F', 'U', 'T', 'R', 4, 0, 0, 0, 1, 2, 3, 4};
    memcpy(g_buf + n, extra, sizeof extra);
    n += sizeof extra;
    g_buf[8] = 4; /* 4 sections */
    uint32_t h = 2166136261U;
    for (size_t i = 16; i < n; i++) {
        h = (h ^ g_buf[i]) * 16777619U;
    }
    for (int k = 0; k < 4; k++) {
        g_buf[12 + k] = (uint8_t)(h >> (8 * k));
    }
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_EQ(r.hp, s.hp);
}

UTEST(save_game, file_write_is_atomic_and_reads_back)
{
    const char *path = "tests_tmp_save.rssv";
    const EngSaveGame s = sample();
    eng_wstate_init(&g_world);
    ASSERT_TRUE(eng_save_write_file(path, &s, &g_world));
    EngSaveGame r;
    char err[96];
    ASSERT_TRUE(eng_save_read_file(path, &r, &g_world2, err, sizeof err));
    EXPECT_EQ(r.player_pos[0], s.player_pos[0]);
    SDL_PathInfo info;
    EXPECT_FALSE(SDL_GetPathInfo("tests_tmp_save.rssv.tmp", &info)); /* pas de reste temporaire */
    (void)SDL_RemovePath(path);
    EXPECT_FALSE(eng_save_read_file(path, &r, NULL, err, sizeof err));
}

/* ── menus ── */

static EngInputState none_in(void)
{
    EngInputState in;
    memset(&in, 0, sizeof in);
    return in;
}

static EngMenuAction press(EngMenu *m, EngAction a)
{
    EngInputState in = none_in();
    in.pressed = 1U << a;
    in.held = in.pressed;
    const EngMenuAction r = eng_menu_update(m, &in);
    EngInputState idle = none_in();
    (void)eng_menu_update(m, &idle);
    return r;
}

static EngMenuAction stick(EngMenu *m, float x, float y)
{
    EngInputState in = none_in();
    in.axis[ENG_AXIS_MOVE] = (EngAxisValue){x, y};
    const EngMenuAction r = eng_menu_update(m, &in);
    EngInputState idle = none_in();
    (void)eng_menu_update(m, &idle); /* le stick revient au centre */
    return r;
}

UTEST(game_menu, the_whole_menu_tree_is_reachable_without_a_mouse)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    static EngInput input;
    eng_input_init(&input);
    eng_input_set_default_bindings(&input);
    EngMenu m;
    eng_menu_init(&m, &opt, &input);
    eng_menu_open(&m, ENG_MENU_MAIN);
    m.has_save = false;
    EngInputState idle = none_in();
    (void)eng_menu_update(&m, &idle);
    EXPECT_EQ(m.selected, 1); /* « Continuer » grisé : sélection sur « Nouvelle partie » */
    (void)stick(&m, 0.0F, -1.0F);
    EXPECT_EQ(m.selected, 2); /* Options */
    EXPECT_EQ(press(&m, ENG_ACTION_JUMP), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_OPTIONS);
    EXPECT_EQ(stick(&m, -1.0F, 0.0F), ENG_MENU_ACT_OPTIONS_CHANGED); /* volume général -10 % */
    EXPECT_NEAR(opt.master, 0.7F, 1e-5F);
    for (int i = 0; i < 20; i++) {
        (void)stick(&m, -1.0F, 0.0F);
    }
    EXPECT_NEAR(opt.master, 0.0F, 1e-6F); /* borné à 0 */
    (void)press(&m, ENG_ACTION_DODGE);    /* retour */
    EXPECT_EQ(m.screen, ENG_MENU_MAIN);
    EXPECT_EQ(press(&m, ENG_ACTION_JUMP), ENG_MENU_ACT_NEW_GAME); /* sélection revenue au début */
    EXPECT_FALSE(eng_menu_is_open(&m));
    /* en jeu : Pause ouvre, Pause referme */
    EXPECT_EQ(press(&m, ENG_ACTION_PAUSE), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_PAUSE);
    EXPECT_EQ(press(&m, ENG_ACTION_PAUSE), ENG_MENU_ACT_RESUME);
    (void)press(&m, ENG_ACTION_PAUSE);
    (void)stick(&m, 0.0F, -1.0F);
    EXPECT_EQ(press(&m, ENG_ACTION_ATTACK), ENG_MENU_ACT_SAVE);
    EXPECT_EQ(m.screen, ENG_MENU_PAUSE); /* sauvegarder ne ferme pas la pause */
    (void)stick(&m, 0.0F, -1.0F);
    (void)stick(&m, 0.0F, -1.0F);
    EXPECT_EQ(press(&m, ENG_ACTION_JUMP), ENG_MENU_ACT_TO_MAIN);
    EXPECT_EQ(m.screen, ENG_MENU_MAIN);
    m.has_save = true;
    EXPECT_EQ(m.selected, 1);    /* « Continuer » était grisé à l'arrivée */
    (void)stick(&m, 0.0F, 1.0F); /* remonte d'une ligne */
    EXPECT_EQ(m.selected, 0);
    EXPECT_EQ(press(&m, ENG_ACTION_JUMP), ENG_MENU_ACT_CONTINUE);
}

UTEST(game_menu, every_action_can_be_remapped_from_the_controls_screen)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    static EngInput input;
    eng_input_init(&input);
    eng_input_set_default_bindings(&input);
    EngMenu m;
    eng_menu_init(&m, &opt, &input);
    eng_menu_open(&m, ENG_MENU_CONTROLS);
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    const uint32_t n = eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS);
    EXPECT_EQ(n, 11U); /* 9 actions (dont « Changer d'arme », M8-08) + Par défaut + Retour */
    for (int32_t a = 0; a < 8; a++) {
        m.selected = a;
        eng_input_key(&input, 44 /* espace : la touche qui valide */, true);
        EngInputState in = none_in();
        in.pressed = 1U << ENG_ACTION_JUMP;
        (void)eng_menu_update(&m, &in);
        eng_input_key(&input, 44, false);
        EXPECT_EQ(m.remap, a);
        (void)eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS);
        EXPECT_NE(strstr(items[a].value, "appuie"), NULL);
        eng_input_key(&input, 30 + a /* touches 1 à 8 */, true);
        EngInputState idle = none_in();
        EXPECT_EQ(eng_menu_update(&m, &idle), ENG_MENU_ACT_REMAPPED);
        eng_input_key(&input, 30 + a, false);
        EXPECT_EQ(m.remap, -1);
    }
    EXPECT_EQ(input.buttons[ENG_ACTION_ATTACK][0].kind, ENG_BIND_KEY);
    EXPECT_EQ(input.buttons[ENG_ACTION_ATTACK][0].code, 30);
    EXPECT_EQ(input.buttons[ENG_ACTION_ATTACK][1].kind,
              ENG_BIND_NONE);                /* l'ancienne liaison a disparu */
    eng_input_mouse_button(&input, 1, true); /* une touche de souris aussi */
    eng_input_mouse_button(&input, 1, false);
    /* sauvegarde / relecture des options et des liaisons */
    static char text[4096];
    opt.master = 0.3F;
    opt.invert_y = true;
    ASSERT_GT(eng_options_save(&opt, &input, text, sizeof text), 50U);
    EngGameOptions o2;
    eng_options_default(&o2);
    static EngInput in2;
    eng_input_init(&in2);
    eng_input_set_default_bindings(&in2);
    ASSERT_TRUE(eng_options_load(&o2, &in2, text));
    EXPECT_NEAR(o2.master, 0.3F, 1e-3F);
    EXPECT_TRUE(o2.invert_y);
    EXPECT_EQ(in2.buttons[ENG_ACTION_PAUSE][0].code, 37);
    EXPECT_EQ(in2.buttons[ENG_ACTION_PAUSE][1].kind, ENG_BIND_NONE);
    /* « Par défaut » (après les 9 actions) */
    m.selected = 9;
    EngInputState in = none_in();
    in.pressed = 1U << ENG_ACTION_JUMP;
    EXPECT_EQ(eng_menu_update(&m, &in), ENG_MENU_ACT_REMAPPED);
    EXPECT_EQ(input.buttons[ENG_ACTION_ATTACK][0].kind, ENG_BIND_MOUSE_BUTTON);
    /* valeurs hors bornes ignorées une par une */
    ASSERT_TRUE(eng_options_load(
        &o2, NULL, "volume.master = 7\nvolume.music = 0.25\nbogus = 1\nnot a line\n"));
    EXPECT_NEAR(o2.master, 0.3F, 1e-3F);
    EXPECT_NEAR(o2.volume[ENG_BUS_MUSIC], 0.25F, 1e-6F);
}

UTEST(game_menu, graphics_options_toggle_persist_and_controls_returns_to_its_line)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    EXPECT_TRUE(opt.fxaa);
    EXPECT_NEAR(opt.bloom, 0.5F, 1e-6F);
    static EngInput input;
    eng_input_init(&input);
    eng_input_set_default_bindings(&input);
    EngMenu m;
    eng_menu_init(&m, &opt, &input);
    eng_menu_open(&m, ENG_MENU_OPTIONS);
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    const uint32_t n = eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS);
    ASSERT_EQ(n, (uint32_t)ENG_BUS_COUNT + 15U);
    EXPECT_NE(strstr(items[ENG_BUS_COUNT + 5].label, "FXAA"), NULL);
    EXPECT_NE(strstr(items[ENG_BUS_COUNT + 6].label, "bloom"), NULL);
    m.selected = (int32_t)ENG_BUS_COUNT + 5;
    EXPECT_EQ(stick(&m, 1.0F, 0.0F), ENG_MENU_ACT_OPTIONS_CHANGED);
    EXPECT_FALSE(opt.fxaa);
    m.selected = (int32_t)ENG_BUS_COUNT + 6;
    for (int i = 0; i < 9; i++) {
        (void)stick(&m, 1.0F, 0.0F);
    }
    EXPECT_NEAR(opt.bloom, 1.0F, 1e-6F); /* borné à 1 */
    (void)stick(&m, -1.0F, 0.0F);
    EXPECT_NEAR(opt.bloom, 0.9F, 1e-5F);
    static char text[4096];
    ASSERT_GT(eng_options_save(&opt, &input, text, sizeof text), 50U);
    EXPECT_NE(strstr(text, "graphics.fxaa = 0"), NULL);
    EngGameOptions o2;
    eng_options_default(&o2);
    ASSERT_TRUE(eng_options_load(&o2, NULL, text));
    EXPECT_FALSE(o2.fxaa);
    EXPECT_NEAR(o2.bloom, 0.9F, 1e-3F);
    ASSERT_TRUE(eng_options_load(&o2, NULL, "graphics.bloom = 3\n")); /* hors bornes : ignoré */
    EXPECT_NEAR(o2.bloom, 0.9F, 1e-3F);
    /* M8 (finition) : vsync, plein écran, qualité des ombres, distance de vue */
    EXPECT_TRUE(opt.vsync);
    EXPECT_FALSE(opt.fullscreen);
    EXPECT_EQ(opt.shadow_quality, 2U);
    EXPECT_EQ(opt.view_distance, 1U);
    EXPECT_NE(strstr(items[ENG_BUS_COUNT + 7].label, "verticale"), NULL);
    EXPECT_NE(strstr(items[ENG_BUS_COUNT + 10].label, "Distance"), NULL);
    m.selected = (int32_t)ENG_BUS_COUNT + 7;
    EXPECT_EQ(stick(&m, 1.0F, 0.0F), ENG_MENU_ACT_OPTIONS_CHANGED);
    EXPECT_FALSE(opt.vsync);
    m.selected = (int32_t)ENG_BUS_COUNT + 8;
    (void)stick(&m, 1.0F, 0.0F);
    EXPECT_TRUE(opt.fullscreen);
    m.selected = (int32_t)ENG_BUS_COUNT + 9;
    (void)stick(&m, 1.0F, 0.0F); /* haut -> bas (boucle) */
    EXPECT_EQ(opt.shadow_quality, 0U);
    m.selected = (int32_t)ENG_BUS_COUNT + 10;
    (void)stick(&m, -1.0F, 0.0F); /* moyenne -> proche */
    EXPECT_EQ(opt.view_distance, 0U);
    /* finition M8-06 : SSAO, activé par défaut, enregistré et relu */
    EXPECT_TRUE(opt.ssao);
    EXPECT_NE(strstr(items[ENG_BUS_COUNT + 11].label, "SSAO"), NULL);
    m.selected = (int32_t)ENG_BUS_COUNT + 11;
    EXPECT_EQ(stick(&m, 1.0F, 0.0F), ENG_MENU_ACT_OPTIONS_CHANGED);
    EXPECT_FALSE(opt.ssao);
    ASSERT_GT(eng_options_save(&opt, &input, text, sizeof text), 50U);
    EXPECT_NE(strstr(text, "graphics.ssao = 0"), NULL);
    eng_options_default(&o2);
    EXPECT_TRUE(o2.ssao);
    ASSERT_TRUE(eng_options_load(&o2, NULL, text));
    EXPECT_FALSE(o2.ssao);
    /* échelle de résolution : 100 -> 75 -> 50 %, bornée, enregistrée et relue */
    EXPECT_NEAR(opt.render_scale, 1.0F, 0.0F);
    EXPECT_NE(strstr(items[ENG_BUS_COUNT + 12].label, "chelle"), NULL);
    m.selected = (int32_t)ENG_BUS_COUNT + 12;
    (void)stick(&m, -1.0F, 0.0F);
    EXPECT_NEAR(opt.render_scale, 0.75F, 1e-6F);
    (void)stick(&m, -1.0F, 0.0F);
    (void)stick(&m, -1.0F, 0.0F);
    EXPECT_NEAR(opt.render_scale, 0.5F, 1e-6F); /* borné, sans boucler */
    ASSERT_GT(eng_options_save(&opt, &input, text, sizeof text), 50U);
    EXPECT_NE(strstr(text, "graphics.render_scale = 0.50"), NULL);
    eng_options_default(&o2);
    ASSERT_TRUE(eng_options_load(&o2, NULL, text));
    EXPECT_NEAR(o2.render_scale, 0.5F, 1e-6F);
    ASSERT_TRUE(eng_options_load(&o2, NULL, "graphics.render_scale = 0.2\n")); /* ignoré */
    EXPECT_NEAR(o2.render_scale, 0.5F, 1e-6F);
    ASSERT_GT(eng_options_save(&opt, &input, text, sizeof text), 50U);
    EXPECT_NE(strstr(text, "graphics.vsync = 0"), NULL);
    EXPECT_NE(strstr(text, "graphics.shadows = 0"), NULL);
    eng_options_default(&o2);
    ASSERT_TRUE(eng_options_load(&o2, NULL, text));
    EXPECT_FALSE(o2.vsync);
    EXPECT_TRUE(o2.fullscreen);
    EXPECT_EQ(o2.shadow_quality, 0U);
    EXPECT_EQ(o2.view_distance, 0U);
    ASSERT_TRUE(eng_options_load(&o2, NULL, "graphics.shadows = 7\n")); /* hors bornes */
    EXPECT_EQ(o2.shadow_quality, 0U);
    /* Contrôles... puis Retour : la sélection revient sur la ligne Contrôles... */
    m.selected = (int32_t)n - 2;
    EXPECT_EQ(press(&m, ENG_ACTION_JUMP), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_CONTROLS);
    m.selected = 10; /* Retour (9 actions remappables, Par défaut, Retour) */
    EXPECT_EQ(press(&m, ENG_ACTION_JUMP), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_OPTIONS);
    EXPECT_EQ(m.selected, (int32_t)n - 2);
}

/* ── audio ── */

UTEST(mixer_bus, bus_volume_applies_live_and_independently)
{
    static EngMixer m;
    ASSERT_TRUE(eng_mixer_init(&m, 48000));
    static float one[4800];
    for (uint32_t i = 0; i < 4800U; i++) {
        one[i] = 0.5F;
    }
    ASSERT_GE(eng_mixer_play_bus(&m, ENG_BUS_MUSIC, one, 4800, 1.0F, 0.0F, 0), 0);
    ASSERT_GE(eng_mixer_play_bus(&m, ENG_BUS_SFX, one, 4800, 1.0F, 0.0F, 0), 0);
    static float out[2 * 480];
    eng_mixer_mix(&m, out, 480);
    const float both = out[0];
    EXPECT_TRUE(eng_mixer_set_bus(&m, ENG_BUS_MUSIC, 0.0F)); /* musique coupée pendant la lecture */
    eng_mixer_mix(&m, out, 480);
    EXPECT_NEAR(out[0], both * 0.5F, 1e-5F);
    EXPECT_TRUE(eng_mixer_set_bus(&m, ENG_BUS_SFX, 0.5F));
    eng_mixer_mix(&m, out, 480);
    EXPECT_NEAR(out[0], both * 0.25F, 1e-5F);
    EXPECT_FALSE(eng_mixer_set_bus(&m, ENG_BUS_SFX, 1.5F));
    EXPECT_FALSE(eng_mixer_set_bus(&m, ENG_BUS_COUNT, 0.5F));
    EXPECT_EQ(eng_mixer_play_bus(&m, ENG_BUS_COUNT, one, 10, 1.0F, 0.0F, 0), -1);
}

UTEST(mixer_bus, a_sound_is_localisable_left_right_front_back_and_fades_with_distance)
{
    const float me[2] = {0.0F, 0.0F};
    float g = 0.0F;
    float p = 0.0F;
    const float right[2] = {5.0F, 0.0F}; /* regard vers -Z : +X est à droite */
    eng_audio_spatialize(me, 0.0F, right, 1.0F, 50.0F, &g, &p);
    EXPECT_NEAR(p, 1.0F, 1e-5F);
    EXPECT_NEAR(g, 0.2F, 1e-5F);
    const float left[2] = {-5.0F, 0.0F};
    eng_audio_spatialize(me, 0.0F, left, 1.0F, 50.0F, &g, &p);
    EXPECT_NEAR(p, -1.0F, 1e-5F);
    const float front[2] = {0.0F, -2.0F};
    const float back[2] = {0.0F, 2.0F};
    float gf = 0.0F;
    float gb = 0.0F;
    eng_audio_spatialize(me, 0.0F, front, 1.0F, 50.0F, &gf, &p);
    EXPECT_NEAR(p, 0.0F, 1e-5F);
    eng_audio_spatialize(me, 0.0F, back, 1.0F, 50.0F, &gb, &p);
    EXPECT_LT(gb, gf); /* derrière : légèrement plus faible */
    eng_audio_spatialize(me, 1.5707963F, right, 1.0F, 50.0F, &g,
                         &p); /* tourné vers -X : +X derrière */
    EXPECT_NEAR(p, 0.0F, 1e-4F);
    const float close[2] = {0.5F, 0.0F};
    eng_audio_spatialize(me, 0.0F, close, 1.0F, 50.0F, &g, &p);
    EXPECT_NEAR(g, 1.0F, 1e-6F);
    const float far_s[2] = {60.0F, 0.0F};
    eng_audio_spatialize(me, 0.0F, far_s, 1.0F, 50.0F, &g, &p);
    EXPECT_NEAR(g, 0.0F, 1e-9F);
    eng_audio_spatialize(me, 0.0F, far_s, 0.0F, 50.0F, &g, &p);
    EXPECT_NEAR(g, 0.0F, 1e-9F);
}
