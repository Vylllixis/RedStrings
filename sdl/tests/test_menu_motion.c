/* Animation des menus (engine/menu_motion.h) : courbes, ressorts, cascade, et l'état animé d'un
 * menu piloté comme dans le jeu. Tout est pur : ni fenêtre ni GPU. */
#include <math.h>
#include <string.h>

#include "engine/menu_motion.h"
#include "utest.h"

#define DT (1.0F / 60.0F)

UTEST(menu_motion, easing_curves_start_at_0_end_at_1_and_out_back_overshoots)
{
    float (*const curves[3])(float) = {eng_ease_out_cubic, eng_ease_in_out_cubic,
                                       eng_ease_out_back};
    for (int c = 0; c < 3; c++) {
        EXPECT_NEAR(curves[c](0.0F), 0.0F, 1e-5F);
        EXPECT_NEAR(curves[c](1.0F), 1.0F, 1e-5F);
        EXPECT_NEAR(curves[c](-3.0F), 0.0F, 1e-5F); /* hors de [0, 1] : ramené aux bornes */
        EXPECT_NEAR(curves[c](7.0F), 1.0F, 1e-5F);
    }
    float previous = 0.0F;
    float peak = 0.0F;
    for (int i = 1; i <= 100; i++) {
        const float t = (float)i / 100.0F;
        EXPECT_GE(eng_ease_out_cubic(t), previous); /* croissante */
        previous = eng_ease_out_cubic(t);
        peak = fmaxf(peak, eng_ease_out_back(t));
    }
    EXPECT_GT(eng_ease_out_cubic(0.5F), 0.8F); /* rapide au début, douce à la fin */
    EXPECT_LT(eng_ease_in_out_cubic(0.25F), 0.1F);
    EXPECT_NEAR(eng_ease_in_out_cubic(0.5F), 0.5F, 1e-5F);
    EXPECT_GT(peak, 1.05F); /* « se pose » : dépasse puis revient */
    EXPECT_LT(peak, 1.2F);
}

UTEST(menu_motion, a_spring_reaches_its_target_and_stays_stable_with_long_frames)
{
    EngSpring critical = {0.0F, 0.0F};
    float highest = 0.0F;
    for (int i = 0; i < 120; i++) {
        eng_spring_step(&critical, 1.0F, 3.0F, 1.0F, DT);
        highest = fmaxf(highest, critical.x);
    }
    EXPECT_NEAR(critical.x, 1.0F, 1e-3F);
    EXPECT_LT(highest, 1.001F); /* amortissement critique : pas de dépassement */
    EngSpring bouncy = {0.0F, 0.0F};
    highest = 0.0F;
    for (int i = 0; i < 240; i++) {
        eng_spring_step(&bouncy, 1.0F, 3.0F, 0.3F, DT);
        highest = fmaxf(highest, bouncy.x);
    }
    EXPECT_GT(highest, 1.2F); /* sous-amorti : il dépasse, puis se pose */
    EXPECT_NEAR(bouncy.x, 1.0F, 5e-3F);
    /* une image d'une demi-seconde (à-coup) ne fait pas diverger un ressort raide */
    EngSpring stiff = {0.0F, 0.0F};
    for (int i = 0; i < 8; i++) {
        eng_spring_step(&stiff, 1.0F, 8.0F, 0.5F, 0.5F);
        EXPECT_TRUE(isfinite(stiff.x));
        EXPECT_LT(fabsf(stiff.x), 3.0F);
    }
    EXPECT_NEAR(stiff.x, 1.0F, 0.05F);
    /* entrées absurdes : sans effet */
    EngSpring same = {0.25F, 0.5F};
    eng_spring_step(&same, 1.0F, 3.0F, 1.0F, 0.0F);
    eng_spring_step(&same, 1.0F, 0.0F, 1.0F, DT);
    eng_spring_step(&same, NAN, 3.0F, 1.0F, DT);
    eng_spring_step(NULL, 1.0F, 3.0F, 1.0F, DT);
    EXPECT_NEAR(same.x, 0.25F, 0.0F);
    EXPECT_NEAR(same.v, 0.5F, 0.0F);
}

UTEST(menu_motion, a_stagger_delays_each_item_after_the_previous_one)
{
    EXPECT_NEAR(eng_stagger(0.0F, 0, 0.1F, 0.4F), 0.0F, 1e-6F);
    EXPECT_NEAR(eng_stagger(0.2F, 0, 0.1F, 0.4F), 0.5F, 1e-6F);
    EXPECT_NEAR(eng_stagger(0.2F, 2, 0.1F, 0.4F), 0.0F, 1e-6F); /* le 3e commence à 0,2 s */
    EXPECT_NEAR(eng_stagger(0.4F, 2, 0.1F, 0.4F), 0.5F, 1e-6F);
    EXPECT_NEAR(eng_stagger(9.0F, 5, 0.1F, 0.4F), 1.0F, 1e-6F);
    EXPECT_NEAR(eng_stagger(0.3F, 3, 0.1F, 0.0F), 1.0F, 1e-6F); /* durée nulle : tout ou rien */
    EXPECT_NEAR(eng_stagger(0.29F, 3, 0.1F, 0.0F), 0.0F, 1e-6F);
}

static void run(EngMenuMotion *a, const EngMenu *m, float seconds, float aspect)
{
    const int frames = (int)lroundf(seconds / DT);
    for (int i = 0; i < frames; i++) {
        eng_menu_motion_update(a, m, DT, aspect);
    }
}

static void use_slot(EngMenu *m, uint32_t index, const char *name)
{
    EngMenuSlot s;
    memset(&s, 0, sizeof s);
    s.used = true;
    (void)snprintf(s.name, sizeof s.name, "%s", name);
    eng_menu_set_slot(m, index, &s);
}

UTEST(menu_motion, screens_enter_leave_and_the_cursor_follows_the_selection)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    EngMenu m;
    eng_menu_init(&m, &opt, NULL);
    eng_menu_open(&m, ENG_MENU_MAIN);
    EngMenuMotion a;
    eng_menu_motion_init(&a);
    EXPECT_EQ(eng_menu_family(ENG_MENU_MAIN), ENG_MENU_FAMILY_RAIL);
    EXPECT_EQ(eng_menu_family(ENG_MENU_SAVES), ENG_MENU_FAMILY_CARDS);
    EXPECT_EQ(eng_menu_family(ENG_MENU_NAME), ENG_MENU_FAMILY_CARDS);
    EXPECT_EQ(eng_menu_family(ENG_MENU_OPTIONS), ENG_MENU_FAMILY_LIST);
    EXPECT_EQ(eng_menu_family(ENG_MENU_CLOSED), ENG_MENU_FAMILY_NONE);
    /* première image : l'écran entre (rien n'est encore mis en avant) */
    eng_menu_motion_update(&a, &m, DT, 16.0F / 9.0F);
    EXPECT_EQ(a.screen, ENG_MENU_MAIN);
    EXPECT_NEAR(a.enter, DT, 1e-6F);
    EXPECT_LT(a.emphasis[0], 0.3F);
    EXPECT_LT(a.open, 0.3F);
    run(&a, &m, 1.0F, 16.0F / 9.0F);
    EXPECT_GT(a.emphasis[0], 0.99F); /* la ligne choisie est mise en avant, les autres non */
    EXPECT_LT(a.emphasis[1], 0.01F);
    EXPECT_GT(a.open, 0.99F);
    EXPECT_NEAR(a.cursor.x, 0.0F, 1e-3F);
    /* changer de ligne : le fil est pincé, le curseur rejoint la ligne en dépassant un peu */
    m.selected = 2;
    eng_menu_motion_update(&a, &m, DT, 16.0F / 9.0F);
    EXPECT_NEAR(a.pluck, 0.0F, 1e-6F);
    EXPECT_NEAR(a.since_select, 0.0F, 1e-6F);
    eng_menu_motion_update(&a, &m, DT, 16.0F / 9.0F);
    EXPECT_NEAR(a.pluck, DT, 1e-6F); /* puis le temps repart */
    float furthest = 0.0F;
    for (int i = 0; i < 90; i++) {
        eng_menu_motion_update(&a, &m, DT, 16.0F / 9.0F);
        furthest = fmaxf(furthest, a.cursor.x);
    }
    EXPECT_NEAR(a.cursor.x, 2.0F, 0.01F);
    EXPECT_GT(furthest, 2.02F);
    EXPECT_GT(a.emphasis[2], 0.99F);
    EXPECT_LT(a.emphasis[0], 0.01F);
    /* valider, refuser, régler : des impulsions qui s'éteignent */
    m.confirm_serial++;
    m.deny_serial++;
    m.adjust_serial++;
    m.adjust_dir = -1;
    eng_menu_motion_update(&a, &m, DT, 16.0F / 9.0F);
    EXPECT_GT(a.confirm, 0.8F);
    EXPECT_GT(a.deny, 0.8F);
    EXPECT_GT(a.adjust, 0.8F);
    EXPECT_EQ(a.adjust_dir, -1);
    run(&a, &m, 2.0F, 16.0F / 9.0F);
    EXPECT_NEAR(a.confirm, 0.0F, 0.0F);
    EXPECT_NEAR(a.deny, 0.0F, 0.0F);
    EXPECT_NEAR(a.adjust, 0.0F, 0.0F);
    /* changer d'écran : l'ancien sort (from, leave), le nouveau entre (enter repart de 0) */
    eng_menu_open(&m, ENG_MENU_OPTIONS);
    eng_menu_motion_update(&a, &m, DT, 16.0F / 9.0F);
    EXPECT_EQ(a.screen, ENG_MENU_OPTIONS);
    EXPECT_EQ(a.from, ENG_MENU_MAIN);
    EXPECT_EQ(a.from_selected, 2);
    EXPECT_NEAR(a.leave, 0.0F, 1e-6F);
    EXPECT_NEAR(a.enter, 0.0F, 1e-6F);
    EXPECT_NEAR(a.cursor.x, 0.0F, 1e-6F); /* le curseur ne traverse pas l'écran : il est posé */
    run(&a, &m, ENG_MOTION_OUT_TIME + 0.05F, 16.0F / 9.0F);
    EXPECT_GT(a.leave, ENG_MOTION_OUT_TIME);
    /* le message : il apparaît, et reste lisible pendant qu'il s'efface */
    eng_menu_set_status(&m, "Partie sauvegard\xC3\xA9"
                            "e");
    run(&a, &m, 0.6F, 16.0F / 9.0F);
    EXPECT_GT(a.status, 0.95F);
    m.status[0] = '\0';
    eng_menu_motion_update(&a, &m, DT, 16.0F / 9.0F);
    EXPECT_LT(a.status, 0.95F);
    EXPECT_STREQ(a.status_text, "Partie sauvegard\xC3\xA9"
                                "e");
    /* fermer le menu : il s'efface */
    m.screen = ENG_MENU_CLOSED;
    run(&a, &m, 1.0F, 16.0F / 9.0F);
    EXPECT_LT(a.open, 0.01F);
    EXPECT_EQ(a.from, ENG_MENU_OPTIONS);
}

UTEST(menu_motion, a_long_frame_advances_the_animation_by_50_ms_at_most)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    EngMenu m;
    eng_menu_init(&m, &opt, NULL);
    eng_menu_open(&m, ENG_MENU_MAIN);
    EngMenuMotion a;
    eng_menu_motion_init(&a);
    eng_menu_motion_update(&a, &m, 2.0F, 1.6F); /* première image très longue (chargement) */
    EXPECT_NEAR(a.enter, 0.05F, 1e-6F);
    EXPECT_NEAR(a.clock, 0.05F, 1e-6F);
    eng_menu_motion_update(&a, &m, -1.0F, 1.6F); /* dt absurde : le temps ne recule pas */
    eng_menu_motion_update(&a, &m, NAN, 1.6F);
    EXPECT_NEAR(a.clock, 0.05F, 1e-6F);
    eng_menu_motion_update(&a, &m, DT, 0.0F); /* écran sans largeur : ignoré */
    EXPECT_NEAR(a.clock, 0.05F, 1e-6F);
}

UTEST(menu_motion, cards_hang_in_a_row_then_the_chosen_one_comes_forward)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    EngMenu m;
    eng_menu_init(&m, &opt, NULL);
    use_slot(&m, 0, "Alpha");
    eng_menu_open(&m, ENG_MENU_MAIN);
    EngMenuMotion a;
    eng_menu_motion_init(&a);
    const float aspect = 16.0F / 9.0F;
    run(&a, &m, 1.0F, aspect);
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) { /* menu principal : pas de cartes */
        EXPECT_LT(a.cards[i].alpha.x, 0.01F);
    }
    EXPECT_NEAR(a.thread, 0.0F, 0.0F);
    eng_menu_open(&m, ENG_MENU_SAVES);
    m.selected = 1;
    eng_menu_motion_update(&a, &m, DT, aspect);
    EXPECT_LT(a.thread, 0.1F); /* le fil se tend en une demi-seconde */
    run(&a, &m, 3.0F, aspect);
    EXPECT_NEAR(a.thread, 1.0F, 0.0F);
    EXPECT_NEAR(a.fit, 1.0F, 0.0F);
    /* trois cartes côte à côte, dans l'écran, sous le fil ; la carte choisie est plus grande */
    EXPECT_LT(a.cards[0].x.x, a.cards[1].x.x - 0.3F);
    EXPECT_LT(a.cards[1].x.x, a.cards[2].x.x - 0.3F);
    EXPECT_NEAR(a.cards[1].x.x, 0.0F, 1e-3F);
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        EXPECT_LT(fabsf(a.cards[i].x.x) + (ENG_MOTION_CARD_W * 0.53F), aspect * 0.5F);
        EXPECT_GT(a.cards[i].y.x - (ENG_MOTION_CARD_H * 0.53F), ENG_MOTION_THREAD_Y);
        EXPECT_GT(a.cards[i].alpha.x, 0.8F);
        EXPECT_NEAR(a.cards[i].angle.x, 0.0F, 0.01F); /* le balancement s'est éteint */
    }
    EXPECT_GT(a.cards[1].scale.x, a.cards[0].scale.x + 0.04F);
    EXPECT_GT(a.cards[1].alpha.x, a.cards[0].alpha.x + 0.05F);
    /* changer de carte : la nouvelle se balance */
    m.selected = 0;
    eng_menu_motion_update(&a, &m, DT, aspect);
    eng_menu_motion_update(&a, &m, DT, aspect);
    EXPECT_GT(fabsf(a.cards[0].angle.x), 0.005F);
    /* ouvrir la carte 1 : elle vient à gauche, agrandie ; les autres sortent de l'écran */
    m.slot = 0;
    eng_menu_open(&m, ENG_MENU_SLOT);
    run(&a, &m, 3.0F, aspect);
    EXPECT_LT(a.cards[0].x.x, -0.2F);
    EXPECT_GT(a.cards[0].scale.x, 1.1F);
    EXPECT_GT(a.cards[0].alpha.x, 0.99F);
    EXPECT_LT(a.cards[1].alpha.x, 0.01F);
    EXPECT_LT(a.cards[2].alpha.x, 0.01F);
    EXPECT_GT(a.cards[2].x.x, aspect * 0.5F);
    /* fenêtre étroite (4:3 puis portrait) : les cartes rétrécissent pour tenir */
    eng_menu_open(&m, ENG_MENU_SAVES);
    run(&a, &m, 3.0F, 4.0F / 3.0F);
    EXPECT_NEAR(a.fit, 1.0F, 0.0F);
    run(&a, &m, 3.0F, 0.75F);
    EXPECT_LT(a.fit, 0.7F);
    for (uint32_t i = 0; i < ENG_MENU_SLOTS; i++) {
        EXPECT_LT(fabsf(a.cards[i].x.x) + (ENG_MOTION_CARD_W * a.fit * 0.53F), 0.75F * 0.5F);
    }
}

UTEST(menu_motion, a_deleted_game_falls_and_a_new_one_drops_in)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    EngMenu m;
    eng_menu_init(&m, &opt, NULL);
    use_slot(&m, 0, "Alpha");
    use_slot(&m, 1, "B\xC3\xAAta");
    eng_menu_open(&m, ENG_MENU_SAVES);
    EngMenuMotion a;
    eng_menu_motion_init(&a);
    const float aspect = 16.0F / 9.0F;
    run(&a, &m, 3.0F, aspect);
    EXPECT_LT(a.ghost_time, 0.0F); /* rien ne tombe */
    const float rest_y = a.cards[1].y.x;
    /* suppression : la carte de la partie tombe (son contenu est gardé le temps de la chute), une
     * carte vide redescend à sa place */
    eng_menu_set_slot(&m, 1, NULL);
    eng_menu_motion_update(&a, &m, DT, aspect);
    EXPECT_GE(a.ghost_time, 0.0F);
    EXPECT_EQ(a.ghost_index, 1);
    EXPECT_STREQ(a.ghost.name, "B\xC3\xAAta");
    EXPECT_NEAR(a.ghost_y, rest_y, 1e-4F);
    EXPECT_LT(a.cards[1].y.x, -0.5F); /* la carte vide part du haut de l'écran */
    EXPECT_LT(a.cards[1].born, 0.1F);
    run(&a, &m, ENG_MOTION_GHOST_TIME + 0.1F, aspect);
    EXPECT_LT(a.ghost_time, 0.0F);
    run(&a, &m, 3.0F, aspect);
    EXPECT_NEAR(a.cards[1].y.x, rest_y, 2e-3F);
    /* une partie créée (ou copiée) : sa carte descend aussi, sans fantôme */
    use_slot(&m, 2, "Gamma");
    eng_menu_motion_update(&a, &m, DT, aspect);
    EXPECT_LT(a.cards[2].y.x, -0.5F);
    EXPECT_LT(a.ghost_time, 0.0F);
}

UTEST(menu_motion, a_long_list_scrolls_to_keep_the_selection_visible)
{
    /* 19 lignes, 10 visibles : la ligne choisie garde une ligne de marge quand c'est possible */
    EXPECT_NEAR(eng_menu_motion_scroll_target(0.0F, 0, 19, 10), 0.0F, 0.0F);
    EXPECT_NEAR(eng_menu_motion_scroll_target(0.0F, 8, 19, 10), 0.0F, 0.0F);
    EXPECT_NEAR(eng_menu_motion_scroll_target(0.0F, 9, 19, 10), 1.0F, 0.0F);
    EXPECT_NEAR(eng_menu_motion_scroll_target(0.0F, 18, 19, 10), 9.0F,
                0.0F); /* la fin, sans vide */
    EXPECT_NEAR(eng_menu_motion_scroll_target(9.0F, 12, 19, 10), 9.0F, 0.0F); /* déjà visible */
    EXPECT_NEAR(eng_menu_motion_scroll_target(9.0F, 9, 19, 10), 8.0F, 0.0F);
    EXPECT_NEAR(eng_menu_motion_scroll_target(9.0F, 0, 19, 10), 0.0F, 0.0F);
    EXPECT_NEAR(eng_menu_motion_scroll_target(3.0F, 2, 5, 10), 0.0F, 0.0F); /* tout tient */
    EXPECT_NEAR(eng_menu_motion_scroll_target(3.0F, 2, 5, 0), 0.0F, 0.0F);
    /* dans le menu : le dessin annonce 10 lignes visibles, la liste suit le curseur */
    EngGameOptions opt;
    eng_options_default(&opt);
    EngMenu m;
    eng_menu_init(&m, &opt, NULL);
    eng_menu_open(&m, ENG_MENU_OPTIONS);
    EngMenuMotion a;
    eng_menu_motion_init(&a);
    eng_menu_motion_update(&a, &m, DT, 1.6F);
    a.list_count = 19;
    a.list_visible = 10;
    m.selected = 18;
    eng_menu_motion_update(&a, &m, DT, 1.6F);
    EXPECT_NEAR(a.scroll.x, 9.0F, 1e-4F); /* écran tout juste ouvert : placée sans défiler */
    m.selected = 0;
    eng_menu_motion_update(&a, &m, DT, 1.6F);
    EXPECT_GT(a.scroll.x, 7.0F); /* ensuite elle défile en douceur */
    EXPECT_LT(a.scroll.x, 9.0F);
    run(&a, &m, 2.0F, 1.6F);
    EXPECT_NEAR(a.scroll.x, 0.0F, 0.01F);
}

UTEST(menu_motion, the_title_scene_reframes_and_dims_behind_cards_and_lists)
{
    EngGameOptions opt;
    eng_options_default(&opt);
    EngMenu m;
    eng_menu_init(&m, &opt, NULL);
    eng_menu_open(&m, ENG_MENU_MAIN);
    EngMenuMotion a;
    eng_menu_motion_init(&a);
    const float aspect = 16.0F / 9.0F;
    /* à la sortie de l'écran de chargement : le titre est à sa place de chargement, sans menu */
    eng_menu_motion_update(&a, &m, DT, aspect);
    EXPECT_LT(a.title_menu, 0.2F);
    EXPECT_NEAR(a.title_y.x, 0.255F, 0.01F);
    EXPECT_NEAR(a.title_w.x, 0.56F, 0.01F);
    run(&a, &m, 2.0F, aspect);
    EXPECT_GT(a.title_menu, 0.99F);
    EXPECT_NEAR(a.dim.x, 0.0F, 1e-3F);
    EXPECT_NEAR(a.cam_zoom.x, 1.0F, 1e-3F);
    /* parties : le titre monte et rétrécit, la scène se rapproche et s'assombrit */
    const float title_y = a.title_y.x;
    const float title_w = a.title_w.x;
    eng_menu_open(&m, ENG_MENU_SAVES);
    run(&a, &m, 4.0F, aspect);
    EXPECT_GT(a.title_y.x, title_y + 0.08F);
    EXPECT_LT(a.title_w.x, title_w - 0.2F);
    EXPECT_GT(a.dim.x, 0.7F);
    EXPECT_GT(a.cam_zoom.x, 1.03F);
    /* le haut du titre reste dans l'écran (demi-hauteur ~ demi-largeur / 7) */
    EXPECT_LT(a.title_y.x + (a.title_w.x / 7.0F), 0.5F);
    const float dim_saves = a.dim.x;
    m.slot = 0;
    eng_menu_open(&m, ENG_MENU_SLOT);
    run(&a, &m, 4.0F, aspect);
    EXPECT_GT(a.dim.x, dim_saves + 0.05F); /* plus sombre encore derrière le panneau */
    EXPECT_LT(a.cam_x.x, -0.01F);
    eng_menu_open(&m, ENG_MENU_MAIN);
    run(&a, &m, 4.0F, aspect);
    EXPECT_NEAR(a.dim.x, 0.0F, 2e-3F);
    EXPECT_NEAR(a.title_y.x, title_y, 2e-3F);
    /* retour au titre depuis le jeu : le titre est déjà à sa place de menu (pas de chargement) */
    eng_menu_motion_init(&a);
    a.title_menu = 1.0F;
    eng_menu_open(&m, ENG_MENU_SAVES);
    eng_menu_motion_update(&a, &m, DT, aspect);
    EXPECT_NEAR(a.title_menu, 1.0F, 1e-4F);
    EXPECT_GT(a.dim.x, 0.7F);
    /* état de repos sans animation (captures, tests) */
    EngMenuMotion still;
    eng_menu_motion_init(&still);
    eng_menu_open(&m, ENG_MENU_SLOT);
    eng_menu_motion_settle(&still, &m, aspect);
    EXPECT_GT(still.enter, 10.0F);
    EXPECT_NEAR(still.thread, 1.0F, 0.0F);
    EXPECT_GT(still.cards[0].scale.x, 1.1F);
    EXPECT_NEAR(still.emphasis[0], 1.0F, 0.0F);
    EXPECT_NEAR(still.open, 1.0F, 0.0F);
}
