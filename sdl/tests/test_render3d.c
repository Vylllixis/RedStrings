#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "engine/math.h"
#include "engine/renderer.h"
#include "test_render_util.h"
#include "test_util.h"
#include "utest.h"

/* Uniforms et profondeur (M1-07), de bout en bout : la matrice MVP traverse la file de commandes
 * jusqu'au vertex shader, le depth buffer ordonne les surfaces indépendamment de l'ordre de dessin.
 * Les pixels attendus sont calculés à la main ou avec les maths testées dans test_math.c. Sans GPU
 * (ou sans shaders compilés), les tests qui en ont besoin s'arrêtent là. */

#define SIZE 64U
#define TOL 14

/* ── File d'uniforms : pure, sans GPU ── */

typedef struct SmallQueue {
    EngRenderQueue queue;
    EngDrawCmd commands[4];
    uint8_t uniforms[256];
} SmallQueue;

static void init_small_queue(SmallQueue *s, uint32_t uniform_capacity)
{
    memset(s, 0, sizeof *s);
    s->queue.commands = s->commands;
    s->queue.capacity = 4;
    s->queue.uniforms = s->uniforms;
    s->queue.uniform_capacity = uniform_capacity;
}

UTEST(eng_render_uniforms, data_is_copied_and_aligned_in_the_queue)
{
    SmallQueue s;
    init_small_queue(&s, sizeof s.uniforms);
    mat4 m;
    glm_translate_make(m, (vec3){1.0F, 2.0F, 3.0F});
    uint8_t expected[sizeof(mat4)];
    memcpy(expected, m, sizeof m);
    const EngDrawCmd cmd = {.count = 3};

    ASSERT_TRUE(eng_render_draw_uniforms(&s.queue, &cmd, m, sizeof m));
    glm_mat4_identity(m); /* la source change : la file doit avoir gardé sa propre copie */
    uint8_t twenty[20];
    memset(twenty, 0xAB, sizeof twenty);
    ASSERT_TRUE(eng_render_draw_uniforms(&s.queue, &cmd, twenty, sizeof twenty));
    ASSERT_TRUE(eng_render_draw_uniforms(&s.queue, &cmd, expected, sizeof expected));

    EXPECT_EQ(s.queue.count, (uint32_t)3);
    EXPECT_EQ(s.queue.commands[0].uniform_offset, (uint32_t)0);
    EXPECT_EQ(s.queue.commands[0].uniform_size, (uint32_t)64);
    EXPECT_EQ(s.queue.commands[1].uniform_offset, (uint32_t)64);
    EXPECT_EQ(s.queue.commands[1].uniform_size, (uint32_t)20);
    EXPECT_EQ(s.queue.commands[2].uniform_offset, (uint32_t)96); /* 84 arrondi à 16 */
    EXPECT_EQ(s.queue.uniform_used, (uint32_t)(96 + 64));
    EXPECT_EQ(memcmp(s.uniforms, expected, sizeof expected), 0);
    EXPECT_EQ(s.uniforms[64], (uint8_t)0xAB);
    EXPECT_EQ(s.queue.commands[0].count, (uint32_t)3); /* le reste de la commande est conservé */
}

UTEST(eng_render_uniforms, invalid_or_oversized_uniforms_are_refused_and_counted)
{
    SmallQueue s;
    init_small_queue(&s, 100);
    const EngDrawCmd cmd = {.count = 3};
    uint8_t data[64] = {0};
    EXPECT_FALSE(eng_render_draw_uniforms(&s.queue, &cmd, NULL, 16));
    EXPECT_FALSE(eng_render_draw_uniforms(&s.queue, &cmd, data, 0));
    EXPECT_EQ(s.queue.dropped, (uint32_t)2);
    EXPECT_EQ(s.queue.count, (uint32_t)0);

    ASSERT_TRUE(eng_render_draw_uniforms(&s.queue, &cmd, data, 64));
    /* il reste 100 - 64 = 36 octets, et le prochain emplacement aligné est 64 : 64 octets ne
     * tiennent pas */
    EXPECT_FALSE(eng_render_draw_uniforms(&s.queue, &cmd, data, 64));
    EXPECT_EQ(s.queue.dropped, (uint32_t)3);
    EXPECT_EQ(s.queue.count, (uint32_t)1);
    EXPECT_EQ(s.queue.uniform_used, (uint32_t)64);                   /* un refus ne consomme rien */
    EXPECT_TRUE(eng_render_draw_uniforms(&s.queue, &cmd, data, 36)); /* 64 + 36 = 100 : juste */

    /* au-delà de la taille maximale par dessin, même avec de la place */
    SmallQueue big;
    init_small_queue(&big, 100);
    static uint8_t huge[ENG_MAX_UNIFORM_BYTES + 1];
    big.queue.uniform_capacity = ENG_MAX_UNIFORM_BYTES + 1;
    big.queue.uniforms = huge; /* assez de place, mais plus que le maximum par dessin */
    EXPECT_FALSE(eng_render_draw_uniforms(&big.queue, &cmd, huge, ENG_MAX_UNIFORM_BYTES + 1));
    EXPECT_EQ(big.queue.dropped, (uint32_t)1);
}

UTEST(eng_render_uniforms, a_full_command_queue_refuses_the_draw_and_keeps_the_uniforms)
{
    SmallQueue s;
    init_small_queue(&s, sizeof s.uniforms);
    const EngDrawCmd cmd = {.count = 3};
    uint8_t data[16] = {0};
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(eng_render_draw_uniforms(&s.queue, &cmd, data, sizeof data));
    }
    EXPECT_FALSE(eng_render_draw_uniforms(&s.queue, &cmd, data, sizeof data));
    EXPECT_EQ(s.queue.dropped, (uint32_t)1);
    EXPECT_EQ(s.queue.uniform_used, (uint32_t)64);
}

/* ── Uniforms de vertex ET de fragment (M1-13) ── */

UTEST(eng_render_uniforms2, both_blocks_are_copied_aligned_and_recorded_in_the_command)
{
    SmallQueue s;
    init_small_queue(&s, sizeof s.uniforms);
    uint8_t vertex[20];
    uint8_t fragment[48];
    memset(vertex, 0xA1, sizeof vertex);
    memset(fragment, 0xB2, sizeof fragment);
    EngDrawCmd cmd = {.count = 3};
    cmd.fragment_uniform_offset = 999; /* valeurs périmées de l'appelant : écrasées */
    cmd.fragment_uniform_size = 999;
    ASSERT_TRUE(eng_render_draw_uniforms2(&s.queue, &cmd, vertex, sizeof vertex, fragment,
                                          sizeof fragment));
    memset(vertex, 0, sizeof vertex); /* la file a sa propre copie */
    memset(fragment, 0, sizeof fragment);
    const EngDrawCmd *c = &s.queue.commands[0];
    EXPECT_EQ(c->uniform_offset, (uint32_t)0);
    EXPECT_EQ(c->uniform_size, (uint32_t)20);
    EXPECT_EQ(c->fragment_uniform_offset, (uint32_t)32); /* 20 arrondi à 16 */
    EXPECT_EQ(c->fragment_uniform_size, (uint32_t)48);
    EXPECT_EQ(s.queue.uniform_used, (uint32_t)(32 + 48));
    EXPECT_EQ(s.queue.count, (uint32_t)1);
    EXPECT_EQ(c->count, (uint32_t)3);
    EXPECT_EQ(s.uniforms[0], (uint8_t)0xA1);
    EXPECT_EQ(s.uniforms[19], (uint8_t)0xA1);
    EXPECT_EQ(s.uniforms[20], (uint8_t)0); /* l'écart d'alignement n'est pas écrit */
    EXPECT_EQ(s.uniforms[32], (uint8_t)0xB2);
    EXPECT_EQ(s.uniforms[79], (uint8_t)0xB2);
    /* le dessin suivant commence après le bloc de fragment, aligné */
    ASSERT_TRUE(eng_render_draw_uniforms(&s.queue, &cmd, vertex, 16));
    EXPECT_EQ(s.queue.commands[1].uniform_offset, (uint32_t)80);
    EXPECT_EQ(s.queue.commands[1].fragment_uniform_size, (uint32_t)0); /* effacé */
    EXPECT_EQ(s.queue.commands[1].fragment_uniform_offset, (uint32_t)0);
}

UTEST(eng_render_uniforms2, one_block_alone_is_allowed_on_either_side)
{
    SmallQueue s;
    init_small_queue(&s, sizeof s.uniforms);
    const EngDrawCmd cmd = {.count = 3};
    uint8_t data[32];
    memset(data, 0xC3, sizeof data);
    ASSERT_TRUE(eng_render_draw_uniforms2(&s.queue, &cmd, NULL, 0, data, sizeof data));
    EXPECT_EQ(s.queue.commands[0].uniform_size, (uint32_t)0);
    EXPECT_EQ(s.queue.commands[0].fragment_uniform_offset, (uint32_t)0);
    EXPECT_EQ(s.queue.commands[0].fragment_uniform_size, (uint32_t)32);
    EXPECT_EQ(s.queue.uniform_used, (uint32_t)32);
    ASSERT_TRUE(eng_render_draw_uniforms2(&s.queue, &cmd, data, 16, NULL, 0));
    EXPECT_EQ(s.queue.commands[1].uniform_offset, (uint32_t)32);
    EXPECT_EQ(s.queue.commands[1].uniform_size, (uint32_t)16);
    EXPECT_EQ(s.queue.commands[1].fragment_uniform_size, (uint32_t)0);
    EXPECT_EQ(s.queue.uniform_used, (uint32_t)48);
}

UTEST(eng_render_uniforms2, incoherent_blocks_are_refused_without_consuming_anything)
{
    SmallQueue s;
    init_small_queue(&s, 100);
    const EngDrawCmd cmd = {.count = 3};
    uint8_t data[64] = {0};
    uint32_t refused = 0;
    EXPECT_FALSE(eng_render_draw_uniforms2(&s.queue, &cmd, NULL, 0, NULL, 0)); /* rien du tout */
    refused++;
    EXPECT_FALSE(eng_render_draw_uniforms2(&s.queue, &cmd, NULL, 16, data, 16)); /* NULL + taille */
    refused++;
    EXPECT_FALSE(eng_render_draw_uniforms2(&s.queue, &cmd, data, 0, data, 16)); /* données + 0 */
    refused++;
    EXPECT_FALSE(eng_render_draw_uniforms2(&s.queue, &cmd, data, 16, NULL, 16));
    refused++;
    EXPECT_FALSE(eng_render_draw_uniforms2(&s.queue, &cmd, data, 16, data, 0));
    refused++;
    EXPECT_FALSE(
        eng_render_draw_uniforms2(&s.queue, &cmd, data, ENG_MAX_UNIFORM_BYTES + 1, data, 16));
    refused++;
    EXPECT_FALSE(
        eng_render_draw_uniforms2(&s.queue, &cmd, data, 16, data, ENG_MAX_UNIFORM_BYTES + 1));
    refused++;
    EXPECT_EQ(s.queue.dropped, refused);
    EXPECT_EQ(s.queue.count, (uint32_t)0);
    EXPECT_EQ(s.queue.uniform_used, (uint32_t)0);

    /* place insuffisante pour le second bloc : 64 + 48 = 112 > 100. Rien n'est écrit, pas même le
     * premier bloc. */
    EXPECT_FALSE(eng_render_draw_uniforms2(&s.queue, &cmd, data, 64, data, 48));
    EXPECT_EQ(s.queue.count, (uint32_t)0);
    EXPECT_EQ(s.queue.uniform_used, (uint32_t)0);
    EXPECT_EQ(s.queue.dropped, refused + 1);
    /* et 64 + 36 = 100 : juste */
    EXPECT_TRUE(eng_render_draw_uniforms2(&s.queue, &cmd, data, 64, data, 36));
    EXPECT_EQ(s.queue.uniform_used, (uint32_t)100);
    /* un bloc de fragment seul dans une file où l'espace aligné manque */
    EXPECT_FALSE(eng_render_draw_uniforms2(&s.queue, &cmd, NULL, 0, data, 16));
    /* le premier bloc seul ne tient pas non plus : début aligné 112 > 100 */
    EXPECT_FALSE(eng_render_draw_uniforms2(&s.queue, &cmd, data, 16, NULL, 0));
    EXPECT_EQ(s.queue.count, (uint32_t)1);
}

UTEST(eng_render_uniforms2, stale_fields_of_the_source_command_are_cleared_on_both_sides)
{
    SmallQueue s;
    init_small_queue(&s, sizeof s.uniforms);
    EngDrawCmd cmd = {.count = 3};
    cmd.uniform_offset = 48;
    cmd.uniform_size = 32;
    cmd.fragment_uniform_offset = 64;
    cmd.fragment_uniform_size = 16;
    uint8_t data[16] = {0};
    ASSERT_TRUE(eng_render_draw_uniforms2(&s.queue, &cmd, NULL, 0, data, sizeof data));
    EXPECT_EQ(s.queue.commands[0].uniform_offset, (uint32_t)0);
    EXPECT_EQ(s.queue.commands[0].uniform_size, (uint32_t)0);
    EXPECT_EQ(s.queue.commands[0].fragment_uniform_size, (uint32_t)16);
}

UTEST(eng_render_uniforms2, a_fragment_block_alone_above_the_maximum_is_refused)
{
    static EngDrawCmd commands[2];
    static uint8_t storage[2U * ENG_MAX_UNIFORM_BYTES];
    static uint8_t data[ENG_MAX_UNIFORM_BYTES + 16U];
    EngRenderQueue queue = {.commands = commands,
                            .capacity = 2,
                            .uniforms = storage,
                            .uniform_capacity = sizeof storage};
    const EngDrawCmd cmd = {.count = 3};
    EXPECT_FALSE(eng_render_draw_uniforms2(&queue, &cmd, NULL, 0, data, ENG_MAX_UNIFORM_BYTES + 1));
    EXPECT_EQ(queue.count, (uint32_t)0);
    EXPECT_EQ(queue.dropped, (uint32_t)1);
    EXPECT_TRUE(eng_render_draw_uniforms2(&queue, &cmd, NULL, 0, data, ENG_MAX_UNIFORM_BYTES));
}

UTEST(eng_render_uniforms2, a_full_command_queue_refuses_the_draw)
{
    SmallQueue s;
    init_small_queue(&s, sizeof s.uniforms);
    const EngDrawCmd cmd = {.count = 3};
    uint8_t data[16] = {0};
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(eng_render_draw_uniforms2(&s.queue, &cmd, data, 16, data, 16));
    }
    EXPECT_FALSE(eng_render_draw_uniforms2(&s.queue, &cmd, data, 16, data, 16));
    EXPECT_EQ(s.queue.dropped, (uint32_t)1);
    EXPECT_EQ(s.queue.uniform_used, (uint32_t)(3 * 32 + 16 + 16));
}

#ifdef OW_SHADER_DIR

/* ── Avec un GPU ── */

typedef struct Vertex {
    float position[3];
    float color[3];
} Vertex;

typedef struct Test3d {
    RenderFixture f;
    EngArena permanent;
    EngRenderer r;
} Test3d;

static bool open_3d(Test3d *t, bool depth_buffer)
{
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "render3d-test", (size_t)4 << 20)) {
        close_render_fixture(&t->f);
        return false;
    }
    const EngRendererDesc desc = {.offline_dir = OW_SHADER_DIR,
                                  .max_commands = 64,
                                  .max_uniform_bytes = 8192,
                                  .depth_buffer = depth_buffer};
    if (!eng_renderer_init(&t->r, &t->f.gpu, &t->permanent, &t->f.scratch, &desc)) {
        eng_arena_shutdown(&t->permanent);
        close_render_fixture(&t->f);
        return false;
    }
    return true;
}

static uint32_t close_3d(Test3d *t)
{
    const uint32_t leaks = eng_renderer_shutdown(&t->r);
    eng_arena_shutdown(&t->permanent);
    close_render_fixture(&t->f);
    return leaks;
}

/* Fermeture + vérification : tout a été détruit par le test, aucune fuite. */
#define CLOSE_3D(t) EXPECT_EQ(close_3d(&(t)), (uint32_t)0)

static EngPipelineDesc mvp_pipeline_desc(bool depth_test, bool depth_write, EngCullMode cull)
{
    EngPipelineDesc d = {.name = "mvp",
                         .vertex_shader = "cube.vert",
                         .fragment_shader = "triangle.frag",
                         .vertex_stride = sizeof(Vertex),
                         .attribute_count = 2,
                         .cull_mode = cull,
                         .color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                         .depth_test = depth_test,
                         .depth_write = depth_write};
    d.attributes[0] = (EngVertexAttribute){
        .location = 0, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(Vertex, position)};
    d.attributes[1] = (EngVertexAttribute){
        .location = 1, .format = ENG_VERTEX_FLOAT3, .offset = offsetof(Vertex, color)};
    return d;
}

static EngBufferHandle make_buffer(Test3d *t, EngBufferUsage usage, const void *data, uint32_t size)
{
    const EngBufferDesc d = {.usage = usage, .size = size, .data = data};
    return eng_renderer_create_buffer(&t->r, &d);
}

typedef struct DrawContext {
    EngRenderer *renderer;
    uint32_t drawn;
} DrawContext;

static void execute_queue(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    DrawContext *c = (DrawContext *)context;
    c->drawn = eng_renderer_execute(c->renderer, &c->renderer->queue, commands, pass);
}

static uint8_t g_pixels[SIZE * SIZE * 4];

/* Dessine la file du renderer dans une texture 64 x 64, avec la profondeur du renderer. */
static bool render_queue(Test3d *t, uint32_t *drawn)
{
    DrawContext c = {&t->r, 0};
    const bool ok = render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue,
                                           &c, SIZE, g_pixels);
    if (drawn != NULL) {
        *drawn = c.drawn;
    }
    return ok;
}

/* Pixel (colonne, ligne) de l'écran 64 x 64 où se projette un point NDC. */
static void ndc_to_pixel(const vec3 ndc, uint32_t *x, uint32_t *y)
{
    *x = (uint32_t)((((ndc[0] + 1.0F) * 0.5F) * (float)SIZE) - 0.5F + 0.5F);
    *y = (uint32_t)((((1.0F - ndc[1]) * 0.5F) * (float)SIZE) - 0.5F + 0.5F);
}

/* ── Les uniforms arrivent au vertex shader ── */

static const Vertex TRIANGLE[3] = {
    {{0.0F, 0.5F, 0.0F}, {1.0F, 0.0F, 0.0F}},
    {{-0.5F, -0.5F, 0.0F}, {0.0F, 1.0F, 0.0F}},
    {{0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}},
};

UTEST(eng_render3d, an_identity_mvp_leaves_the_triangle_in_clip_space)
{
    Test3d t;
    if (!open_3d(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngPipelineDesc desc = mvp_pipeline_desc(false, false, ENG_CULL_BACK);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, TRIANGLE, sizeof TRIANGLE);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
    mat4 mvp;
    glm_mat4_identity(mvp);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    const EngDrawCmd cmd = {.pipeline = pipe, .vertex_buffer = vb, .count = 3};
    ASSERT_TRUE(eng_render_draw_uniforms(q, &cmd, mvp, sizeof mvp));
    uint32_t drawn = 0;
    ASSERT_TRUE(render_queue(&t, &drawn));
    EXPECT_EQ(drawn, (uint32_t)1);
    EXPECT_EQ(t.r.stats.uniform_pushes, (uint64_t)1);
    /* mêmes pixels que le triangle de test_renderer.c (calculés à la main) */
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 24, 187, 30, 38, TOL));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 19, 45, 20, 217, 18, TOL));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 45, 45, 20, 10, 225, TOL));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 1, 1, 0, 0, 0, 0));
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_3D(t);
}

UTEST(eng_render3d, a_translation_in_the_uniform_moves_the_triangle_by_the_expected_pixels)
{
    Test3d t;
    if (!open_3d(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngPipelineDesc desc = mvp_pipeline_desc(false, false, ENG_CULL_BACK);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, TRIANGLE, sizeof TRIANGLE);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
    /* +0,5 en x = +16 pixels. Une matrice lue transposée par le shader enverrait la translation
     * dans la dernière ligne et ne déplacerait rien (ou ferait disparaître le triangle). */
    mat4 mvp;
    glm_translate_make(mvp, (vec3){0.5F, 0.0F, 0.0F});
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    const EngDrawCmd cmd = {.pipeline = pipe, .vertex_buffer = vb, .count = 3};
    ASSERT_TRUE(eng_render_draw_uniforms(q, &cmd, mvp, sizeof mvp));
    ASSERT_TRUE(render_queue(&t, NULL));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 48, 24, 187, 30, 38, TOL));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 35, 45, 20, 217, 18, TOL));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 61, 45, 20, 10, 225, TOL));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 24, 0, 0, 0, 0)); /* l'ancien emplacement est vide */
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_3D(t);
}

UTEST(eng_render3d, each_draw_gets_its_own_matrix)
{
    Test3d t;
    if (!open_3d(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngPipelineDesc desc = mvp_pipeline_desc(false, false, ENG_CULL_BACK);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, TRIANGLE, sizeof TRIANGLE);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
    /* même triangle, quatre fois, décalé de -0,5 puis +0,5 : le buffer est lié une seule fois et
     * seuls les uniforms changent d'un dessin à l'autre */
    const EngDrawCmd cmd = {.pipeline = pipe, .vertex_buffer = vb, .count = 3};
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    mat4 left;
    mat4 right;
    glm_translate_make(left, (vec3){-0.5F, 0.0F, 0.0F});
    glm_scale(left, (vec3){0.5F, 0.5F, 1.0F}); /* petit triangle à gauche */
    glm_translate_make(right, (vec3){0.5F, 0.0F, 0.0F});
    glm_scale(right, (vec3){0.5F, 0.5F, 1.0F}); /* petit triangle à droite */
    ASSERT_TRUE(eng_render_draw_uniforms(q, &cmd, left, sizeof left));
    ASSERT_TRUE(eng_render_draw_uniforms(q, &cmd, right, sizeof right));
    uint32_t drawn = 0;
    ASSERT_TRUE(render_queue(&t, &drawn));
    EXPECT_EQ(drawn, (uint32_t)2);
    EXPECT_EQ(t.r.stats.pipeline_binds, (uint64_t)1);
    EXPECT_EQ(t.r.stats.uniform_pushes, (uint64_t)2);
    /* triangle réduit de moitié : sommets (cx, 0.25), (cx-0.25, -0.25), (cx+0.25, -0.25) ;
     * centre de gravité en NDC (cx, -0.0833) soit environ le pixel (cx*32+32, 34,7) */
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 16, 34, 85, 85, 85, 30)); /* gauche : mélange des 3 */
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 48, 34, 85, 85, 85, 30)); /* droite */
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 34, 0, 0, 0, 0));     /* entre les deux : vide */
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_3D(t);
}

UTEST(eng_render3d, a_perspective_mvp_projects_on_the_gpu_where_the_cpu_says)
{
    Test3d t;
    if (!open_3d(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngPipelineDesc desc = mvp_pipeline_desc(false, false, ENG_CULL_NONE);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    /* triangle en 3D, plan z = 0, sommets rouge / vert / bleu */
    const Vertex tri[3] = {
        {{0.0F, 1.0F, 0.0F}, {1.0F, 0.0F, 0.0F}},
        {{-1.0F, -1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}},
        {{1.0F, -1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    };
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, tri, sizeof tri);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));

    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 vp;
    mat4 mvp;
    glm_rotate_make(model, glm_rad(35.0F), ENG_AXIS_UP); /* triangle tourné : vraie perspective */
    eng_look_at((vec3){0.5F, 0.3F, 4.0F}, (vec3){0.0F, 0.0F, 0.0F}, ENG_AXIS_UP, view);
    eng_perspective(glm_rad(60.0F), 1.0F, 0.1F, 50.0F, proj);
    glm_mat4_mul(proj, view, vp);
    glm_mat4_mul(vp, model, mvp);

    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    const EngDrawCmd cmd = {.pipeline = pipe, .vertex_buffer = vb, .count = 3};
    ASSERT_TRUE(eng_render_draw_uniforms(q, &cmd, mvp, sizeof mvp));
    ASSERT_TRUE(render_queue(&t, NULL));

    /* Le centre de gravité 3D se projette (CPU, test_math.c) en un pixel ; l'interpolation étant
     * perspective-correcte, la couleur y est la moyenne des trois sommets : (85, 85, 85). Un pixel
     * proche du sommet haut (rouge) doit être rouge dominant. */
    vec3 centroid_local = {0.0F, -1.0F / 3.0F, 0.0F};
    vec3 ndc;
    ASSERT_TRUE(eng_project_point(mvp, centroid_local, ndc));
    uint32_t cx = 0;
    uint32_t cy = 0;
    ndc_to_pixel(ndc, &cx, &cy);
    ASSERT_TRUE(cx < SIZE && cy < SIZE);
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, cx, cy, 85, 85, 85, 30));

    vec3 near_top = {0.0F, 0.8F, 0.0F}; /* à 10 % du sommet rouge, vers le centre */
    ASSERT_TRUE(eng_project_point(mvp, near_top, ndc));
    uint32_t tx = 0;
    uint32_t ty = 0;
    ndc_to_pixel(ndc, &tx, &ty);
    const uint8_t *p = g_pixels + (((size_t)ty * SIZE) + tx) * 4;
    EXPECT_TRUE(p[0] > 180 && p[1] < 60 && p[2] < 60);
    /* un coin de l'écran reste vide */
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 0, 0, 0, 0, 0, 0));
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_3D(t);
}

UTEST(eng_render3d, uniforms_that_do_not_fit_the_queue_leave_the_draw_out)
{
    Test3d t;
    if (!open_3d(&t, false)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngPipelineDesc desc = mvp_pipeline_desc(false, false, ENG_CULL_BACK);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, TRIANGLE, sizeof TRIANGLE);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    /* commande écrite à la main avec des uniforms hors de la zone utilisée : refusée */
    const EngDrawCmd forged = {.pipeline = pipe,
                               .vertex_buffer = vb,
                               .count = 3,
                               .uniform_offset = 4096,
                               .uniform_size = 64};
    ASSERT_TRUE(eng_render_draw(q, &forged));
    /* offset valide mais taille qui déborde la zone utilisée */
    const EngDrawCmd forged_size = {.pipeline = pipe,
                                    .vertex_buffer = vb,
                                    .count = 3,
                                    .uniform_offset = 0,
                                    .uniform_size = 128};
    ASSERT_TRUE(eng_render_draw(q, &forged_size));
    /* uniforms sans tampon de commandes (exécution sans pass valide pour les uniforms) */
    mat4 mvp;
    glm_mat4_identity(mvp);
    const EngDrawCmd cmd = {.pipeline = pipe, .vertex_buffer = vb, .count = 3};
    ASSERT_TRUE(eng_render_draw_uniforms(q, &cmd, mvp, sizeof mvp));
    uint32_t drawn = 0;
    ASSERT_TRUE(render_queue(&t, &drawn));
    EXPECT_EQ(drawn, (uint32_t)1); /* seule la commande valide est dessinée */
    EXPECT_EQ(t.r.stats.rejected_commands, (uint64_t)2);
    EXPECT_EQ(t.r.stats.uniform_pushes, (uint64_t)1);
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 24, 187, 30, 38, TOL));
    /* begin_frame vide aussi la zone des uniforms */
    EXPECT_TRUE(t.r.queue.uniform_used > 0);
    (void)eng_renderer_begin_frame(&t.r);
    EXPECT_EQ(t.r.queue.uniform_used, (uint32_t)0);
    (void)eng_renderer_destroy_buffer(&t.r, vb);
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_3D(t);
}

/* ── Profondeur ── */

/* Grand triangle flat (une seule couleur) à la profondeur z, centré à l'écran. Le point (32, 40)
 * est dedans, quel que soit z (la matrice est l'identité). */
static void flat_triangle(Vertex out[3], float z, float r, float g, float b)
{
    const Vertex v[3] = {
        {{-0.8F, -0.8F, z}, {r, g, b}},
        {{0.8F, -0.8F, z}, {r, g, b}},
        {{0.0F, 0.8F, z}, {r, g, b}},
    };
    memcpy(out, v, sizeof v);
}

typedef struct DepthScene {
    EngBufferHandle near_green;
    EngBufferHandle far_red;
    EngPipelineHandle depth_on;  /* test + écriture */
    EngPipelineHandle depth_off; /* ni test ni écriture */
    EngPipelineHandle test_only; /* test sans écriture */
} DepthScene;

static bool make_depth_scene(Test3d *t, DepthScene *s)
{
    Vertex near_tri[3];
    Vertex far_tri[3];
    flat_triangle(near_tri, 0.2F, 0.0F, 1.0F, 0.0F);
    flat_triangle(far_tri, 0.8F, 1.0F, 0.0F, 0.0F);
    s->near_green = make_buffer(t, ENG_BUFFER_VERTEX, near_tri, sizeof near_tri);
    s->far_red = make_buffer(t, ENG_BUFFER_VERTEX, far_tri, sizeof far_tri);
    EngPipelineDesc d = mvp_pipeline_desc(true, true, ENG_CULL_NONE);
    s->depth_on = eng_renderer_create_pipeline(&t->r, &d);
    d = mvp_pipeline_desc(false, false, ENG_CULL_NONE);
    s->depth_off = eng_renderer_create_pipeline(&t->r, &d);
    d = mvp_pipeline_desc(true, false, ENG_CULL_NONE);
    s->test_only = eng_renderer_create_pipeline(&t->r, &d);
    return eng_renderer_pipeline_is_valid(&t->r, s->depth_on) &&
           eng_renderer_pipeline_is_valid(&t->r, s->depth_off) &&
           eng_renderer_pipeline_is_valid(&t->r, s->test_only);
}

static void destroy_depth_scene(Test3d *t, DepthScene *s)
{
    (void)eng_renderer_destroy_pipeline(&t->r, s->depth_on);
    (void)eng_renderer_destroy_pipeline(&t->r, s->depth_off);
    (void)eng_renderer_destroy_pipeline(&t->r, s->test_only);
    (void)eng_renderer_destroy_buffer(&t->r, s->near_green);
    (void)eng_renderer_destroy_buffer(&t->r, s->far_red);
}

/* Dessine les deux triangles dans l'ordre demandé avec le pipeline `pipe`, renvoie la couleur du
 * pixel (32, 40) : vert = le proche, rouge = le lointain. */
typedef enum Seen { SEEN_OTHER, SEEN_GREEN, SEEN_RED } Seen;

static Seen draw_pair(Test3d *t, const DepthScene *s, EngPipelineHandle pipe, bool near_first)
{
    mat4 identity;
    glm_mat4_identity(identity);
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    const EngBufferHandle first = near_first ? s->near_green : s->far_red;
    const EngBufferHandle second = near_first ? s->far_red : s->near_green;
    const EngDrawCmd a = {.pipeline = pipe, .vertex_buffer = first, .count = 3};
    const EngDrawCmd b = {.pipeline = pipe, .vertex_buffer = second, .count = 3};
    if (!eng_render_draw_uniforms(q, &a, identity, sizeof identity) ||
        !eng_render_draw_uniforms(q, &b, identity, sizeof identity) || !render_queue(t, NULL)) {
        return SEEN_OTHER;
    }
    if (pixel_is(g_pixels, SIZE, 32, 40, 0, 255, 0, 3)) {
        return SEEN_GREEN;
    }
    if (pixel_is(g_pixels, SIZE, 32, 40, 255, 0, 0, 3)) {
        return SEEN_RED;
    }
    return SEEN_OTHER;
}

UTEST(eng_render3d, the_depth_buffer_keeps_the_nearest_surface_whatever_the_drawing_order)
{
    Test3d t;
    if (!open_3d(&t, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EXPECT_TRUE(eng_renderer_depth_format(&t.r) != 0);
    DepthScene s;
    ASSERT_TRUE(make_depth_scene(&t, &s));
    EXPECT_EQ(draw_pair(&t, &s, s.depth_on, true), SEEN_GREEN);  /* proche puis lointain */
    EXPECT_EQ(draw_pair(&t, &s, s.depth_on, false), SEEN_GREEN); /* lointain puis proche */
    destroy_depth_scene(&t, &s);
    CLOSE_3D(t);
}

UTEST(eng_render3d, without_the_depth_test_the_last_drawn_surface_wins)
{
    Test3d t;
    if (!open_3d(&t, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    DepthScene s;
    ASSERT_TRUE(make_depth_scene(&t, &s));
    EXPECT_EQ(draw_pair(&t, &s, s.depth_off, true), SEEN_RED);    /* le lointain, dessiné après */
    EXPECT_EQ(draw_pair(&t, &s, s.depth_off, false), SEEN_GREEN); /* le proche, dessiné après */
    destroy_depth_scene(&t, &s);
    CLOSE_3D(t);
}

UTEST(eng_render3d, a_test_without_write_does_not_hide_what_comes_later)
{
    Test3d t;
    if (!open_3d(&t, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    DepthScene s;
    ASSERT_TRUE(make_depth_scene(&t, &s));
    /* le proche n'écrit pas la profondeur : le lointain passe le test contre le fond (1,0) */
    EXPECT_EQ(draw_pair(&t, &s, s.test_only, true), SEEN_RED);
    EXPECT_EQ(draw_pair(&t, &s, s.test_only, false), SEEN_GREEN);
    destroy_depth_scene(&t, &s);
    CLOSE_3D(t);
}

UTEST(eng_render3d, the_depth_buffer_is_cleared_every_frame)
{
    Test3d t;
    if (!open_3d(&t, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    DepthScene s;
    ASSERT_TRUE(make_depth_scene(&t, &s));
    /* si la profondeur de l'image précédente (proche, 0,2) restait, le lointain (0,8) serait
     * rejeté à l'image suivante */
    EXPECT_EQ(draw_pair(&t, &s, s.depth_on, true), SEEN_GREEN);
    mat4 identity;
    glm_mat4_identity(identity);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    const EngDrawCmd far_only = {.pipeline = s.depth_on, .vertex_buffer = s.far_red, .count = 3};
    ASSERT_TRUE(eng_render_draw_uniforms(q, &far_only, identity, sizeof identity));
    ASSERT_TRUE(render_queue(&t, NULL));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 40, 255, 0, 0, 3));
    destroy_depth_scene(&t, &s);
    CLOSE_3D(t);
}

UTEST(eng_render3d, the_clip_volume_depth_range_is_zero_to_one)
{
    Test3d t;
    if (!open_3d(&t, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngPipelineDesc desc = mvp_pipeline_desc(true, true, ENG_CULL_NONE);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &desc);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
    mat4 identity;
    glm_mat4_identity(identity);
    /* z = -0,2 : derrière le plan proche en [0,1] (visible avec la convention OpenGL [-1,1]) ;
     * z = 0,2 et 0,9 : dedans ; z = 1,2 : derrière le plan lointain */
    const struct {
        float z;
        bool visible;
    } cases[] = {{-0.2F, false}, {0.0F, true}, {0.2F, true}, {0.9F, true}, {1.2F, false}};
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        Vertex tri[3];
        flat_triangle(tri, cases[i].z, 0.0F, 1.0F, 0.0F);
        const EngBufferHandle vb = make_buffer(&t, ENG_BUFFER_VERTEX, tri, sizeof tri);
        EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
        const EngDrawCmd cmd = {.pipeline = pipe, .vertex_buffer = vb, .count = 3};
        ASSERT_TRUE(eng_render_draw_uniforms(q, &cmd, identity, sizeof identity));
        ASSERT_TRUE(render_queue(&t, NULL));
        EXPECT_EQ(pixel_is(g_pixels, SIZE, 32, 40, 0, 255, 0, 3), cases[i].visible);
        (void)eng_renderer_destroy_buffer(&t.r, vb);
    }
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    CLOSE_3D(t);
}

UTEST(eng_render3d, depth_requests_that_cannot_work_are_refused_without_leaking)
{
    Test3d t;
    if (!open_3d(&t, false)) { /* pas de depth buffer */
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EXPECT_EQ(eng_renderer_depth_format(&t.r), (uint32_t)0);
    EngPipelineDesc desc = mvp_pipeline_desc(true, true, ENG_CULL_BACK);
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &desc)));
    desc = mvp_pipeline_desc(true, false, ENG_CULL_BACK);
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &desc)));
    EXPECT_EQ(eng_pool_count(&t.r.pipelines), (uint32_t)0);
    CLOSE_3D(t);

    Test3d d;
    if (!open_3d(&d, true)) {
        return;
    }
    desc = mvp_pipeline_desc(false, true, ENG_CULL_BACK); /* écriture sans test : ambigu, refusé */
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&d.r, eng_renderer_create_pipeline(&d.r, &desc)));
    EXPECT_EQ(eng_pool_count(&d.r.pipelines), (uint32_t)0);
    desc = mvp_pipeline_desc(false, false, ENG_CULL_BACK); /* sans profondeur : accepté */
    const EngPipelineHandle ok = eng_renderer_create_pipeline(&d.r, &desc);
    EXPECT_TRUE(eng_renderer_pipeline_is_valid(&d.r, ok));
    (void)eng_renderer_destroy_pipeline(&d.r, ok);
    CLOSE_3D(d);
}

/* ── Le cube ── */

typedef struct Face {
    float n[3];
    float u[3];
    float v[3];
    float color[3];
} Face;

/* Même disposition que la scène de démonstration : ordre +X -X +Y -Y +Z -Z, donc la face -Z est
 * dessinée en dernier. */
static const Face FACES[6] = {
    {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}, {1, 0, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0}},
    {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}, {1, 1, 0}},
    {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {1, 0, 1}},  {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, {0, 1, 1}},
};

static void build_cube(Vertex vertices[24], uint16_t indices[36])
{
    static const float corner[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (int f = 0; f < 6; f++) {
        for (int c = 0; c < 4; c++) {
            for (int a = 0; a < 3; a++) {
                vertices[(f * 4) + c].position[a] =
                    0.5F * (FACES[f].n[a] + (corner[c][0] * FACES[f].u[a]) +
                            (corner[c][1] * FACES[f].v[a]));
                vertices[(f * 4) + c].color[a] = FACES[f].color[a];
            }
        }
        const uint16_t b = (uint16_t)(f * 4);
        const uint16_t quad[6] = {b, (uint16_t)(b + 1), (uint16_t)(b + 2),
                                  b, (uint16_t)(b + 2), (uint16_t)(b + 3)};
        memcpy(&indices[(size_t)f * 6U], quad, sizeof quad);
    }
}

typedef struct CubeScene {
    EngBufferHandle vertices;
    EngBufferHandle indices;
} CubeScene;

static bool make_cube(Test3d *t, CubeScene *c)
{
    Vertex vertices[24];
    uint16_t indices[36];
    build_cube(vertices, indices);
    c->vertices = make_buffer(t, ENG_BUFFER_VERTEX, vertices, sizeof vertices);
    c->indices = make_buffer(t, ENG_BUFFER_INDEX, indices, sizeof indices);
    return eng_renderer_buffer_is_valid(&t->r, c->vertices) &&
           eng_renderer_buffer_is_valid(&t->r, c->indices);
}

/* Dessine le cube tourné de (rx, ry) radians, vu de (0, 0, 3) vers l'origine ; renvoie false si
 * un dessin échoue. */
static bool draw_cube(Test3d *t, const CubeScene *c, EngPipelineHandle pipe, float rx, float ry)
{
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 vp;
    mat4 mvp;
    glm_rotate_make(model, ry, ENG_AXIS_UP);
    glm_rotate(model, rx, ENG_AXIS_RIGHT);
    eng_look_at((vec3){0.0F, 0.0F, 3.0F}, (vec3){0.0F, 0.0F, 0.0F}, ENG_AXIS_UP, view);
    eng_perspective(glm_rad(60.0F), 1.0F, 0.1F, 100.0F, proj);
    glm_mat4_mul(proj, view, vp);
    glm_mat4_mul(vp, model, mvp);
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    const EngDrawCmd cmd = {.pipeline = pipe,
                            .vertex_buffer = c->vertices,
                            .index_buffer = c->indices,
                            .index_type = ENG_INDEX_U16,
                            .count = 36};
    return eng_render_draw_uniforms(q, &cmd, mvp, sizeof mvp) && render_queue(t, NULL);
}

/* Face visible au centre de l'écran : le rayon passe par le centre du cube et y entre par la face
 * dont l'axe local est le plus aligné avec lui. Retourne l'indice dans FACES, ou -1 si deux axes
 * sont presque à égalité (cas ambigu, à éviter dans le test). */
static int face_at_center(float rx, float ry)
{
    mat4 model;
    glm_rotate_make(model, ry, ENG_AXIS_UP);
    glm_rotate(model, rx, ENG_AXIS_RIGHT);
    mat4 inverse;
    glm_mat4_inv(model, inverse);
    vec3 toward_camera = {0.0F, 0.0F, 1.0F}; /* du cube vers l'œil (axe du rayon central) */
    vec3 local;
    glm_mat4_mulv3(inverse, toward_camera, 0.0F, local);
    int best = 0;
    float best_abs = -1.0F;
    float second_abs = -1.0F;
    for (int a = 0; a < 3; a++) {
        const float v = fabsf(local[a]);
        if (v > best_abs) {
            second_abs = best_abs;
            best_abs = v;
            best = a;
        } else if (v > second_abs) {
            second_abs = v;
        }
    }
    if (best_abs - second_abs < 0.12F) {
        return -1;
    }
    const int sign = local[best] > 0.0F ? 0 : 1; /* +axe : face « + » (indice pair) */
    return (best * 2) + sign;
}

UTEST(eng_render3d, a_rotating_cube_always_shows_the_face_nearest_to_the_camera)
{
    Test3d t;
    if (!open_3d(&t, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    CubeScene c;
    ASSERT_TRUE(make_cube(&t, &c));
    /* Sans élimination des faces arrière : seule la profondeur décide. La face -Z, la plus
     * lointaine à l'angle 0, est dessinée en dernier : sans depth buffer elle gagnerait. */
    const EngPipelineDesc both_sides = mvp_pipeline_desc(true, true, ENG_CULL_NONE);
    const EngPipelineHandle no_cull = eng_renderer_create_pipeline(&t.r, &both_sides);
    const EngPipelineDesc culled = mvp_pipeline_desc(true, true, ENG_CULL_BACK);
    const EngPipelineHandle back_cull = eng_renderer_create_pipeline(&t.r, &culled);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, no_cull));
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, back_cull));

    int checked = 0;
    for (int i = 0; i < 24; i++) {
        const float rx = glm_rad(17.0F * (float)i);
        const float ry = glm_rad(29.0F * (float)i);
        const int face = face_at_center(rx, ry);
        if (face < 0) {
            continue;
        }
        const int r = (int)(FACES[face].color[0] * 255.0F);
        const int g = (int)(FACES[face].color[1] * 255.0F);
        const int b = (int)(FACES[face].color[2] * 255.0F);
        ASSERT_TRUE(draw_cube(&t, &c, no_cull, rx, ry));
        EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 32, r, g, b, 3));
        ASSERT_TRUE(draw_cube(&t, &c, back_cull, rx, ry));
        EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 32, r, g, b, 3)); /* même résultat avec culling */
        /* l'arrière-plan reste noir : le cube (±0,5 m à 3 m) n'atteint pas le coin */
        EXPECT_TRUE(pixel_is(g_pixels, SIZE, 1, 1, 0, 0, 0, 0));
        checked++;
    }
    EXPECT_TRUE(checked >= 10); /* assez d'angles non ambigus pour que le test ait un sens */
    (void)eng_renderer_destroy_pipeline(&t.r, no_cull);
    (void)eng_renderer_destroy_pipeline(&t.r, back_cull);
    (void)eng_renderer_destroy_buffer(&t.r, c.vertices);
    (void)eng_renderer_destroy_buffer(&t.r, c.indices);
    CLOSE_3D(t);
}

UTEST(eng_render3d, the_same_cube_without_depth_shows_the_wrong_face)
{
    Test3d t;
    if (!open_3d(&t, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    CubeScene c;
    ASSERT_TRUE(make_cube(&t, &c));
    const EngPipelineDesc no_depth = mvp_pipeline_desc(false, false, ENG_CULL_NONE);
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &no_depth);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
    ASSERT_TRUE(draw_cube(&t, &c, pipe, 0.0F, 0.0F));
    /* de face : +Z (magenta) est la bonne réponse, mais -Z (cyan, dessinée après) la recouvre */
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 32, 32, 0, 255, 255, 3));
    EXPECT_FALSE(pixel_is(g_pixels, SIZE, 32, 32, 255, 0, 255, 3));
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    (void)eng_renderer_destroy_buffer(&t.r, c.vertices);
    (void)eng_renderer_destroy_buffer(&t.r, c.indices);
    CLOSE_3D(t);
}

UTEST(eng_render3d, frames_with_depth_reach_the_swapchain_and_resizes_are_followed)
{
    Test3d t;
    if (!open_3d(&t, true)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    CubeScene c;
    ASSERT_TRUE(make_cube(&t, &c));
    EngPipelineDesc d = mvp_pipeline_desc(true, true, ENG_CULL_BACK);
    d.color_format = 0; /* swapchain */
    const EngPipelineHandle pipe = eng_renderer_create_pipeline(&t.r, &d);
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, pipe));
    const int sizes[][2] = {{64, 64}, {128, 96}, {50, 120}, {200, 80}, {64, 64}};
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        ASSERT_TRUE(SDL_SetWindowSize(t.f.platform.window, sizes[i][0], sizes[i][1]));
        (void)SDL_SyncWindow(t.f.platform.window);
        for (int frame = 0; frame < 3; frame++) {
            (void)eng_platform_pump(&t.f.platform);
            mat4 mvp;
            glm_mat4_identity(mvp);
            EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
            const EngDrawCmd cmd = {.pipeline = pipe,
                                    .vertex_buffer = c.vertices,
                                    .index_buffer = c.indices,
                                    .count = 36};
            ASSERT_TRUE(eng_render_draw_uniforms(q, &cmd, mvp, sizeof mvp));
            eng_renderer_end_frame(&t.r);
        }
        EXPECT_EQ((int)t.f.gpu.depth_width, sizes[i][0]);
        EXPECT_EQ((int)t.f.gpu.depth_height, sizes[i][1]);
    }
    EXPECT_EQ(t.f.gpu.frames_skipped, (uint64_t)0);
    EXPECT_EQ(t.r.stats.rejected_commands, (uint64_t)0);
    EXPECT_EQ(t.r.stats.draws, (uint64_t)(5 * 3));
    (void)eng_renderer_destroy_pipeline(&t.r, pipe);
    (void)eng_renderer_destroy_buffer(&t.r, c.vertices);
    (void)eng_renderer_destroy_buffer(&t.r, c.indices);
    CLOSE_3D(t);
}

#endif /* OW_SHADER_DIR */
