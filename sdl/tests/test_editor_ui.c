#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#include "engine/editor.h"
#include "engine/ui.h"
#include "engine/world.h"
#include "test_render_util.h"
#include "utest.h"

/* Fenêtre d'éditeur (M2-10a) : Dear ImGui pour de vrai, piloté par des événements de souris
 * synthétiques, comme test_ui.c. On clique sur la hiérarchie et les boutons et on regarde le monde
 * changer. Sans GPU ces tests s'arrêtent là (OW_REQUIRE_GPU=1 : échec). */

#ifndef OW_UI

UTEST(eng_editor_ui_disabled, the_editor_window_is_a_harmless_no_op)
{
    EngUi ui;
    EngWorld world;
    EngEditor editor;
    EngUiEditorResult result = {0};
    float r[4];
    memset(&ui, 0, sizeof ui);
    eng_editor_init(&editor);
    eng_ui_draw_editor(&ui, &world, &editor, &result);
    EXPECT_FALSE(result.save_requested);
    EXPECT_FALSE(eng_ui_editor_row_rect(&ui, ENG_ENTITY_NULL, r));
    EXPECT_FALSE(eng_ui_editor_field_rect(&ui, 1, 0, r));
}

#else

#define RGBA8 ((uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM)
#define SIZE 600U

typedef struct EdTest {
    RenderFixture f;
    EngUi ui;
    EngWorld world;
    EngEditor editor;
    EngEntity a;
    EngEntity b;
    EngEntity c;
    uint32_t saves;
} EdTest;

static EngEntity make(EngWorld *w, const char *name, EngEntity parent)
{
    const EngEntity e = eng_world_create(w, name);
    (void)eng_world_add(w, e, ENG_COMPONENT_TRANSFORM);
    if (parent.generation != 0) {
        (void)eng_world_set_parent(w, e, parent);
    }
    return e;
}

static bool open_ed(EdTest *t)
{
    memset(t, 0, sizeof *t);
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_ui_init(&t->ui, NULL, t->f.gpu.device, RGBA8)) {
        close_render_fixture(&t->f);
        return false;
    }
    eng_ui_set_visible(&t->ui, true);
    (void)eng_world_init(&t->world, 256);
    eng_editor_init(&t->editor);
    t->a = make(&t->world, "alpha", ENG_ENTITY_NULL);
    t->b = make(&t->world, "beta", t->a);
    t->c = make(&t->world, "gamma", ENG_ENTITY_NULL);
    return true;
}

static void close_ed(EdTest *t)
{
    eng_world_shutdown(&t->world);
    eng_ui_shutdown(&t->ui);
    close_render_fixture(&t->f);
}

static void ed_frames(EdTest *t, int n)
{
    for (int i = 0; i < n; i++) {
        if (eng_ui_begin_frame(&t->ui, (int)SIZE, (int)SIZE, 1.0F / 60.0F)) {
            EngUiEditorResult result = {0};
            eng_ui_draw_editor(&t->ui, &t->world, &t->editor, &result);
            if (result.save_requested) {
                t->saves++;
            }
            eng_ui_end_frame(&t->ui);
        }
    }
}

static void ed_move(EdTest *t, float x, float y)
{
    eng_ui_add_mouse_pos(&t->ui, x, y);
    ed_frames(t, 2);
}

static void ed_click(EdTest *t, const float r[4])
{
    ed_move(t, (r[0] + r[2]) * 0.5F, (r[1] + r[3]) * 0.5F);
    eng_ui_add_mouse_button(&t->ui, 0, true);
    ed_frames(t, 2);
    eng_ui_add_mouse_button(&t->ui, 0, false);
    ed_frames(t, 2);
}

UTEST(eng_editor_ui, the_hierarchy_lists_entities_parent_first_and_indents_children)
{
    EdTest t;
    if (!open_ed(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ed_frames(&t, 3);
    EXPECT_EQ(t.ui.editor_row_count, 3U);
    float ra[4];
    float rb[4];
    float rc[4];
    ASSERT_TRUE(eng_ui_editor_row_rect(&t.ui, t.a, ra));
    ASSERT_TRUE(eng_ui_editor_row_rect(&t.ui, t.b, rb));
    ASSERT_TRUE(eng_ui_editor_row_rect(&t.ui, t.c, rc));
    /* alpha, puis son enfant beta, puis gamma : de haut en bas, sans chevauchement */
    EXPECT_LE(ra[3], rb[1]);
    EXPECT_LE(rb[3], rc[1]);
    EXPECT_GT(rb[0], ra[0]); /* l'enfant est décalé vers la droite */
    EXPECT_FALSE(eng_ui_editor_row_rect(&t.ui, ENG_ENTITY_NULL, ra));
    close_ed(&t);
}

UTEST(eng_editor_ui, clicking_a_row_selects_that_entity)
{
    EdTest t;
    if (!open_ed(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ed_frames(&t, 3);
    float r[4];
    ASSERT_TRUE(eng_ui_editor_row_rect(&t.ui, t.c, r));
    ed_click(&t, r);
    EXPECT_EQ(eng_editor_selected(&t.editor, &t.world).index, t.c.index);
    ASSERT_TRUE(eng_ui_editor_row_rect(&t.ui, t.b, r));
    ed_click(&t, r);
    EXPECT_EQ(eng_editor_selected(&t.editor, &t.world).index, t.b.index);
    close_ed(&t);
}

UTEST(eng_editor_ui, the_save_button_requests_one_save_per_click)
{
    EdTest t;
    if (!open_ed(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ed_frames(&t, 3);
    EXPECT_EQ(t.saves, 0U);
    ed_click(&t, t.ui.editor_save_rect);
    EXPECT_EQ(t.saves, 1U);
    ed_frames(&t, 5);
    EXPECT_EQ(t.saves, 1U); /* maintenir la souris ne répète pas */
    ed_click(&t, t.ui.editor_save_rect);
    EXPECT_EQ(t.saves, 2U);
    close_ed(&t);
}

UTEST(eng_editor_ui, new_and_delete_buttons_edit_the_world)
{
    EdTest t;
    if (!open_ed(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ed_frames(&t, 3);
    const uint32_t before = eng_world_entity_count(&t.world);
    ed_click(&t, t.ui.editor_add_rect);
    EXPECT_EQ(eng_world_entity_count(&t.world), before + 1U);
    const EngEntity created = eng_editor_selected(&t.editor, &t.world);
    ASSERT_NE(created.generation, 0U);
    ed_frames(&t, 3);
    float r[4];
    EXPECT_TRUE(eng_ui_editor_row_rect(&t.ui, created, r)); /* la nouvelle ligne est visible */
    ed_click(&t, t.ui.editor_delete_rect);
    EXPECT_EQ(eng_world_entity_count(&t.world), before);
    EXPECT_FALSE(eng_world_alive(&t.world, created));
    EXPECT_EQ(eng_editor_selected(&t.editor, &t.world).generation, 0U);
    /* Supprimer sans sélection ne détruit rien. */
    ed_click(&t, t.ui.editor_delete_rect);
    EXPECT_EQ(eng_world_entity_count(&t.world), before);
    /* Supprimer un parent emporte son enfant. */
    ed_frames(&t, 3);
    ASSERT_TRUE(eng_ui_editor_row_rect(&t.ui, t.a, r));
    ed_click(&t, r);
    ed_click(&t, t.ui.editor_delete_rect);
    EXPECT_FALSE(eng_world_alive(&t.world, t.a));
    EXPECT_FALSE(eng_world_alive(&t.world, t.b));
    EXPECT_TRUE(eng_world_alive(&t.world, t.c));
    close_ed(&t);
}

UTEST(eng_editor_ui, the_inspector_shows_the_selected_entity_fields_and_edits_them)
{
    EdTest t;
    if (!open_ed(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    float r[4];
    ed_frames(&t, 3);
    /* Rien de sélectionné : pas d'inspecteur. */
    EXPECT_FALSE(eng_ui_editor_field_rect(&t.ui, ENG_COMPONENT_TRANSFORM, 0, r));
    ASSERT_TRUE(eng_ui_editor_row_rect(&t.ui, t.c, r));
    ed_click(&t, r);
    ed_frames(&t, 3);
    float pos[4];
    float rot[4];
    float scl[4];
    ASSERT_TRUE(eng_ui_editor_field_rect(&t.ui, ENG_COMPONENT_TRANSFORM, 0, pos));
    ASSERT_TRUE(eng_ui_editor_field_rect(&t.ui, ENG_COMPONENT_TRANSFORM, 1, rot));
    ASSERT_TRUE(eng_ui_editor_field_rect(&t.ui, ENG_COMPONENT_TRANSFORM, 2, scl));
    EXPECT_LE(pos[3], rot[1]);
    EXPECT_LE(rot[3], scl[1]);
    EXPECT_FALSE(eng_ui_editor_field_rect(&t.ui, ENG_COMPONENT_TRANSFORM, 3, r));
    /* Glisser dans la première case de « position » vers la droite augmente x seulement. */
    const EngTransform *tr = eng_world_get(&t.world, t.c, ENG_COMPONENT_TRANSFORM);
    EXPECT_EQ(tr->position[0], 0.0F);
    const float y = (pos[1] + pos[3]) * 0.5F;
    ed_move(&t, pos[0] + 10.0F, y);
    eng_ui_add_mouse_button(&t.ui, 0, true);
    ed_frames(&t, 2);
    ed_move(&t, pos[0] + 60.0F, y);
    eng_ui_add_mouse_button(&t.ui, 0, false);
    ed_frames(&t, 2);
    EXPECT_GT(tr->position[0], 0.5F);
    EXPECT_EQ(tr->position[1], 0.0F);
    EXPECT_EQ(tr->position[2], 0.0F);
    /* Échelle et rotation restent intactes. */
    EXPECT_EQ(tr->scale[0], 1.0F);
    EXPECT_EQ(tr->rotation[3], 1.0F);
    /* Les autres entités ne bougent pas. */
    const EngTransform *ta = eng_world_get(&t.world, t.a, ENG_COMPONENT_TRANSFORM);
    EXPECT_EQ(ta->position[0], 0.0F);
    close_ed(&t);
}

UTEST(eng_editor_ui, a_stale_selection_is_dropped_by_the_window)
{
    EdTest t;
    if (!open_ed(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ed_frames(&t, 3);
    ASSERT_TRUE(eng_editor_select(&t.editor, &t.world, t.c));
    ed_frames(&t, 3);
    float r[4];
    EXPECT_TRUE(eng_ui_editor_field_rect(&t.ui, ENG_COMPONENT_TRANSFORM, 0, r));
    (void)eng_world_destroy(&t.world, t.c);
    ed_frames(&t, 3);
    EXPECT_FALSE(eng_ui_editor_field_rect(&t.ui, ENG_COMPONENT_TRANSFORM, 0, r));
    /* Le premier champ dessiné est celui de Name : son ancienne case ne doit pas survivre. */
    EXPECT_FALSE(eng_ui_editor_field_rect(&t.ui, ENG_COMPONENT_NAME, 0, r));
    EXPECT_EQ(t.ui.editor_field_count, 0U);
    EXPECT_EQ(t.editor.selected.generation, 0U);
    close_ed(&t);
}

typedef struct Big {
    float v[40];
} Big;

static const char *const BIG_NAMES[ENG_UI_EDITOR_MAX_FIELDS + 4U] = {
    "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",  "f8",  "f9",  "f10", "f11",
    "f12", "f13", "f14", "f15", "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
    "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31", "f32", "f33", "f34", "f35"};

UTEST(eng_editor_ui, a_custom_component_is_shown_and_field_tracking_is_bounded)
{
    EdTest t;
    if (!open_ed(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* 31 champs : avec Name (1) + Transform (3) on dépasse largement le suivi (32). */
    EngFieldDesc fields[ENG_WORLD_MAX_FIELDS];
    const uint32_t count = 31;
    for (uint32_t i = 0; i < count; i++) {
        fields[i] = (EngFieldDesc){BIG_NAMES[i], ENG_FIELD_FLOAT, (uint32_t)(i * sizeof(float)), 0};
    }
    const EngComponentDesc desc = {"Big", sizeof(Big), fields, count, NULL};
    const uint32_t id = eng_world_register(&t.world, &desc);
    ASSERT_NE(id, 0U);
    ASSERT_NE(eng_world_add(&t.world, t.c, id), NULL);
    ASSERT_TRUE(eng_editor_select(&t.editor, &t.world, t.c));
    ed_frames(&t, 3);
    float r[4];
    /* Le dernier composant enregistré est affiché (premier champ) : l'inspecteur parcourt tous
     * les composants. */
    EXPECT_TRUE(eng_ui_editor_field_rect(&t.ui, id, 0, r));
    /* Name (1) + Transform (3) + 28 premiers champs de Big = 32 suivis ; les suivants ne le sont
     * pas mais le programme ne dépasse pas la table (ASan). */
    EXPECT_EQ(t.ui.editor_field_count, ENG_UI_EDITOR_MAX_FIELDS);
    EXPECT_TRUE(eng_ui_editor_field_rect(&t.ui, id, 27, r));
    EXPECT_FALSE(eng_ui_editor_field_rect(&t.ui, id, 28, r));
    close_ed(&t);
}

UTEST(eng_editor_ui, a_large_world_shows_a_bounded_number_of_rows)
{
    EdTest t;
    if (!open_ed(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    for (int i = 0; i < 40; i++) {
        (void)make(&t.world, "x", ENG_ENTITY_NULL);
    }
    ed_frames(&t, 3);
    EXPECT_EQ(t.ui.editor_row_count, 43U);
    for (int i = 0; i < 40; i++) {
        (void)make(&t.world, "y", ENG_ENTITY_NULL);
    }
    ed_frames(&t, 3);
    EXPECT_EQ(t.ui.editor_row_count, ENG_UI_EDITOR_MAX_ROWS);
    EXPECT_EQ(eng_world_entity_count(&t.world), 83U); /* 3 + 80 : tous créés, 64 affichés */
    close_ed(&t);
}

#endif
