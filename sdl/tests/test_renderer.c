#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "engine/renderer.h"
#include "test_render_util.h"
#include "test_util.h"
#include "utest.h"

/* Le renderer de bout en bout : handles, file de commandes, dessin réel dans une texture dont on
 * lit les pixels. Sans GPU, les tests qui en ont besoin s'arrêtent là (voir test_render_util.h) ;
 * sans shaders compilés (OW_BUILD_SHADERS=OFF), seuls les tests purs tournent. */

#define SIZE 64U
#define TOL 12
#define RGBA8 ((uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM)

/* ── File de commandes : pure, sans GPU ── */

UTEST(eng_renderer, the_queue_records_draws_in_order_and_counts_overflow)
{
    EngDrawCmd storage[2];
    EngRenderQueue queue = {.commands = storage, .count = 0, .capacity = 2, .dropped = 0};
    EngDrawCmd cmd = {0};
    cmd.count = 3;
    EXPECT_TRUE(eng_render_draw(&queue, &cmd));
    cmd.count = 6;
    EXPECT_TRUE(eng_render_draw(&queue, &cmd));
    cmd.count = 9;
    EXPECT_FALSE(eng_render_draw(&queue, &cmd)); /* pleine : refusée, pas d'écrasement */
    EXPECT_FALSE(eng_render_draw(&queue, &cmd));
    EXPECT_EQ(queue.count, (uint32_t)2);
    EXPECT_EQ(queue.dropped, (uint32_t)2);
    EXPECT_EQ(storage[0].count, (uint32_t)3);
    EXPECT_EQ(storage[1].count, (uint32_t)6);
}

UTEST(eng_renderer, an_uninitialised_renderer_refuses_everything_harmlessly)
{
    EXPECT_EQ(ENG_BUFFER_NULL.handle.generation, (uint32_t)0);
    EXPECT_EQ(ENG_PIPELINE_NULL.handle.generation, (uint32_t)0);
    EngRenderer r = {0}; /* renderer non initialisé : tout est invalide, rien ne plante */
    EXPECT_FALSE(eng_renderer_buffer_is_valid(&r, ENG_BUFFER_NULL));
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&r, ENG_PIPELINE_NULL));
    EXPECT_FALSE(eng_renderer_destroy_buffer(&r, ENG_BUFFER_NULL));
    EXPECT_FALSE(eng_renderer_destroy_pipeline(&r, ENG_PIPELINE_NULL));
    EXPECT_TRUE(eng_renderer_begin_frame(&r) == NULL);
    EXPECT_EQ(eng_renderer_update(&r, 1.0), (uint32_t)0);
    EXPECT_EQ(eng_renderer_shutdown(&r), (uint32_t)0);
    eng_renderer_end_frame(&r);
}

#ifdef OW_SHADER_DIR

/* ── Avec un GPU ── */

typedef struct Vertex {
    float position[3];
    float color[3];
} Vertex;

static const Vertex TRIANGLE[3] = {
    {{0.0F, 0.5F, 0.0F}, {1.0F, 0.0F, 0.0F}},
    {{-0.5F, -0.5F, 0.0F}, {0.0F, 1.0F, 0.0F}},
    {{0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}},
};

typedef struct TestRenderer {
    RenderFixture f;
    EngArena permanent;
    EngRenderer r;
} TestRenderer;

static bool open_renderer(TestRenderer *t, const char *source_dir)
{
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "renderer-test", (size_t)4 << 20)) {
        close_render_fixture(&t->f);
        return false;
    }
    const EngRendererDesc desc = {
        .source_dir = source_dir, .offline_dir = OW_SHADER_DIR, .max_commands = 16};
    if (!eng_renderer_init(&t->r, &t->f.gpu, &t->permanent, &t->f.scratch, &desc)) {
        eng_arena_shutdown(&t->permanent);
        close_render_fixture(&t->f);
        return false;
    }
    return true;
}

static void close_renderer(TestRenderer *t)
{
    (void)eng_renderer_shutdown(&t->r);
    eng_arena_shutdown(&t->permanent);
    close_render_fixture(&t->f);
}

static EngPipelineDesc triangle_pipeline_desc(uint32_t color_format)
{
    EngPipelineDesc d = {.name = "triangle",
                         .vertex_shader = "triangle.vert",
                         .fragment_shader = "triangle.frag",
                         .vertex_stride = sizeof(Vertex),
                         .attribute_count = 2,
                         .color_format = color_format};
    d.attributes[0] = (EngVertexAttribute){
        .location = 0, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(Vertex, position)};
    d.attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(Vertex, color)};
    return d;
}

static EngBufferHandle make_vertex_buffer(TestRenderer *t, const void *data, uint32_t size)
{
    const EngBufferDesc d = {.usage = ENG_BUFFER_VERTEX, .size = size, .data = data};
    return eng_renderer_create_buffer(&t->r, &d);
}

static EngBufferHandle make_index_buffer(TestRenderer *t, const void *data, uint32_t size)
{
    const EngBufferDesc d = {.usage = ENG_BUFFER_INDEX, .size = size, .data = data};
    return eng_renderer_create_buffer(&t->r, &d);
}

typedef struct DrawContext {
    EngRenderer *renderer;
    const EngRenderQueue *queue;
    uint32_t drawn;
} DrawContext;

static void execute_queue(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    DrawContext *c = (DrawContext *)context;
    c->drawn = eng_renderer_execute(c->renderer, c->queue, commands, pass);
}

/* Dessine la file dans une texture 64 x 64 ; retourne le nombre de dessins émis dans *drawn. */
static bool render_queue(TestRenderer *t, uint8_t *pixels, uint32_t *drawn)
{
    DrawContext c = {&t->r, &t->r.queue, 0};
    const bool ok = render_to_pixels(&t->f, execute_queue, &c, SIZE, pixels);
    if (drawn != NULL) {
        *drawn = c.drawn;
    }
    return ok;
}

#define EXPECT_TRIANGLE(pixels)                                                                    \
    do {                                                                                           \
        EXPECT_TRUE(pixel_is((pixels), SIZE, 32, 24, 187, 30, 38, TOL));                           \
        EXPECT_TRUE(pixel_is((pixels), SIZE, 19, 45, 20, 217, 18, TOL));                           \
        EXPECT_TRUE(pixel_is((pixels), SIZE, 45, 45, 20, 10, 225, TOL));                           \
        EXPECT_TRUE(pixel_is((pixels), SIZE, 1, 1, 0, 0, 0, 0));                                   \
    } while (0)

#define EXPECT_EMPTY(pixels)                                                                       \
    do {                                                                                           \
        EXPECT_TRUE(pixel_is((pixels), SIZE, 32, 40, 0, 0, 0, 0));                                 \
        EXPECT_TRUE(pixel_is((pixels), SIZE, 32, 24, 0, 0, 0, 0));                                 \
        EXPECT_TRUE(pixel_is((pixels), SIZE, 19, 45, 0, 0, 0, 0));                                 \
    } while (0)

static uint8_t g_pixels[SIZE * SIZE * 4];

UTEST(eng_renderer, a_destroyed_resource_makes_its_handle_stale_for_good)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferHandle a = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    EXPECT_TRUE(eng_renderer_buffer_is_valid(&t.r, a));
    EXPECT_EQ(eng_pool_count(&t.r.buffers), (uint32_t)1);
    EXPECT_TRUE(eng_renderer_destroy_buffer(&t.r, a));
    EXPECT_FALSE(eng_renderer_buffer_is_valid(&t.r, a));
    EXPECT_FALSE(eng_renderer_destroy_buffer(&t.r, a)); /* double destruction refusée */
    EXPECT_EQ(eng_pool_count(&t.r.buffers), (uint32_t)0);

    /* La case est réutilisée : l'ancien handle ne doit pas désigner le nouveau buffer. */
    const EngBufferHandle b = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    EXPECT_TRUE(eng_renderer_buffer_is_valid(&t.r, b));
    EXPECT_EQ(b.handle.index, a.handle.index);
    EXPECT_TRUE(b.handle.generation != a.handle.generation);
    EXPECT_FALSE(eng_renderer_buffer_is_valid(&t.r, a));
    EXPECT_FALSE(eng_renderer_destroy_buffer(&t.r, a)); /* ne détruit pas b par erreur */
    EXPECT_TRUE(eng_renderer_buffer_is_valid(&t.r, b));
    close_renderer(&t);
}

UTEST(eng_renderer, invalid_descriptions_are_refused_and_leave_nothing_behind)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferDesc no_size = {.usage = ENG_BUFFER_VERTEX, .size = 0, .data = TRIANGLE};
    const EngBufferDesc no_data = {.usage = ENG_BUFFER_VERTEX, .size = 36, .data = NULL};
    const EngBufferDesc bad_usage = {.usage = ENG_BUFFER_NONE, .size = 36, .data = TRIANGLE};
    EXPECT_FALSE(eng_renderer_buffer_is_valid(&t.r, eng_renderer_create_buffer(&t.r, &no_size)));
    EXPECT_FALSE(eng_renderer_buffer_is_valid(&t.r, eng_renderer_create_buffer(&t.r, &no_data)));
    EXPECT_FALSE(eng_renderer_buffer_is_valid(&t.r, eng_renderer_create_buffer(&t.r, &bad_usage)));
    EXPECT_EQ(eng_pool_count(&t.r.buffers), (uint32_t)0);

    EngPipelineDesc too_many = triangle_pipeline_desc(RGBA8);
    too_many.attribute_count = ENG_MAX_VERTEX_ATTRIBUTES + 1;
    EXPECT_FALSE(
        eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &too_many)));
    EngPipelineDesc no_stride = triangle_pipeline_desc(RGBA8);
    no_stride.vertex_stride = 0;
    EXPECT_FALSE(
        eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &no_stride)));
    EngPipelineDesc missing_shader = triangle_pipeline_desc(RGBA8);
    missing_shader.fragment_shader = "n_existe_pas.frag";
    EXPECT_FALSE(
        eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &missing_shader)));
    EngPipelineDesc bad_format = triangle_pipeline_desc(RGBA8);
    bad_format.attributes[0].format = ENG_VERTEX_NONE;
    EXPECT_FALSE(
        eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &bad_format)));
    EXPECT_EQ(eng_pool_count(&t.r.pipelines), (uint32_t)0); /* aucune case perdue */
    EXPECT_EQ(eng_renderer_shutdown(&t.r), (uint32_t)0);
    close_renderer(&t);
}

UTEST(eng_renderer, the_queue_draws_the_triangle)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferHandle vb = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    const EngPipelineDesc desc = triangle_pipeline_desc(RGBA8);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));

    EngRenderQueue *queue = eng_renderer_begin_frame(&t.r);
    ASSERT_TRUE(queue != NULL);
    EXPECT_TRUE(
        eng_render_draw(queue, &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb, .count = 3}));
    uint32_t drawn = 0;
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_EQ(drawn, (uint32_t)1);
    EXPECT_TRIANGLE(g_pixels);
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 40, 60, 94, 102, TOL)); /* mélange au centre */

    /* begin_frame vide la file : l'image suivante repart d'une file vide. */
    queue = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(queue->count, (uint32_t)0);
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_EQ(drawn, (uint32_t)0);
    EXPECT_EMPTY(g_pixels);
    close_renderer(&t);
}

UTEST(eng_renderer, a_stale_or_missing_handle_rejects_only_that_draw)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferHandle vb = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    const EngBufferHandle dead = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    EXPECT_TRUE(eng_renderer_destroy_buffer(&t.r, dead));
    const EngPipelineDesc desc = triangle_pipeline_desc(RGBA8);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    const EngPipelineHandle dead_pipe = eng_renderer_create_pipeline(&t.r, &desc);
    EXPECT_TRUE(eng_renderer_destroy_pipeline(&t.r, dead_pipe));

    EngRenderQueue *queue = eng_renderer_begin_frame(&t.r);
    (void)eng_render_draw(queue,
                          &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = dead, .count = 3});
    (void)eng_render_draw(queue,
                          &(EngDrawCmd){.pipeline = dead_pipe, .vertex_buffer = vb, .count = 3});
    (void)eng_render_draw(
        queue, &(EngDrawCmd){.pipeline = ENG_PIPELINE_NULL, .vertex_buffer = vb, .count = 3});
    (void)eng_render_draw(
        queue, &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = ENG_BUFFER_NULL, .count = 3});
    (void)eng_render_draw(
        queue,
        &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb, .index_buffer = dead, .count = 3});
    (void)eng_render_draw(queue, &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb, .count = 0});
    uint32_t drawn = 0;
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_EQ(drawn, (uint32_t)0);
    EXPECT_EQ(t.r.stats.rejected_commands, (uint64_t)6);
    EXPECT_EMPTY(g_pixels);

    /* Un dessin valide dans la même file est exécuté malgré les rejetés qui le précèdent. */
    queue = eng_renderer_begin_frame(&t.r);
    (void)eng_render_draw(queue,
                          &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = dead, .count = 3});
    (void)eng_render_draw(queue, &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb, .count = 3});
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_EQ(drawn, (uint32_t)1);
    EXPECT_EQ(t.r.stats.rejected_commands, (uint64_t)7);
    EXPECT_TRIANGLE(g_pixels);
    close_renderer(&t);
}

UTEST(eng_renderer, indexed_draws_use_the_index_buffer_offsets_and_winding)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* 6 sommets : 3 hors de l'écran, puis le triangle. */
    Vertex six[6];
    for (int i = 0; i < 3; i++) {
        six[i] = (Vertex){{2.0F, 2.0F, 0.0F}, {1.0F, 1.0F, 1.0F}};
        six[3 + i] = TRIANGLE[i];
    }
    const EngBufferHandle vb3 = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    const EngBufferHandle vb6 = make_vertex_buffer(&t, six, sizeof six);
    const uint16_t ccw16[3] = {0, 1, 2};
    const uint16_t cw16[3] = {0, 2, 1};
    const uint32_t ccw32[3] = {0, 1, 2};
    const EngBufferHandle ib_ccw = make_index_buffer(&t, ccw16, sizeof ccw16);
    const EngBufferHandle ib_cw = make_index_buffer(&t, cw16, sizeof cw16);
    const EngBufferHandle ib32 = make_index_buffer(&t, ccw32, sizeof ccw32);
    const EngPipelineDesc desc = triangle_pipeline_desc(RGBA8);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
    uint32_t drawn = 0;

    EngRenderQueue *q = eng_renderer_begin_frame(&t.r); /* index 16 bits */
    (void)eng_render_draw(
        q,
        &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb3, .index_buffer = ib_ccw, .count = 3});
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_EQ(drawn, (uint32_t)1);
    EXPECT_TRIANGLE(g_pixels);

    q = eng_renderer_begin_frame(&t.r); /* index 32 bits */
    (void)eng_render_draw(q, &(EngDrawCmd){.pipeline = pipe,
                                           .vertex_buffer = vb3,
                                           .index_buffer = ib32,
                                           .index_type = ENG_INDEX_U32,
                                           .count = 3});
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_TRIANGLE(g_pixels);

    q = eng_renderer_begin_frame(&t.r); /* même triangle, sens horaire : face arrière éliminée */
    (void)eng_render_draw(
        q,
        &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb3, .index_buffer = ib_cw, .count = 3});
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_EQ(drawn, (uint32_t)1); /* émis, mais éliminé par le GPU */
    EXPECT_EMPTY(g_pixels);

    q = eng_renderer_begin_frame(&t.r); /* vertex_offset : les index visent les sommets 3 à 5 */
    (void)eng_render_draw(q, &(EngDrawCmd){.pipeline = pipe,
                                           .vertex_buffer = vb6,
                                           .index_buffer = ib_ccw,
                                           .vertex_offset = 3,
                                           .count = 3});
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_TRIANGLE(g_pixels);

    q = eng_renderer_begin_frame(&t.r); /* sans offset : les 3 premiers sommets, hors écran */
    (void)eng_render_draw(
        q,
        &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb6, .index_buffer = ib_ccw, .count = 3});
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_EMPTY(g_pixels);

    q = eng_renderer_begin_frame(&t.r); /* non indexé, `first` : à partir du sommet 3 */
    (void)eng_render_draw(
        q, &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb6, .first = 3, .count = 3});
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_TRIANGLE(g_pixels);
    close_renderer(&t);
}

UTEST(eng_renderer, culling_and_front_face_options_are_honoured)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferHandle vb = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    const uint16_t ccw[3] = {0, 1, 2}; /* antihoraire */
    const uint16_t cw[3] = {0, 2, 1};  /* horaire */
    const EngBufferHandle ib_ccw = make_index_buffer(&t, ccw, sizeof ccw);
    const EngBufferHandle ib_cw = make_index_buffer(&t, cw, sizeof cw);

    /* { élimination, sens de la face avant, antihoraire visible ?, horaire visible ? } */
    const struct {
        EngCullMode cull;
        EngFrontFace front;
        bool ccw_visible;
        bool cw_visible;
    } cases[] = {
        {ENG_CULL_BACK, ENG_FRONT_COUNTER_CLOCKWISE, true, false}, /* valeurs par défaut */
        {ENG_CULL_NONE, ENG_FRONT_COUNTER_CLOCKWISE, true, true},
        {ENG_CULL_FRONT, ENG_FRONT_COUNTER_CLOCKWISE, false, true},
        {ENG_CULL_BACK, ENG_FRONT_CLOCKWISE, false, true},
        {ENG_CULL_FRONT, ENG_FRONT_CLOCKWISE, true, false},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        EngPipelineDesc desc = triangle_pipeline_desc(RGBA8);
        desc.cull_mode = cases[i].cull;
        desc.front_face = cases[i].front;
        const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
        ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
        for (int winding = 0; winding < 2; winding++) {
            EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
            (void)eng_render_draw(q, &(EngDrawCmd){.pipeline = pipe,
                                                   .vertex_buffer = vb,
                                                   .index_buffer = winding == 0 ? ib_ccw : ib_cw,
                                                   .count = 3});
            ASSERT_TRUE(render_queue(&t, g_pixels, NULL));
            const bool expected = winding == 0 ? cases[i].ccw_visible : cases[i].cw_visible;
            if (expected) {
                EXPECT_TRIANGLE(g_pixels);
            } else {
                EXPECT_EMPTY(g_pixels);
            }
        }
        EXPECT_TRUE(eng_renderer_destroy_pipeline(&t.r, pipe));
    }
    close_renderer(&t);
}

UTEST(eng_renderer, redundant_state_changes_are_skipped)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferHandle vb = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    const EngPipelineDesc desc = triangle_pipeline_desc(RGBA8);
    const EngPipelineHandle p1 = eng_renderer_create_pipeline(&t.r, &desc);
    const EngPipelineHandle p2 = eng_renderer_create_pipeline(&t.r, &desc);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    for (int i = 0; i < 4; i++) {
        (void)eng_render_draw(q, &(EngDrawCmd){.pipeline = p1, .vertex_buffer = vb, .count = 3});
    }
    for (int i = 0; i < 3; i++) {
        (void)eng_render_draw(
            q, &(EngDrawCmd){.pipeline = p2, .vertex_buffer = vb, .count = 3, .instance_count = 2});
    }
    (void)eng_render_draw(q, &(EngDrawCmd){.pipeline = p1, .vertex_buffer = vb, .count = 3});
    uint32_t drawn = 0;
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_EQ(drawn, (uint32_t)8);
    EXPECT_EQ(t.r.stats.draws, (uint64_t)8);
    EXPECT_EQ(t.r.stats.pipeline_binds, (uint64_t)3); /* p1, p2, p1 */
    EXPECT_TRIANGLE(g_pixels);
    close_renderer(&t);
}

UTEST(eng_renderer, a_full_queue_drops_extra_draws_and_reports_them)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) { /* capacité de la file : 16 */
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferHandle vb = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    const EngPipelineDesc desc = triangle_pipeline_desc(RGBA8);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(q->capacity, (uint32_t)16);
    for (int i = 0; i < 20; i++) {
        (void)eng_render_draw(q, &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb, .count = 3});
    }
    uint32_t drawn = 0;
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_EQ(drawn, (uint32_t)16);
    EXPECT_EQ(t.r.stats.dropped_commands, (uint64_t)4);
    EXPECT_EQ(eng_renderer_begin_frame(&t.r)->dropped, (uint32_t)0); /* compteur remis à zéro */
    close_renderer(&t);
}

UTEST(eng_renderer, shutdown_frees_what_was_left_and_reports_it)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngPipelineDesc desc = triangle_pipeline_desc(RGBA8);
    EngBufferHandle b1 = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    EngBufferHandle b2 = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    EngPipelineHandle p1 = eng_renderer_create_pipeline(&t.r, &desc);
    EXPECT_EQ(eng_renderer_shutdown(&t.r), (uint32_t)3); /* 2 buffers + 1 pipeline oubliés */
    EXPECT_EQ(eng_renderer_shutdown(&t.r), (uint32_t)0); /* deuxième arrêt : rien */
    EXPECT_FALSE(eng_renderer_buffer_is_valid(&t.r, b1));
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&t.r, p1));
    EXPECT_TRUE(eng_renderer_begin_frame(&t.r) == NULL);
    (void)b2;

    /* Réinitialisation puis destruction propre : rien à libérer à l'arrêt. */
    const EngRendererDesc rd = {.offline_dir = OW_SHADER_DIR};
    ASSERT_TRUE(eng_renderer_init(&t.r, &t.f.gpu, &t.permanent, &t.f.scratch, &rd));
    b1 = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    p1 = eng_renderer_create_pipeline(&t.r, &desc);
    EXPECT_TRUE(eng_renderer_destroy_pipeline(&t.r, p1));
    EXPECT_TRUE(eng_renderer_destroy_buffer(&t.r, b1));
    EXPECT_EQ(eng_renderer_shutdown(&t.r), (uint32_t)0);
    eng_arena_shutdown(&t.permanent);
    close_render_fixture(&t.f);
}

UTEST(eng_renderer, end_frame_executes_the_queue_on_the_swapchain)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferHandle vb = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    const EngPipelineDesc desc = triangle_pipeline_desc(0); /* 0 : format de la swapchain */
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
    for (int i = 0; i < 3; i++) {
        EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
        EXPECT_EQ(q->count, (uint32_t)0);
        (void)eng_render_draw(q, &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb, .count = 3});
        eng_renderer_end_frame(&t.r);
    }
    EXPECT_EQ(t.r.stats.frames, (uint64_t)3);
    EXPECT_EQ(t.r.stats.draws, (uint64_t)3);
    EXPECT_EQ(t.r.stats.rejected_commands, (uint64_t)0);
    EXPECT_EQ(t.f.gpu.frames_submitted, (uint64_t)3);
    EXPECT_EQ(t.f.gpu.frames_skipped, (uint64_t)0);
    /* Format 0 = celui de la swapchain ; un format explicite est respecté. */
    EXPECT_EQ(eng_renderer_pipeline_color_format(&t.r, pipe), eng_gpu_swapchain_format(&t.f.gpu));
    EXPECT_TRUE(eng_renderer_pipeline_color_format(&t.r, pipe) != 0);
    const EngPipelineDesc explicit_desc = triangle_pipeline_desc(RGBA8);
    const EngPipelineHandle explicit_pipe = eng_renderer_create_pipeline(&t.r, &explicit_desc);
    EXPECT_EQ(eng_renderer_pipeline_color_format(&t.r, explicit_pipe), RGBA8);
    EXPECT_EQ(eng_renderer_pipeline_color_format(&t.r, ENG_PIPELINE_NULL), (uint32_t)0);
    close_renderer(&t);
}

/* ── Overlay (M1-12) ── */

typedef struct OverlayProbe {
    uint32_t prepare_calls;
    uint32_t draw_calls;
    uint64_t draws_at_prepare; /* dessins de la scène déjà exécutés quand prepare est appelé */
    uint64_t draws_at_draw;
    bool prepare_result;
    bool pass_was_null;
    bool commands_were_null;
    EngRenderer *r;
} OverlayProbe;

static bool probe_prepare(void *user, SDL_GPUCommandBuffer *commands)
{
    OverlayProbe *p = (OverlayProbe *)user;
    p->prepare_calls++;
    p->draws_at_prepare = p->r->stats.draws;
    p->commands_were_null = p->commands_were_null || commands == NULL;
    return p->prepare_result;
}

static void probe_draw(void *user, SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass)
{
    OverlayProbe *p = (OverlayProbe *)user;
    p->draw_calls++;
    p->draws_at_draw = p->r->stats.draws;
    p->pass_was_null = p->pass_was_null || pass == NULL;
    p->commands_were_null = p->commands_were_null || commands == NULL;
}

UTEST(eng_renderer, the_overlay_is_prepared_before_and_drawn_after_the_scene)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferHandle vb = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    const EngPipelineDesc desc = triangle_pipeline_desc(0);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
    OverlayProbe probe = {.prepare_result = true, .r = &t.r};
    eng_renderer_set_overlay(&t.r, probe_prepare, probe_draw, &probe);
    for (int i = 0; i < 3; i++) {
        EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
        (void)eng_render_draw(q, &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb, .count = 3});
        eng_renderer_end_frame(&t.r);
        /* à chaque image : prepare AVANT les dessins de la scène, draw APRÈS */
        EXPECT_EQ(probe.draws_at_prepare, (uint64_t)i);
        EXPECT_EQ(probe.draws_at_draw, (uint64_t)i + 1U);
    }
    EXPECT_EQ(probe.prepare_calls, 3U);
    EXPECT_EQ(probe.draw_calls, 3U);
    EXPECT_FALSE(probe.pass_was_null);
    EXPECT_FALSE(probe.commands_were_null);
    EXPECT_EQ(t.r.stats.overlay_passes, (uint64_t)3);
    EXPECT_EQ(t.f.gpu.overlay_passes, (uint64_t)3);
    EXPECT_EQ(t.f.gpu.frames_submitted, (uint64_t)3);
    EXPECT_EQ(t.r.stats.draws,
              (uint64_t)3); /* l'overlay ne compte pas comme un dessin de la file */
    close_renderer(&t);
}

UTEST(eng_renderer, an_overlay_with_nothing_to_draw_costs_no_pass)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    OverlayProbe probe = {.prepare_result = false, .r = &t.r};
    eng_renderer_set_overlay(&t.r, probe_prepare, probe_draw, &probe);
    for (int i = 0; i < 2; i++) {
        (void)eng_renderer_begin_frame(&t.r);
        eng_renderer_end_frame(&t.r);
    }
    EXPECT_EQ(probe.prepare_calls, 2U);
    EXPECT_EQ(probe.draw_calls, 0U);
    EXPECT_EQ(t.f.gpu.overlay_passes, (uint64_t)0);
    EXPECT_EQ(t.r.stats.overlay_passes, (uint64_t)0);
    EXPECT_EQ(t.f.gpu.frames_submitted, (uint64_t)2);
    close_renderer(&t);
}

UTEST(eng_renderer, an_overlay_without_prepare_is_drawn_every_frame_and_can_be_removed)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    OverlayProbe probe = {.r = &t.r};
    eng_renderer_set_overlay(&t.r, NULL, probe_draw, &probe);
    (void)eng_renderer_begin_frame(&t.r);
    eng_renderer_end_frame(&t.r);
    EXPECT_EQ(probe.draw_calls, 1U);
    EXPECT_EQ(probe.prepare_calls, 0U);
    eng_renderer_set_overlay(&t.r, probe_prepare, NULL, &probe); /* sans draw : retiré */
    (void)eng_renderer_begin_frame(&t.r);
    eng_renderer_end_frame(&t.r);
    EXPECT_EQ(probe.draw_calls, 1U);
    EXPECT_EQ(probe.prepare_calls, 0U); /* prepare n'est pas appelé sans draw */
    EXPECT_EQ(t.f.gpu.overlay_passes, (uint64_t)1);
    close_renderer(&t);
}

UTEST(eng_renderer, a_renderer_without_overlay_never_opens_the_overlay_pass)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    for (int i = 0; i < 2; i++) {
        (void)eng_renderer_begin_frame(&t.r);
        eng_renderer_end_frame(&t.r);
    }
    EXPECT_EQ(t.f.gpu.overlay_passes, (uint64_t)0);
    close_renderer(&t);
}

UTEST(eng_renderer, executing_without_a_pass_is_harmless)
{
    TestRenderer t = {0};
    if (!open_renderer(&t, NULL)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EXPECT_EQ(eng_renderer_update(&t.r, 1.0), (uint32_t)0); /* rien à recharger */
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(eng_renderer_execute(&t.r, q, NULL, NULL), (uint32_t)0);
    close_renderer(&t);
}

#ifdef ENG_SHADER_HOT_RELOAD

#define HOT_DIR TEST_TMP_DIR "/renderer"

static void copy_source(const char *name)
{
    char from[512];
    char to[512];
    SDL_snprintf(from, sizeof from, "%s/%s", OW_SHADER_SOURCE_DIR, name);
    SDL_snprintf(to, sizeof to, "%s/%s", HOT_DIR, name);
    size_t size = 0;
    void *data = SDL_LoadFile(from, &size);
    if (data != NULL) {
        (void)SDL_SaveFile(to, data, size);
        SDL_free(data);
    }
}

static void write_fragment(const char *text)
{
    (void)SDL_SaveFile(HOT_DIR "/triangle.frag.hlsl", text, strlen(text));
}

static bool update_until_rebuilt(TestRenderer *t, double *now, int max_steps)
{
    for (int i = 0; i < max_steps; i++) {
        *now += 0.3;
        if (eng_renderer_update(&t->r, *now) != 0) {
            return true;
        }
    }
    return false;
}

UTEST(eng_renderer, editing_a_shader_rebuilds_the_pipeline_behind_a_stable_handle)
{
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    (void)SDL_CreateDirectory(HOT_DIR);
    copy_source("triangle.vert.hlsl");
    copy_source("triangle.frag.hlsl");
    TestRenderer t = {0};
    if (!open_renderer(&t, HOT_DIR)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferHandle vb = make_vertex_buffer(&t, TRIANGLE, sizeof TRIANGLE);
    const EngPipelineDesc desc = triangle_pipeline_desc(RGBA8);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
    uint32_t drawn = 0;

    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    (void)eng_render_draw(q, &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb, .count = 3});
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 40, 60, 94, 102, TOL)); /* dégradé d'origine */

    double now = 0.0;
    EXPECT_FALSE(update_until_rebuilt(&t, &now, 4)); /* rien n'a changé */
    EXPECT_EQ(t.r.stats.pipeline_rebuilds, (uint64_t)0);

    write_fragment(
        "float4 main(float4 color : TEXCOORD0) : SV_Target0 { return float4(0, 1, 0, 1); }\n");
    ASSERT_TRUE(update_until_rebuilt(&t, &now, 10));
    EXPECT_EQ(t.r.stats.pipeline_rebuilds, (uint64_t)1);
    EXPECT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe)); /* le MÊME handle sert toujours */
    q = eng_renderer_begin_frame(&t.r); /* et les commandes qui le contiennent restent valides */
    (void)eng_render_draw(q, &(EngDrawCmd){.pipeline = pipe, .vertex_buffer = vb, .count = 3});
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 40, 0, 255, 0, 0)); /* vert uni */
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 1, 1, 0, 0, 0, 0));

    /* Erreur de compilation : le pipeline vert reste en service, pas de reconstruction. */
    write_fragment("float4 main( : SV_Target0 { return nonsense; }\n");
    EXPECT_FALSE(update_until_rebuilt(&t, &now, 10));
    EXPECT_EQ(t.r.stats.pipeline_rebuilds, (uint64_t)1);
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 40, 0, 255, 0, 0));

    /* Un deuxième pipeline sur les mêmes shaders se reconstruit aussi, indépendamment. */
    const EngPipelineHandle pipe2 = eng_renderer_create_pipeline(&t.r, &desc);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe2));
    copy_source("triangle.frag.hlsl"); /* retour au dégradé d'origine */
    ASSERT_TRUE(update_until_rebuilt(&t, &now, 10));
    EXPECT_EQ(t.r.stats.pipeline_rebuilds, (uint64_t)3); /* pipe et pipe2 */
    q = eng_renderer_begin_frame(&t.r);
    (void)eng_render_draw(q, &(EngDrawCmd){.pipeline = pipe2, .vertex_buffer = vb, .count = 3});
    ASSERT_TRUE(render_queue(&t, g_pixels, &drawn));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 40, 60, 94, 102, TOL));
    close_renderer(&t);
}

#endif /* ENG_SHADER_HOT_RELOAD */

#endif /* OW_SHADER_DIR */
