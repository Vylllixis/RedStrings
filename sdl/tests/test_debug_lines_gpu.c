#include <SDL3/SDL.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "engine/debug_lines.h"
#include "engine/debug_lines_gpu.h"
#include "engine/memory.h"
#include "test_render_util.h"
#include "utest.h"

/* Debug draw dessiné pour de vrai (M1-11) et buffers de streaming du renderer : les lignes sont
 * envoyées au GPU, dessinées dans une cible de 64 x 64 pixels, et on lit les pixels. Avec une
 * matrice vue-projection identité, les coordonnées des lignes sont directement celles du NDC : x et
 * y dans [-1, 1] (y vers le haut), profondeur dans [0, 1]. Le centre du pixel (px, py) est en
 * x = (px + 0,5) / 32 - 1, y = 1 - (py + 0,5) / 32. Sans GPU ou sans shaders compilés, ces tests
 * s'arrêtent là. */

#ifdef OW_SHADER_DIR

#define SIZE 64U
#define RGBA8 ((uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM)

/* Pixel (px, py) -> NDC du centre du pixel. */
static float ndc_x(uint32_t px)
{
    return ((float)px + 0.5F) / 32.0F - 1.0F;
}

static float ndc_y(uint32_t py)
{
    return 1.0F - ((float)py + 0.5F) / 32.0F;
}

typedef struct TestLines {
    RenderFixture f;
    EngArena permanent;
    EngRenderer r;
    EngDebugLines lines;
    EngDebugLinesGpu gpu;
} TestLines;

static bool open_lines_test(TestLines *t, bool depth_buffer, bool debug)
{
    if (!open_render_fixture_with(&t->f, debug)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "lines-test", (size_t)4 << 20)) {
        close_render_fixture(&t->f);
        return false;
    }
    const EngRendererDesc desc = {.offline_dir = OW_SHADER_DIR,
                                  .max_commands = 64,
                                  .max_uniform_bytes = 16384,
                                  .depth_buffer = depth_buffer};
    if (!eng_renderer_init(&t->r, &t->f.gpu, &t->permanent, &t->f.scratch, &desc) ||
        !eng_debug_lines_init(&t->lines, &t->permanent, 256) ||
        !eng_debug_lines_gpu_create(&t->gpu, &t->r, 256, RGBA8)) {
        (void)eng_renderer_shutdown(&t->r);
        eng_arena_shutdown(&t->permanent);
        close_render_fixture(&t->f);
        return false;
    }
    return true;
}

static uint32_t close_lines_test(TestLines *t)
{
    eng_debug_lines_gpu_destroy(&t->gpu, &t->r);
    const uint32_t leaks = eng_renderer_shutdown(&t->r);
    eng_arena_shutdown(&t->permanent);
    close_render_fixture(&t->f);
    return leaks;
}

#define CLOSE_LINES(t) EXPECT_EQ(close_lines_test(&(t)), (uint32_t)0)

static void prepare_streams(SDL_GPUCommandBuffer *commands, void *context)
{
    (void)eng_renderer_flush_streams((EngRenderer *)context, commands);
}

static void execute_queue(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    EngRenderer *r = (EngRenderer *)context;
    (void)eng_renderer_execute(r, &r->queue, commands, pass);
}

static uint8_t g_pixels[SIZE * SIZE * 4];

/* Dessine la file du renderer (flush des streams avant la passe) dans g_pixels. */
static bool render_queue(TestLines *t)
{
    return render_to_pixels_prepared(&t->f, eng_renderer_depth_format(&t->r), prepare_streams,
                                     execute_queue, &t->r, SIZE, g_pixels);
}

static bool is_black(uint32_t x, uint32_t y)
{
    return pixel_is(g_pixels, SIZE, x, y, 0, 0, 0, 0);
}

static bool is_color(uint32_t x, uint32_t y, EngDebugColor c)
{
    return pixel_is(g_pixels, SIZE, x, y, c.r, c.g, c.b, 2);
}

static void identity(mat4 m)
{
    glm_mat4_identity(m);
}

/* Ligne horizontale sur la rangée `row`, de la colonne `x0` à `x1` (centres de pixels), à la
 * profondeur `z`. */
static bool hline(EngDebugLines *lines, uint32_t row, uint32_t x0, uint32_t x1, float z,
                  EngDebugColor color)
{
    return eng_debug_line(lines, (float[3]){ndc_x(x0), ndc_y(row), z},
                          (float[3]){ndc_x(x1), ndc_y(row), z}, color);
}

static bool vline(EngDebugLines *lines, uint32_t col, uint32_t y0, uint32_t y1, float z,
                  EngDebugColor color)
{
    return eng_debug_line(lines, (float[3]){ndc_x(col), ndc_y(y0), z},
                          (float[3]){ndc_x(col), ndc_y(y1), z}, color);
}

UTEST(eng_debug_lines_gpu, a_line_is_drawn_one_pixel_wide_in_its_color)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ASSERT_TRUE(hline(&t.lines, 16, 8, 56, 0.5F, ENG_DEBUG_RED));
    mat4 vp;
    identity(vp);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &t.lines, vp), 1U);
    ASSERT_TRUE(render_queue(&t));
    EXPECT_TRUE(is_color(32, 16, ENG_DEBUG_RED));
    EXPECT_TRUE(is_color(20, 16, ENG_DEBUG_RED));
    EXPECT_TRUE(is_black(32, 15)); /* une seule rangée */
    EXPECT_TRUE(is_black(32, 17));
    EXPECT_TRUE(is_black(2, 16)); /* hors du segment */
    EXPECT_TRUE(is_black(62, 16));
    CLOSE_LINES(t);
}

UTEST(eng_debug_lines_gpu, each_line_keeps_its_own_color)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ASSERT_TRUE(hline(&t.lines, 10, 4, 60, 0.5F, ENG_DEBUG_GREEN));
    ASSERT_TRUE(vline(&t.lines, 40, 4, 60, 0.5F, ENG_DEBUG_BLUE));
    ASSERT_TRUE(hline(&t.lines, 50, 4, 60, 0.5F, ENG_DEBUG_YELLOW));
    mat4 vp;
    identity(vp);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &t.lines, vp), 1U);
    ASSERT_TRUE(render_queue(&t));
    EXPECT_TRUE(is_color(20, 10, ENG_DEBUG_GREEN));
    EXPECT_TRUE(is_color(40, 30, ENG_DEBUG_BLUE));
    EXPECT_TRUE(is_color(20, 50, ENG_DEBUG_YELLOW));
    CLOSE_LINES(t);
}

/* ── Profondeur ── */

typedef struct OccluderVertex {
    float position[3];
    float color[3];
} OccluderVertex;

/* Un triangle opaque bleu qui couvre toute la cible, à la profondeur 0,3, avec test et écriture de
 * profondeur : ce qui est derrière (z > 0,3) est caché, ce qui est devant (z < 0,3) reste visible.
 */
typedef struct Occluder {
    EngBufferHandle buffer;
    EngPipelineHandle pipeline;
} Occluder;

static bool make_occluder(TestLines *t, Occluder *o)
{
    const OccluderVertex vertices[3] = {
        {{-1, -1, 0.3F}, {0, 0, 1}}, {{3, -1, 0.3F}, {0, 0, 1}}, {{-1, 3, 0.3F}, {0, 0, 1}}};
    const EngBufferDesc buffer = {
        .usage = ENG_BUFFER_VERTEX, .size = sizeof vertices, .data = vertices};
    o->buffer = eng_renderer_create_buffer(&t->r, &buffer);
    EngPipelineDesc d = {.name = "occluder",
                         .vertex_shader = "cube.vert",
                         .fragment_shader = "triangle.frag",
                         .vertex_stride = sizeof(OccluderVertex),
                         .attribute_count = 2,
                         .cull_mode = ENG_CULL_NONE,
                         .color_format = RGBA8,
                         .depth_test = true,
                         .depth_write = true};
    d.attributes[0] = (EngVertexAttribute){
        .location = 0, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(OccluderVertex, position)};
    d.attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(OccluderVertex, color)};
    o->pipeline = eng_renderer_create_pipeline(&t->r, &d);
    return eng_renderer_buffer_is_valid(&t->r, o->buffer) &&
           eng_renderer_pipeline_is_valid(&t->r, o->pipeline);
}

static void destroy_occluder(TestLines *t, Occluder *o)
{
    (void)eng_renderer_destroy_pipeline(&t->r, o->pipeline);
    (void)eng_renderer_destroy_buffer(&t->r, o->buffer);
}

static void draw_occluder(const Occluder *o, EngRenderQueue *q)
{
    mat4 mvp;
    identity(mvp);
    const EngDrawCmd cmd = {.pipeline = o->pipeline, .vertex_buffer = o->buffer, .count = 3};
    (void)eng_render_draw_uniforms(q, &cmd, mvp, sizeof mvp);
}

UTEST(eng_debug_lines_gpu, depth_lines_are_hidden_by_geometry_and_overlay_lines_are_not)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    Occluder o;
    ASSERT_TRUE(make_occluder(&t, &o));
    /* testées : devant l'écran bleu (z = 0,1) rangée 16 ; derrière (z = 0,6) rangée 32 */
    ASSERT_TRUE(hline(&t.lines, 16, 8, 56, 0.1F, ENG_DEBUG_RED));
    ASSERT_TRUE(hline(&t.lines, 32, 8, 56, 0.6F, ENG_DEBUG_RED));
    /* superposée : derrière aussi (z = 0,6), rangée 48 */
    eng_debug_lines_set_overlay(&t.lines, true);
    ASSERT_TRUE(hline(&t.lines, 48, 8, 56, 0.6F, ENG_DEBUG_GREEN));
    mat4 vp;
    identity(vp);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    draw_occluder(&o, q);
    EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &t.lines, vp), 2U);
    ASSERT_TRUE(render_queue(&t));
    EXPECT_TRUE(is_color(32, 16, ENG_DEBUG_RED));                /* devant : visible */
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 32, 0, 0, 255, 2)); /* derrière : cachée par le bleu */
    EXPECT_TRUE(is_color(32, 48, ENG_DEBUG_GREEN)); /* superposée : visible malgré tout */
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 40, 0, 0, 255, 2)); /* le fond reste l'écran bleu */
    destroy_occluder(&t, &o);
    CLOSE_LINES(t);
}

UTEST(eng_debug_lines_gpu, depth_lines_do_not_write_depth_so_the_last_one_drawn_wins)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* deux lignes qui se croisent en (32, 32) : la rouge est plus proche (0,2) que la verte (0,8),
     * mais dessinée avant. Sans écriture de profondeur, la verte, dessinée après, recouvre. */
    ASSERT_TRUE(hline(&t.lines, 32, 8, 56, 0.2F, ENG_DEBUG_RED));
    ASSERT_TRUE(vline(&t.lines, 32, 8, 56, 0.8F, ENG_DEBUG_GREEN));
    mat4 vp;
    identity(vp);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &t.lines, vp), 1U);
    ASSERT_TRUE(render_queue(&t));
    EXPECT_TRUE(is_color(20, 32, ENG_DEBUG_RED));
    EXPECT_TRUE(is_color(32, 20, ENG_DEBUG_GREEN));
    EXPECT_TRUE(is_color(32, 32, ENG_DEBUG_GREEN));
    CLOSE_LINES(t);
}

UTEST(eng_debug_lines_gpu, without_a_depth_buffer_every_line_is_drawn)
{
    TestLines t;
    if (!open_lines_test(&t, false, false)) { /* le pipeline « testé » se replie sans test */
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EXPECT_EQ(eng_renderer_depth_format(&t.r), (uint32_t)0);
    ASSERT_TRUE(hline(&t.lines, 16, 8, 56, 0.9F, ENG_DEBUG_RED));
    mat4 vp;
    identity(vp);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &t.lines, vp), 1U);
    ASSERT_TRUE(render_queue(&t));
    EXPECT_TRUE(is_color(32, 16, ENG_DEBUG_RED));
    CLOSE_LINES(t);
}

/* ── Envoi des lignes ── */

UTEST(eng_debug_lines_gpu, only_non_empty_lists_become_draws)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    mat4 vp;
    identity(vp);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &t.lines, vp), 0U); /* rien */
    EXPECT_EQ(q->count, 0U);
    eng_debug_lines_set_overlay(&t.lines, true);
    ASSERT_TRUE(hline(&t.lines, 16, 8, 56, 0.5F, ENG_DEBUG_RED));
    EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &t.lines, vp), 1U); /* superposée seule */
    EXPECT_EQ(q->count, 1U);
    ASSERT_TRUE(render_queue(&t));
    EXPECT_TRUE(is_color(32, 16, ENG_DEBUG_RED));
    CLOSE_LINES(t);
}

UTEST(eng_debug_lines_gpu, a_long_depth_list_does_not_overlap_the_overlay_list)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* 128 lignes rouges (sommets 0 à 255), puis une bleue : sommets 256 et 257, c'est-à-dire
     * au-delà de max_lines sommets, là où commencerait une liste superposée mal placée */
    for (int i = 0; i < 128; i++) {
        ASSERT_TRUE(hline(&t.lines, 16, 8, 56, 0.5F, ENG_DEBUG_RED));
    }
    ASSERT_TRUE(hline(&t.lines, 20, 8, 56, 0.5F, ENG_DEBUG_BLUE));
    eng_debug_lines_set_overlay(&t.lines, true);
    ASSERT_TRUE(hline(&t.lines, 48, 8, 56, 0.5F, ENG_DEBUG_GREEN));
    mat4 vp;
    identity(vp);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &t.lines, vp), 2U);
    ASSERT_TRUE(render_queue(&t));
    EXPECT_TRUE(is_color(32, 16, ENG_DEBUG_RED));
    EXPECT_TRUE(is_color(32, 20, ENG_DEBUG_BLUE));
    EXPECT_TRUE(is_color(32, 48, ENG_DEBUG_GREEN));
    CLOSE_LINES(t);
}

UTEST(eng_debug_lines_gpu, the_view_projection_matrix_is_applied)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ASSERT_TRUE(hline(&t.lines, 16, 8, 56, 0.5F, ENG_DEBUG_RED));
    mat4 vp;
    identity(vp);
    vp[0][0] = 0.5F; /* x et y divisés par 2 : la ligne y = 0,484 passe en y = 0,242 (rangée 24) */
    vp[1][1] = 0.5F;
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &t.lines, vp), 1U);
    ASSERT_TRUE(render_queue(&t));
    EXPECT_TRUE(is_color(32, 24, ENG_DEBUG_RED));
    EXPECT_TRUE(is_black(32, 16));
    EXPECT_TRUE(is_black(5, 24)); /* x aussi : le segment couvre les colonnes 16 à 48 */
    EXPECT_TRUE(is_color(20, 24, ENG_DEBUG_RED));
    CLOSE_LINES(t);
}

UTEST(eng_debug_lines_gpu, lines_larger_than_the_gpu_buffer_are_refused_whole)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngDebugLines big;
    ASSERT_TRUE(eng_debug_lines_init(&big, &t.permanent, 257)); /* le buffer GPU en prévoit 256 */
    ASSERT_TRUE(hline(&big, 16, 8, 56, 0.5F, ENG_DEBUG_RED));
    mat4 vp;
    identity(vp);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &big, vp), 0U);
    EXPECT_EQ(q->count, 0U);
    CLOSE_LINES(t);
}

UTEST(eng_debug_lines_gpu, each_frame_replaces_the_previous_lines)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    mat4 vp;
    identity(vp);
    for (int frame = 0; frame < 3; frame++) {
        eng_debug_lines_clear(&t.lines);
        const uint32_t row = 16U + 12U * (uint32_t)frame;
        ASSERT_TRUE(hline(&t.lines, row, 8, 56, 0.5F, ENG_DEBUG_RED));
        EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
        EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &t.lines, vp), 1U);
        ASSERT_TRUE(render_queue(&t));
        EXPECT_TRUE(is_color(32, row, ENG_DEBUG_RED));
        EXPECT_TRUE(is_black(32, row == 16U ? 28U : 16U)); /* l'image précédente a disparu */
    }
    CLOSE_LINES(t);
}

UTEST(eng_debug_lines_gpu, a_real_shape_shows_up_with_its_geometry)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* AABB centrée : x, y dans [-0,5, 0,5] en NDC, z = 0,5, vue de face : un carré de 32 pixels de
     * côté, de la colonne 16 à 47 et de la rangée 16 à 47 (les 4 autres arêtes sont dans l'axe de
     * la vue, réduites à des points). */
    ASSERT_TRUE(eng_debug_aabb(&t.lines, (float[3]){-0.5F, -0.5F, 0.5F},
                               (float[3]){0.5F, 0.5F, 0.5F}, ENG_DEBUG_CYAN));
    mat4 vp;
    identity(vp);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &t.lines, vp), 1U);
    ASSERT_TRUE(render_queue(&t));
    EXPECT_TRUE(is_color(32, 16, ENG_DEBUG_CYAN) || is_color(32, 15, ENG_DEBUG_CYAN));
    EXPECT_TRUE(is_color(32, 47, ENG_DEBUG_CYAN) || is_color(32, 48, ENG_DEBUG_CYAN));
    EXPECT_TRUE(is_color(16, 32, ENG_DEBUG_CYAN) || is_color(15, 32, ENG_DEBUG_CYAN));
    EXPECT_TRUE(is_color(47, 32, ENG_DEBUG_CYAN) || is_color(48, 32, ENG_DEBUG_CYAN));
    EXPECT_TRUE(is_black(32, 32)); /* l'intérieur est vide : fil de fer */
    EXPECT_TRUE(is_black(4, 4));
    CLOSE_LINES(t);
}

/* ── Buffers de streaming du renderer ── */

UTEST(eng_renderer_stream, writes_are_validated)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferHandle sb = eng_renderer_create_stream_buffer(&t.r, ENG_BUFFER_VERTEX, 64);
    ASSERT_TRUE(eng_renderer_buffer_is_valid(&t.r, sb));
    uint8_t data[128] = {0};
    EXPECT_TRUE(eng_renderer_stream_write(&t.r, sb, 0, data, 64));
    EXPECT_TRUE(eng_renderer_stream_write(&t.r, sb, 32, data, 32)); /* jusqu'au bout */
    EXPECT_TRUE(eng_renderer_stream_write(&t.r, sb, 64, data, 0));  /* vide, à la fin : permis */
    EXPECT_FALSE(eng_renderer_stream_write(&t.r, sb, 0, data, 65)); /* trop grand */
    EXPECT_FALSE(eng_renderer_stream_write(&t.r, sb, 33, data, 32));
    EXPECT_FALSE(eng_renderer_stream_write(&t.r, sb, 65, data, 0));         /* hors du buffer */
    EXPECT_FALSE(eng_renderer_stream_write(&t.r, sb, UINT32_MAX, data, 2)); /* pas de débordement */
    EXPECT_FALSE(eng_renderer_stream_write(&t.r, sb, 0, NULL, 4));
    EXPECT_FALSE(eng_renderer_stream_write(&t.r, ENG_BUFFER_NULL, 0, data, 4));
    /* un buffer immuable n'accepte pas d'écriture */
    const EngBufferDesc fixed_desc = {.usage = ENG_BUFFER_VERTEX, .size = 64, .data = data};
    const EngBufferHandle fixed = eng_renderer_create_buffer(&t.r, &fixed_desc);
    ASSERT_TRUE(eng_renderer_buffer_is_valid(&t.r, fixed));
    EXPECT_FALSE(eng_renderer_stream_write(&t.r, fixed, 0, data, 4));
    /* handle périmé */
    EXPECT_TRUE(eng_renderer_destroy_buffer(&t.r, sb));
    EXPECT_FALSE(eng_renderer_stream_write(&t.r, sb, 0, data, 4));
    EXPECT_TRUE(eng_renderer_destroy_buffer(&t.r, fixed));
    CLOSE_LINES(t);
}

UTEST(eng_renderer_stream, creation_is_validated)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EXPECT_FALSE(eng_renderer_buffer_is_valid(
        &t.r, eng_renderer_create_stream_buffer(&t.r, ENG_BUFFER_VERTEX, 0)));
    EXPECT_FALSE(eng_renderer_buffer_is_valid(
        &t.r, eng_renderer_create_stream_buffer(&t.r, ENG_BUFFER_NONE, 64)));
    const EngBufferHandle index = eng_renderer_create_stream_buffer(&t.r, ENG_BUFFER_INDEX, 64);
    EXPECT_TRUE(eng_renderer_buffer_is_valid(&t.r, index));
    EXPECT_TRUE(eng_renderer_destroy_buffer(&t.r, index));
    CLOSE_LINES(t);
}

UTEST(eng_renderer_stream, flush_reports_what_it_sent_and_forgets_it)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngBufferHandle a = eng_renderer_create_stream_buffer(&t.r, ENG_BUFFER_VERTEX, 64);
    const EngBufferHandle b = eng_renderer_create_stream_buffer(&t.r, ENG_BUFFER_VERTEX, 64);
    uint8_t data[64] = {0};
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(t.f.gpu.device);
    ASSERT_TRUE(commands != NULL);
    EXPECT_EQ(eng_renderer_flush_streams(&t.r, commands), 0U); /* rien en attente */
    EXPECT_TRUE(eng_renderer_stream_write(&t.r, a, 0, data, 16));
    EXPECT_TRUE(eng_renderer_stream_write(&t.r, a, 32, data, 16)); /* même buffer : un seul envoi */
    EXPECT_TRUE(eng_renderer_stream_write(&t.r, b, 0, data, 8));
    EXPECT_EQ(eng_renderer_flush_streams(&t.r, NULL),
              0U);                      /* sans tampon : rien, et rien d'oublié */
    EXPECT_EQ(t.r.streams_pending, 2U); /* le flush sans tampon n'a rien oublié */
    EXPECT_EQ(eng_renderer_flush_streams(&t.r, commands), 2U);
    EXPECT_EQ(t.r.streams_pending, 0U);
    EXPECT_EQ(eng_renderer_flush_streams(&t.r, commands), 0U); /* déjà envoyé */
    /* écrire puis détruire avant l'envoi : plus rien à envoyer */
    EXPECT_TRUE(eng_renderer_stream_write(&t.r, a, 0, data, 16));
    EXPECT_EQ(t.r.streams_pending, 1U);
    EXPECT_TRUE(eng_renderer_destroy_buffer(&t.r, a));
    EXPECT_EQ(t.r.streams_pending, 0U); /* la destruction retire l'écriture en attente */
    EXPECT_EQ(eng_renderer_flush_streams(&t.r, commands), 0U);
    EXPECT_TRUE(SDL_SubmitGPUCommandBuffer(commands));
    EXPECT_TRUE(eng_renderer_destroy_buffer(&t.r, b));
    CLOSE_LINES(t);
}

/* Écrit `first_bytes` octets de `lines` à l'offset `offset` du buffer de streaming du debug draw.
 */
static bool write_line(TestLines *t, EngDebugLines *lines, uint32_t vertex_offset)
{
    return eng_renderer_stream_write(&t->r, t->gpu.buffer,
                                     vertex_offset * (uint32_t)sizeof(EngDebugVertex), lines->depth,
                                     2U * (uint32_t)sizeof(EngDebugVertex));
}

static bool draw_overlay_range(TestLines *t, uint32_t first_vertex)
{
    mat4 vp;
    identity(vp);
    const EngDrawCmd cmd = {.pipeline = t->gpu.overlay_pipeline,
                            .vertex_buffer = t->gpu.buffer,
                            .count = 2,
                            .first = first_vertex};
    return eng_render_draw_uniforms(&t->r.queue, &cmd, vp, sizeof vp);
}

UTEST(eng_renderer_stream, two_writes_of_one_frame_both_reach_the_gpu_in_either_order)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* deux zones écrites séparément : sommets 0-1 et 6-7 ; les sommets 2 à 5 ne sont pas écrits (et
     * jamais dessinés) */
    EngDebugLines low;
    EngDebugLines high;
    ASSERT_TRUE(eng_debug_lines_init(&low, &t.permanent, 4));
    ASSERT_TRUE(eng_debug_lines_init(&high, &t.permanent, 4));
    for (int order = 0; order < 2; order++) {
        /* contenu différent à chaque tour : une écriture non envoyée ne peut pas être masquée par
         * les données du tour précédent, restées dans le buffer GPU */
        const uint32_t low_row = order == 0 ? 16U : 24U;
        const uint32_t high_row = order == 0 ? 40U : 52U;
        eng_debug_lines_clear(&low);
        eng_debug_lines_clear(&high);
        ASSERT_TRUE(hline(&low, low_row, 8, 56, 0.5F, ENG_DEBUG_RED));
        ASSERT_TRUE(hline(&high, high_row, 8, 56, 0.5F, ENG_DEBUG_GREEN));
        (void)eng_renderer_begin_frame(&t.r);
        if (order == 0) { /* le bas d'abord */
            ASSERT_TRUE(write_line(&t, &low, 0));
            ASSERT_TRUE(write_line(&t, &high, 6));
        } else { /* le haut d'abord : l'enveloppe est la même */
            ASSERT_TRUE(write_line(&t, &high, 6));
            ASSERT_TRUE(write_line(&t, &low, 0));
        }
        ASSERT_TRUE(draw_overlay_range(&t, 0));
        ASSERT_TRUE(draw_overlay_range(&t, 6));
        ASSERT_TRUE(render_queue(&t));
        EXPECT_TRUE(is_color(32, low_row, ENG_DEBUG_RED));
        EXPECT_TRUE(is_color(32, high_row, ENG_DEBUG_GREEN));
    }
    CLOSE_LINES(t);
}

UTEST(eng_renderer_stream, a_write_lands_at_its_own_offset)
{
    TestLines t;
    if (!open_lines_test(&t, true, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngDebugLines lines;
    ASSERT_TRUE(eng_debug_lines_init(&lines, &t.permanent, 4));
    ASSERT_TRUE(hline(&lines, 24, 8, 56, 0.5F, ENG_DEBUG_YELLOW));
    (void)eng_renderer_begin_frame(&t.r);
    ASSERT_TRUE(write_line(&t, &lines, 10)); /* seule écriture : sommets 10 et 11 */
    ASSERT_TRUE(draw_overlay_range(&t, 10));
    ASSERT_TRUE(render_queue(&t));
    EXPECT_TRUE(is_color(32, 24, ENG_DEBUG_YELLOW));
    CLOSE_LINES(t);
}

/* ── De bout en bout : sur la swapchain, avec la validation du pilote ── */

static void fill_frame_lines(EngDebugLines *lines, float time)
{
    eng_debug_lines_clear(lines);
    mat4 m;
    glm_mat4_identity(m);
    (void)eng_debug_grid(lines, (float[3]){0, 0, 0}, 10, 1.0F, ENG_DEBUG_GRAY, ENG_DEBUG_WHITE);
    (void)eng_debug_aabb(lines, (float[3]){-1, 0, -1}, (float[3]){1, 2, 1}, ENG_DEBUG_YELLOW);
    (void)eng_debug_sphere(lines, (float[3]){time, 1, 0}, 0.5F, ENG_DEBUG_CYAN, 0);
    eng_debug_lines_set_overlay(lines, true);
    (void)eng_debug_axes(lines, m, 1.0F);
    eng_debug_lines_set_overlay(lines, false);
}

UTEST(eng_debug_lines_gpu, frames_on_the_swapchain_run_clean_and_allocate_nothing)
{
    eng_mem_track_install();
    TestLines t;
    if (!open_lines_test(&t, true, true)) { /* validation du pilote activée */
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* pipelines de la swapchain, pas de la cible RGBA8 */
    eng_debug_lines_gpu_destroy(&t.gpu, &t.r);
    ASSERT_TRUE(eng_debug_lines_gpu_create(&t.gpu, &t.r, 256, 0));
    mat4 view;
    mat4 proj;
    mat4 vp;
    eng_look_at((float[3]){0, 4, 8}, (float[3]){0, 0, 0}, (float[3]){0, 1, 0}, view);
    eng_perspective(1.0F, 1.0F, 0.1F, 100.0F, proj);
    glm_mat4_mul(proj, view, vp);
    const uint32_t warmup = 4;
    const uint32_t measured = 12;
    uint32_t allocations_before = 0;
    for (uint32_t frame = 0; frame < warmup + measured; frame++) {
        if (frame == warmup) {
            allocations_before = eng_mem_alloc_count();
        }
        fill_frame_lines(&t.lines, (float)frame * 0.1F);
        EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
        EXPECT_EQ(eng_debug_lines_gpu_draw(&t.gpu, &t.r, q, &t.lines, vp), 2U);
        eng_renderer_end_frame(&t.r);
    }
    EXPECT_EQ(eng_mem_alloc_count() - allocations_before, 0U);
    EXPECT_EQ(t.r.stats.frames, (uint64_t)(warmup + measured));
    EXPECT_EQ(t.r.stats.draws, (uint64_t)(2 * (warmup + measured)));
    EXPECT_EQ(t.r.stats.rejected_commands, (uint64_t)0);
    EXPECT_EQ(t.f.gpu.frames_submitted, (uint64_t)(warmup + measured));
    EXPECT_EQ(t.f.gpu.frames_skipped, (uint64_t)0);
    EXPECT_EQ(t.r.streams_pending, 0U); /* tout a été envoyé avant la passe */
    CLOSE_LINES(t);
}

#endif /* OW_SHADER_DIR */
