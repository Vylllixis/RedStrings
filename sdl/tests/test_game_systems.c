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

UTEST(save_game, the_meta_section_names_the_game_and_is_optional)
{
    EngSaveGame s = sample();
    /* sans META : rien n'est inventé */
    size_t n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    ASSERT_GT(n, 0U);
    EngSaveGame r;
    char err[128];
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_FALSE(r.has_meta);
    EXPECT_STREQ(r.name, "");
    EXPECT_EQ(r.saved_at, 0);
    /* nom (accents compris), lieu et date reviennent à l'identique */
    s.has_meta = true;
    (void)snprintf(s.name, sizeof s.name, "Th\xC3\xA9o, l'\xC3\xA9t\xC3\xA9");
    (void)snprintf(s.place, sizeof s.place, "D\xC3\xA9sert");
    s.saved_at = 1795343215LL + (5LL << 32); /* plus de 32 bits : les deux moitiés comptent */
    const size_t with_meta = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    ASSERT_GT(with_meta, n);
    ASSERT_TRUE(eng_save_decode(g_buf, with_meta, &r, NULL, err, sizeof err));
    EXPECT_TRUE(r.has_meta);
    EXPECT_STREQ(r.name, s.name);
    EXPECT_STREQ(r.place, s.place);
    EXPECT_EQ(r.saved_at, s.saved_at);
    EXPECT_FALSE(r.creative);
    EXPECT_NEAR(r.play_time, s.play_time, 0.0); /* le reste de la partie n'a pas bougé */
    /* le mode de jeu (choisi à la création) est enregistré avec la partie */
    s.creative = true;
    const size_t creative_size = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    ASSERT_EQ(creative_size, with_meta);
    ASSERT_TRUE(eng_save_decode(g_buf, creative_size, &r, NULL, err, sizeof err));
    EXPECT_TRUE(r.creative);
    EXPECT_STREQ(r.name, s.name);
    s.creative = false;
    EXPECT_EQ(r.enemy_count, 3U);
    /* le nom le plus long accepté (31 octets) passe ; un texte sans zéro final n'est pas écrit */
    memset(s.name, 'x', sizeof s.name - 1U);
    s.name[sizeof s.name - 1U] = '\0';
    n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    ASSERT_GT(n, 0U);
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_EQ(strlen(r.name), sizeof r.name - 1U);
    memset(s.name, 'x', sizeof s.name);
    EXPECT_EQ(eng_save_encode(&s, NULL, g_buf, sizeof g_buf), 0U);
}

/* Une sauvegarde dont la section META est fabriquée à la main : `name_len` octets de `name`
 * annoncés comme nom (ou `announced` s'il est donné), un lieu vide, une date. La section est
 * ajoutée à une sauvegarde sans META ; le nombre de sections et la somme de contrôle sont refaits.
 */
static size_t forged_meta(const char *name, uint32_t name_len, uint32_t announced)
{
    const EngSaveGame s = sample();
    size_t n = eng_save_encode(&s, NULL, g_buf, sizeof g_buf);
    if (n == 0U || n + 64U + name_len > sizeof g_buf) {
        return 0;
    }
    const uint32_t size = 4U + name_len + 4U + 8U;
    memcpy(&g_buf[n], "META", 4);
    g_buf[n + 4U] = (uint8_t)size;
    g_buf[n + 5U] = (uint8_t)(size >> 8);
    g_buf[n + 6U] = 0;
    g_buf[n + 7U] = 0;
    n += 8U;
    g_buf[n] = (uint8_t)announced;
    g_buf[n + 1U] = (uint8_t)(announced >> 8);
    g_buf[n + 2U] = 0;
    g_buf[n + 3U] = 0;
    memcpy(&g_buf[n + 4U], name, name_len);
    n += 4U + name_len;
    memset(&g_buf[n], 0, 4U); /* lieu vide */
    memset(&g_buf[n + 4U], 0, 8U);
    g_buf[n + 4U] = 42U; /* date : 42 s */
    n += 12U;
    g_buf[8]++;               /* une section de plus (le nombre tient sur un octet ici) */
    uint32_t h = 2166136261U; /* FNV-1a de tout ce qui suit l'en-tête de 16 octets */
    for (size_t i = 16; i < n; i++) {
        h = (h ^ g_buf[i]) * 16777619U;
    }
    g_buf[12] = (uint8_t)h;
    g_buf[13] = (uint8_t)(h >> 8);
    g_buf[14] = (uint8_t)(h >> 16);
    g_buf[15] = (uint8_t)(h >> 24);
    return n;
}

UTEST(save_game, a_damaged_meta_section_is_refused_and_control_bytes_are_blanked)
{
    static const char long_name[] = "0123456789012345678901234567890123456789";
    EngSaveGame r;
    char err[128];
    /* des octets de contrôle dans le nom deviennent des espaces (pas de saut de ligne dans un
     * menu) */
    size_t n = forged_meta("a\n\tb\x7F", 5U, 5U);
    ASSERT_GT(n, 0U);
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_TRUE(r.has_meta);
    EXPECT_STREQ(r.name, "a  b ");
    EXPECT_EQ(r.saved_at, 42);
    EXPECT_FALSE(r.creative); /* section sans drapeaux (plus ancienne) : partie d'aventure */
    /* le nom le plus long accepté : 31 octets (le 32e est le zéro final) */
    n = forged_meta(long_name, (uint32_t)sizeof r.name - 1U, (uint32_t)sizeof r.name - 1U);
    ASSERT_TRUE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    EXPECT_EQ(strlen(r.name), sizeof r.name - 1U);
    /* un octet de plus, bien présent dans le fichier : le fichier ENTIER est refusé (rien n'est lu
     * à moitié, rien n'est écrit hors du nom) */
    n = forged_meta(long_name, (uint32_t)sizeof r.name, (uint32_t)sizeof r.name);
    ASSERT_GT(n, 0U);
    EXPECT_FALSE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
    /* une longueur annoncée qui dépasse la section : refusé aussi */
    n = forged_meta("abcd", 4U, 200U);
    EXPECT_FALSE(eng_save_decode(g_buf, n, &r, NULL, err, sizeof err));
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

static EngMenuAction menu_key(EngMenu *m, uint32_t k)
{
    EngInputState in = none_in();
    in.menu_pressed = k;
    in.menu_held = k;
    const EngMenuAction r = eng_menu_update(m, &in);
    EngInputState idle = none_in();
    (void)eng_menu_update(m, &idle); /* touche relâchée */
    return r;
}

static void use_slot(EngMenu *m, uint32_t index, const char *name, const char *summary)
{
    EngMenuSlot s;
    memset(&s, 0, sizeof s);
    s.used = true;
    (void)snprintf(s.name, sizeof s.name, "%s", name);
    (void)snprintf(s.summary, sizeof s.summary, "%s", summary);
    eng_menu_set_slot(m, index, &s);
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
    EngInputState idle = none_in();
    (void)eng_menu_update(&m, &idle);
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    ASSERT_EQ(eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS), 4U);
    EXPECT_STREQ(items[0].label, "Nouvelle partie"); /* aucune sauvegarde : pas de « Continuer » */
    EXPECT_STREQ(items[1].label, "Parties");
    EXPECT_STREQ(items[3].label, "Quitter");
    EXPECT_EQ(m.selected, 0);
    /* la barre du menu principal : gauche / droite (le stick comme les flèches) */
    (void)stick(&m, 1.0F, 0.0F);
    (void)stick(&m, 1.0F, 0.0F);
    EXPECT_EQ(m.selected, 2);
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
    EXPECT_EQ(m.selected, 2);     /* on revient sur « Options » */
    (void)stick(&m, 0.0F, -1.0F); /* bas : ligne suivante aussi */
    EXPECT_EQ(m.selected, 3);
    (void)stick(&m, 1.0F, 0.0F); /* la barre boucle */
    EXPECT_EQ(m.selected, 0);
    (void)stick(&m, -1.0F, 0.0F);
    EXPECT_EQ(m.selected, 3);
    EXPECT_EQ(press(&m, ENG_ACTION_JUMP), ENG_MENU_ACT_QUIT);
    /* en jeu : Pause ouvre ; revenir à la racine de la pause reprend la partie */
    m.screen = ENG_MENU_CLOSED;
    EXPECT_EQ(press(&m, ENG_ACTION_PAUSE), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_PAUSE);
    EXPECT_EQ(press(&m, ENG_ACTION_PAUSE), ENG_MENU_ACT_RESUME);
    EXPECT_FALSE(eng_menu_is_open(&m));
    (void)press(&m, ENG_ACTION_PAUSE);
    (void)stick(&m, 0.0F, -1.0F);
    EXPECT_EQ(press(&m, ENG_ACTION_ATTACK), ENG_MENU_ACT_SAVE);
    EXPECT_EQ(m.screen, ENG_MENU_PAUSE); /* sauvegarder ne ferme pas la pause */
    (void)stick(&m, 0.0F, -1.0F);
    (void)stick(&m, 0.0F, -1.0F);
    ASSERT_EQ(eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS), 4U);
    EXPECT_STREQ(items[3].label, "Sauvegarder et quitter");
    EXPECT_EQ(press(&m, ENG_ACTION_JUMP), ENG_MENU_ACT_TO_MAIN);
    EXPECT_EQ(m.screen, ENG_MENU_PAUSE); /* l'appelant enregistre, puis ouvre le menu principal */
    /* avec une partie : « Continuer » reprend la plus récente */
    eng_menu_open(&m, ENG_MENU_MAIN);
    use_slot(&m, 1, "Aube", "Niv. 3 · 12 min");
    m.has_save = true;
    m.last_slot = 1;
    ASSERT_EQ(eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS), 4U);
    EXPECT_STREQ(items[0].label, "Continuer");
    EXPECT_NE(strstr(items[0].value, "Aube"), NULL);
    EXPECT_NE(strstr(items[0].value, "Niv. 3"), NULL);
    EXPECT_EQ(press(&m, ENG_ACTION_JUMP), ENG_MENU_ACT_CONTINUE);
    EXPECT_EQ(m.slot, 1);
    EXPECT_FALSE(eng_menu_is_open(&m));
}

UTEST(game_menu, escape_opens_the_menu_goes_back_one_level_and_never_quits)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    EngMenu m;
    eng_menu_init(&m, &opt, NULL);
    /* en jeu : Échap ouvre la pause */
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_BACK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_PAUSE);
    /* pause -> options (flèches, Entrée) ; Échap revient d'UN niveau, sur la ligne d'où l'on vient
     */
    (void)menu_key(&m, ENG_MENU_KEY_DOWN);
    (void)menu_key(&m, ENG_MENU_KEY_DOWN);
    EXPECT_EQ(m.selected, 2);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_OPTIONS);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_LEFT), ENG_MENU_ACT_OPTIONS_CHANGED); /* flèche : régler */
    EXPECT_NEAR(opt.master, 0.7F, 1e-5F);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_BACK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_PAUSE);
    EXPECT_EQ(m.selected, 2);
    /* à la racine de la pause, Échap reprend la partie */
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_BACK), ENG_MENU_ACT_RESUME);
    EXPECT_FALSE(eng_menu_is_open(&m));
    /* à l'écran-titre, Échap ne ferme rien : seul « Quitter » quitte */
    eng_menu_open(&m, ENG_MENU_MAIN);
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_BACK), ENG_MENU_ACT_NONE);
        EXPECT_EQ(m.screen, ENG_MENU_MAIN);
    }
    (void)menu_key(&m, ENG_MENU_KEY_RIGHT);
    EXPECT_EQ(m.selected, 1);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_SAVES);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_BACK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_MAIN);
    EXPECT_EQ(m.selected, 1);
    /* une flèche tenue se répète : un pas tout de suite, puis un toutes les 200 ms (12 ticks) */
    eng_menu_open(&m, ENG_MENU_PAUSE);
    EngInputState held = none_in();
    held.menu_held = ENG_MENU_KEY_DOWN;
    held.menu_pressed = ENG_MENU_KEY_DOWN;
    (void)eng_menu_update(&m, &held);
    EXPECT_EQ(m.selected, 1);
    held.menu_pressed = 0;
    for (int i = 0; i < 11; i++) {
        (void)eng_menu_update(&m, &held);
    }
    EXPECT_EQ(m.selected, 1);
    (void)eng_menu_update(&m, &held);
    EXPECT_EQ(m.selected, 2);
}

UTEST(game_menu, three_slots_are_managed_like_a_list_of_worlds)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    EngMenu m;
    eng_menu_init(&m, &opt, NULL);
    eng_menu_open(&m, ENG_MENU_MAIN);
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    /* aucune partie : « Nouvelle partie » fait nommer la partie de l'emplacement 1 */
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_NAME);
    EXPECT_EQ(m.slot, 0);
    EXPECT_STREQ(m.text, "Partie 1");
    EXPECT_TRUE(m.text_fresh);
    EXPECT_EQ(m.selected, 1);                                      /* sur « Créer la partie » */
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_BACK), ENG_MENU_ACT_NONE); /* annuler : rien n'est créé */
    EXPECT_EQ(m.screen, ENG_MENU_SAVES);
    /* les trois emplacements côte à côte : gauche / droite sans boucler, « Retour » dessous */
    use_slot(&m, 0, "Alpha", "Niv. 3 · 12 min");
    m.has_save = true;
    m.last_slot = 0;
    eng_menu_open(&m, ENG_MENU_SAVES);
    ASSERT_EQ(eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS), 4U);
    EXPECT_STREQ(items[0].label, "Alpha");
    EXPECT_STREQ(items[0].value, "Niv. 3 · 12 min");
    EXPECT_STREQ(items[1].label, "Emplacement libre");
    EXPECT_STREQ(items[3].label, "Retour");
    (void)menu_key(&m, ENG_MENU_KEY_LEFT);
    EXPECT_EQ(m.selected, 0);
    (void)menu_key(&m, ENG_MENU_KEY_RIGHT);
    (void)menu_key(&m, ENG_MENU_KEY_RIGHT);
    (void)menu_key(&m, ENG_MENU_KEY_RIGHT);
    EXPECT_EQ(m.selected, 2);
    (void)menu_key(&m, ENG_MENU_KEY_DOWN);
    EXPECT_EQ(m.selected, 3);
    (void)menu_key(&m, ENG_MENU_KEY_RIGHT); /* sur « Retour », gauche / droite ne font rien */
    EXPECT_EQ(m.selected, 3);
    (void)menu_key(&m, ENG_MENU_KEY_UP); /* on remonte sur la carte d'où l'on venait */
    EXPECT_EQ(m.selected, 2);
    /* emplacement libre : créer une partie, avec un nom */
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_NAME);
    EXPECT_EQ(m.name_purpose, ENG_MENU_NAME_CREATE);
    EXPECT_STREQ(m.text, "Partie 3");
    /* le mode de jeu se choisit ici, sur la première ligne ; le curseur est sur « Créer la
     * partie » : Entrée crée une partie d'aventure sans passer par le mode */
    ASSERT_EQ(eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS), 3U);
    EXPECT_STREQ(items[0].label, "Mode de jeu");
    EXPECT_STREQ(items[0].value, "Aventure");
    EXPECT_EQ(items[0].kind, ENG_MENU_ITEM_CHOICE);
    EXPECT_STREQ(items[1].label, "Cr\xC3\xA9"
                                 "er la partie");
    EXPECT_STREQ(items[2].label, "Annuler");
    EXPECT_EQ(m.selected, 1);
    EXPECT_FALSE(m.creative);
    (void)menu_key(&m, ENG_MENU_KEY_RIGHT); /* ailleurs que sur le mode : sans effet */
    EXPECT_FALSE(m.creative);
    (void)menu_key(&m, ENG_MENU_KEY_UP);
    EXPECT_EQ(m.selected, 0);
    (void)menu_key(&m, ENG_MENU_KEY_RIGHT);
    EXPECT_TRUE(m.creative);
    (void)eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS);
    EXPECT_STREQ(items[0].value, "Cr\xC3\xA9"
                                 "atif");
    EXPECT_NEAR(items[0].frac, 1.0F, 0.0F);
    (void)menu_key(&m, ENG_MENU_KEY_LEFT);
    EXPECT_FALSE(m.creative);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK),
              ENG_MENU_ACT_NONE); /* Entrée sur le mode : il bascule */
    EXPECT_TRUE(m.creative);
    EXPECT_EQ(m.screen, ENG_MENU_NAME);
    (void)menu_key(&m, ENG_MENU_KEY_DOWN);
    EXPECT_EQ(m.selected, 1);
    eng_menu_text_input(&m, "Nuit");
    EXPECT_STREQ(m.text, "Nuit"); /* la première frappe remplace le nom proposé */
    EXPECT_FALSE(m.text_fresh);
    eng_menu_text_input(&m, " d'\xC3\xA9t\xC3\xA9"); /* « été » : deux octets par é */
    EXPECT_STREQ(m.text, "Nuit d'\xC3\xA9t\xC3\xA9");
    eng_menu_text_input(&m, "\b\b");
    EXPECT_STREQ(m.text, "Nuit d'\xC3\xA9");         /* effacer retire un caractère entier */
    eng_menu_text_input(&m, "\x01\t\n\x7F\xFF\xC3"); /* contrôles et octets invalides : ignorés */
    EXPECT_STREQ(m.text, "Nuit d'\xC3\xA9");
    const uint32_t denied = m.deny_serial;
    eng_menu_text_input(&m, "0123456789012345678901234567890");
    EXPECT_EQ(strlen(m.text), (size_t)ENG_MENU_NAME_CHARS + 1U); /* 22 caractères, dont un é */
    EXPECT_GT(m.deny_serial, denied);                            /* nom plein : signalé */
    eng_menu_text_input(&m, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
    EXPECT_STREQ(m.text, "");
    eng_menu_text_input(&m, "  ");
    EXPECT_STREQ(m.text, "");                                    /* pas d'espace en tête */
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_NONE); /* nom vide : refusé */
    EXPECT_EQ(m.screen, ENG_MENU_NAME);
    eng_menu_text_input(&m, "Brume  ");
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_NEW_GAME);
    EXPECT_STREQ(m.text, "Brume"); /* espaces de fin retirés */
    EXPECT_TRUE(m.creative);       /* le mode choisi accompagne l'action */
    EXPECT_EQ(m.slot, 2);
    EXPECT_FALSE(eng_menu_is_open(&m));
    /* emplacement occupé : Jouer, Renommer, Dupliquer, Supprimer, Retour */
    use_slot(&m, 2, "Brume", "Niv. 1 · 0 min");
    eng_menu_open(&m, ENG_MENU_SAVES);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_SLOT);
    EXPECT_EQ(m.slot, 0);
    ASSERT_EQ(eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS), 5U);
    EXPECT_STREQ(items[0].label, "Jouer");
    EXPECT_STREQ(items[2].label, "Dupliquer");
    EXPECT_TRUE(items[2].enabled);
    EXPECT_STREQ(items[2].value, "vers l'emplacement 2");
    /* renommer : le nom actuel est proposé ; Échap annule */
    (void)menu_key(&m, ENG_MENU_KEY_DOWN);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_NAME);
    EXPECT_EQ(m.name_purpose, ENG_MENU_NAME_RENAME);
    EXPECT_STREQ(m.text, "Alpha");
    /* renommer ne propose pas le mode : il est fixé depuis la création */
    ASSERT_EQ(eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS), 2U);
    EXPECT_STREQ(items[0].label, "Renommer");
    EXPECT_EQ(m.selected, 0);
    (void)menu_key(&m, ENG_MENU_KEY_RIGHT);
    EXPECT_TRUE(m.creative); /* (resté de la création précédente : aucune ligne ne le change ici) */
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_BACK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_SLOT);
    EXPECT_EQ(m.selected, 1);
    (void)menu_key(&m, ENG_MENU_KEY_OK);
    eng_menu_text_input(&m, "Aube");
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_RENAME_SLOT);
    EXPECT_STREQ(m.text, "Aube");
    EXPECT_EQ(m.slot, 0);
    EXPECT_EQ(m.screen, ENG_MENU_SLOT);
    /* dupliquer : dans le premier emplacement libre, sur lequel le curseur se pose */
    (void)menu_key(&m, ENG_MENU_KEY_DOWN);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_COPY_SLOT);
    EXPECT_EQ(m.slot, 0);
    EXPECT_EQ(m.slot_target, 1);
    EXPECT_EQ(m.screen, ENG_MENU_SAVES);
    EXPECT_EQ(m.selected, 1);
    use_slot(&m, 1, "Aube (copie)", "Niv. 3 · 12 min");
    /* plus d'emplacement libre : « Dupliquer » est grisé et le curseur le saute */
    (void)menu_key(&m, ENG_MENU_KEY_OK);
    ASSERT_EQ(eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS), 5U);
    EXPECT_FALSE(items[2].enabled);
    (void)menu_key(&m, ENG_MENU_KEY_DOWN);
    (void)menu_key(&m, ENG_MENU_KEY_DOWN);
    EXPECT_EQ(m.selected, 3); /* Supprimer */
    /* supprimer : confirmation, curseur sur « Non » ; seul « Oui » supprime */
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_DELETE);
    EXPECT_EQ(m.selected, 0);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_SLOT);
    EXPECT_EQ(m.selected, 3);
    (void)menu_key(&m, ENG_MENU_KEY_OK);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_BACK), ENG_MENU_ACT_NONE); /* Échap : rien n'est supprimé */
    EXPECT_EQ(m.screen, ENG_MENU_SLOT);
    (void)menu_key(&m, ENG_MENU_KEY_OK);
    (void)menu_key(&m, ENG_MENU_KEY_DOWN);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_DELETE_SLOT);
    EXPECT_EQ(m.slot, 1);
    EXPECT_EQ(m.screen, ENG_MENU_SAVES);
    EXPECT_EQ(m.selected, 1);
    eng_menu_set_slot(&m, 1, NULL);
    EXPECT_FALSE(m.slots[1].used);
    /* jouer : l'emplacement visé */
    (void)menu_key(&m, ENG_MENU_KEY_RIGHT);
    (void)menu_key(&m, ENG_MENU_KEY_OK);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_OK), ENG_MENU_ACT_CONTINUE);
    EXPECT_EQ(m.slot, 2);
    /* fichier illisible : ni jouable ni renommable, seulement supprimable */
    EngMenuSlot broken;
    memset(&broken, 0, sizeof broken);
    broken.used = true;
    broken.unreadable = true;
    eng_menu_set_slot(&m, 1, &broken);
    eng_menu_open(&m, ENG_MENU_SAVES);
    (void)menu_key(&m, ENG_MENU_KEY_RIGHT);
    (void)menu_key(&m, ENG_MENU_KEY_OK);
    ASSERT_EQ(eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS), 5U);
    EXPECT_FALSE(items[0].enabled);
    EXPECT_FALSE(items[1].enabled);
    EXPECT_FALSE(items[2].enabled);
    EXPECT_TRUE(items[3].enabled);
    EXPECT_EQ(m.selected, 3);
}

UTEST(game_menu, typing_a_name_ignores_game_keys_but_not_the_pad_or_the_mouse)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    static EngInput input;
    eng_input_init(&input);
    eng_input_set_default_bindings(&input);
    EngMenu m;
    eng_menu_init(&m, &opt, &input);
    eng_menu_open(&m, ENG_MENU_SAVES);
    (void)menu_key(&m, ENG_MENU_KEY_OK);
    ASSERT_EQ(m.screen, ENG_MENU_NAME);
    /* au clavier : Espace (Saut), Ctrl (Esquive), P (Pause) et ZQSD sont du texte */
    eng_input_key(&input, 44, true); /* espace */
    EXPECT_EQ(press(&m, ENG_ACTION_JUMP), ENG_MENU_ACT_NONE);
    EXPECT_EQ(press(&m, ENG_ACTION_DODGE), ENG_MENU_ACT_NONE);
    EXPECT_EQ(press(&m, ENG_ACTION_PAUSE), ENG_MENU_ACT_NONE);
    EXPECT_EQ(press(&m, ENG_ACTION_ATTACK), ENG_MENU_ACT_NONE);
    (void)stick(&m, 0.0F, -1.0F);
    (void)stick(&m, 1.0F, 0.0F); /* ni le stick ni D ne changent le mode pendant la saisie */
    EXPECT_EQ(m.screen, ENG_MENU_NAME);
    EXPECT_EQ(m.selected, 1);
    EXPECT_FALSE(m.creative);
    /* les flèches choisissent entre le mode, « Créer » et « Annuler » */
    (void)menu_key(&m, ENG_MENU_KEY_DOWN);
    EXPECT_EQ(m.selected, 2);
    (void)menu_key(&m, ENG_MENU_KEY_UP);
    EXPECT_EQ(m.selected, 1);
    /* à la souris : un clic sur la ligne du mode le bascule */
    eng_input_mouse_button(&input, 1, true);
    eng_menu_hover(&m, 0, 0, true);
    EXPECT_EQ(press(&m, ENG_ACTION_ATTACK), ENG_MENU_ACT_NONE);
    EXPECT_TRUE(m.creative);
    EXPECT_EQ(m.screen, ENG_MENU_NAME);
    eng_menu_hover(&m, 1, 0, true);
    /* à la manette : B revient, A valide */
    eng_input_pad_button(&input, 1, true);
    EXPECT_EQ(press(&m, ENG_ACTION_DODGE), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_SAVES);
    (void)menu_key(&m, ENG_MENU_KEY_OK);
    EXPECT_FALSE(m.creative); /* une nouvelle création repart en aventure */
    eng_input_pad_button(&input, 0, true);
    EXPECT_EQ(press(&m, ENG_ACTION_JUMP), ENG_MENU_ACT_NEW_GAME);
    EXPECT_STREQ(m.text, "Partie 1"); /* le nom proposé, sans clavier */
    EXPECT_FALSE(m.creative);
}

UTEST(game_menu, a_mouse_click_only_validates_the_line_under_the_pointer)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    static EngInput input;
    eng_input_init(&input);
    eng_input_set_default_bindings(&input);
    EngMenu m;
    eng_menu_init(&m, &opt, &input);
    eng_menu_open(&m, ENG_MENU_MAIN);
    eng_input_mouse_button(&input, 1, true); /* la dernière pression est un clic */
    /* hors de toute ligne : le clic ne valide rien (avant : il validait la ligne choisie) */
    eng_menu_hover(&m, -1, 0, true);
    EXPECT_EQ(press(&m, ENG_ACTION_ATTACK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_MAIN);
    /* la souris immobile ne change pas la ligne choisie ; en bougeant, elle la choisit */
    eng_menu_hover(&m, 2, 0, false);
    EXPECT_EQ(m.selected, 0);
    eng_menu_hover(&m, 2, 0, true);
    EXPECT_EQ(m.selected, 2);
    EXPECT_EQ(press(&m, ENG_ACTION_ATTACK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.screen, ENG_MENU_OPTIONS);
    /* options : un clic à gauche de la valeur la baisse, à droite il la monte */
    eng_menu_hover(&m, 0, -1, true);
    EXPECT_EQ(press(&m, ENG_ACTION_ATTACK), ENG_MENU_ACT_OPTIONS_CHANGED);
    EXPECT_NEAR(opt.master, 0.7F, 1e-5F);
    eng_menu_hover(&m, 0, 1, true);
    EXPECT_EQ(press(&m, ENG_ACTION_ATTACK), ENG_MENU_ACT_OPTIONS_CHANGED);
    EXPECT_NEAR(opt.master, 0.8F, 1e-5F);
    /* à la manette (X = Attaque), pas besoin de pointeur */
    eng_menu_hover(&m, -1, 0, false);
    eng_input_pad_button(&input, 2, true);
    m.selected = (int32_t)ENG_BUS_COUNT + 2; /* inverser l'axe vertical */
    EXPECT_EQ(press(&m, ENG_ACTION_ATTACK), ENG_MENU_ACT_OPTIONS_CHANGED);
    EXPECT_TRUE(opt.invert_y);
}

UTEST(game_menu, option_rows_describe_their_widget_and_bindings_have_readable_names)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    opt.master = 0.3F;
    opt.shadow_quality = 1U;
    static EngInput input;
    eng_input_init(&input);
    eng_input_set_default_bindings(&input);
    EngMenu m;
    eng_menu_init(&m, &opt, &input);
    eng_menu_open(&m, ENG_MENU_OPTIONS);
    EngMenuItem items[ENG_MENU_MAX_ITEMS];
    const uint32_t n = eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS);
    ASSERT_EQ(n, (uint32_t)ENG_BUS_COUNT + 15U);
    EXPECT_EQ(items[0].kind, ENG_MENU_ITEM_SLIDER);
    EXPECT_NEAR(items[0].frac, 0.3F, 1e-6F);
    EXPECT_EQ(items[ENG_BUS_COUNT + 2].kind, ENG_MENU_ITEM_TOGGLE);
    EXPECT_NEAR(items[ENG_BUS_COUNT + 2].frac, 0.0F, 0.0F);
    EXPECT_EQ(items[ENG_BUS_COUNT + 5].kind, ENG_MENU_ITEM_TOGGLE); /* FXAA : oui */
    EXPECT_NEAR(items[ENG_BUS_COUNT + 5].frac, 1.0F, 0.0F);
    EXPECT_EQ(items[ENG_BUS_COUNT + 9].kind, ENG_MENU_ITEM_CHOICE);
    EXPECT_NEAR(items[ENG_BUS_COUNT + 9].frac, 0.5F, 1e-6F);
    EXPECT_EQ(items[n - 1U].kind, ENG_MENU_ITEM_PLAIN);
    /* les touches portent un nom, pas un numéro */
    char name[32];
    eng_binding_name(ENG_BIND_KEY, ENG_KEY_ESCAPE, name, sizeof name);
    EXPECT_STREQ(name, "\xC3\x89"
                       "chap");
    eng_binding_name(ENG_BIND_KEY, 44, name, sizeof name);
    EXPECT_STREQ(name, "Espace");
    eng_binding_name(ENG_BIND_MOUSE_BUTTON, 1, name, sizeof name);
    EXPECT_STREQ(name, "clic gauche");
    eng_binding_name(ENG_BIND_MOUSE_BUTTON, 3, name, sizeof name);
    EXPECT_STREQ(name, "clic droit");
    eng_binding_name(ENG_BIND_PAD_BUTTON, 0, name, sizeof name);
    EXPECT_STREQ(name, "manette A");
    eng_binding_name(ENG_BIND_PAD_BUTTON, 6, name, sizeof name);
    EXPECT_STREQ(name, "Start");
    eng_binding_name(ENG_BIND_PAD_TRIGGER, ENG_PAD_AXIS_RIGHT_TRIGGER, name, sizeof name);
    EXPECT_STREQ(name, "g\xC3\xA2"
                       "chette droite");
    eng_menu_open(&m, ENG_MENU_CONTROLS);
    (void)eng_menu_items(&m, items, ENG_MENU_MAX_ITEMS);
    EXPECT_STREQ(items[0].value, "clic gauche, manette X"); /* attaque légère */
    EXPECT_STREQ(items[3].value, "Espace, manette A");      /* saut */
    EXPECT_EQ(strstr(items[2].value, "touche "), NULL);     /* plus de « touche 224 » */
    /* remappage : Échap annule (et ne devient pas la touche de l'action) */
    m.selected = 3;
    (void)menu_key(&m, ENG_MENU_KEY_OK);
    EXPECT_EQ(m.remap, 3);
    eng_input_key(&input, ENG_KEY_ESCAPE, true);
    EXPECT_EQ(menu_key(&m, ENG_MENU_KEY_BACK), ENG_MENU_ACT_NONE);
    EXPECT_EQ(m.remap, -1);
    EXPECT_EQ(m.screen, ENG_MENU_CONTROLS);
    EXPECT_EQ(input.buttons[ENG_ACTION_JUMP][0].code, 44);
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
