#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/instancing.h"
#include "engine/model_gpu.h"
#include "engine/rock.h"
#include "test_render_util.h"
#include "utest.h"

/* Instancing GPU (M3-07), pour de vrai : N instances d'un maillage dessinées en UN dessin doivent
 * donner, pixel pour pixel (à l'arrondi 8 bits près), la même image que N dessins ordinaires du
 * même maillage avec la matrice de chaque instance. Sans GPU ou sans shaders compilés, ces tests
 * s'arrêtent là. */

#ifdef OW_SHADER_DIR

#define SIZE 128U

typedef struct Fixture {
    RenderFixture f;
    EngArena permanent;
    EngArena level;
    EngRenderer r;
    EngPipelineHandle plain;
    EngPipelineHandle instanced;
} Fixture;

static bool open_fixture(Fixture *t)
{
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "instancing-gpu", (size_t)8 << 20) ||
        !eng_arena_init(&t->level, "instancing-level", (size_t)8 << 20)) {
        close_render_fixture(&t->f);
        return false;
    }
    const EngRendererDesc desc = {.offline_dir = OW_SHADER_DIR,
                                  .max_commands = 64,
                                  .max_uniform_bytes = 16384,
                                  .depth_buffer = true};
    if (!eng_renderer_init(&t->r, &t->f.gpu, &t->permanent, &t->f.scratch, &desc)) {
        eng_arena_shutdown(&t->level);
        eng_arena_shutdown(&t->permanent);
        close_render_fixture(&t->f);
        return false;
    }
    EngPipelineDesc pipeline;
    eng_model_pipeline_desc(&pipeline);
    pipeline.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    t->plain = eng_renderer_create_pipeline(&t->r, &pipeline);
    eng_instanced_pipeline_desc(&pipeline);
    pipeline.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    t->instanced = eng_renderer_create_pipeline(&t->r, &pipeline);
    return true;
}

static uint32_t close_fixture(Fixture *t)
{
    (void)eng_renderer_destroy_pipeline(&t->r, t->instanced);
    (void)eng_renderer_destroy_pipeline(&t->r, t->plain);
    const uint32_t leaks = eng_renderer_shutdown(&t->r);
    eng_arena_shutdown(&t->level);
    eng_arena_shutdown(&t->permanent);
    close_render_fixture(&t->f);
    return leaks;
}

#define CLOSE_FIXTURE(t) EXPECT_EQ(close_fixture(&(t)), (uint32_t)0)

typedef struct DrawContext {
    EngRenderer *renderer;
} DrawContext;

static void execute_queue(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    DrawContext *c = (DrawContext *)context;
    (void)eng_renderer_execute(c->renderer, &c->renderer->queue, commands, pass);
}

static uint8_t g_pixels[SIZE * SIZE * 4];
static uint8_t g_other[SIZE * SIZE * 4];

static void camera(float eye_y, float eye_z, float far_plane, mat4 view_proj, EngLighting *light)
{
    mat4 view;
    mat4 proj;
    glm_lookat((vec3){0.0F, eye_y, eye_z}, (vec3){0.0F, 0.0F, 0.0F}, (vec3){0.0F, 1.0F, 0.0F},
               view);
    glm_perspective(glm_rad(50.0F), 1.0F, 0.5F, far_plane, proj);
    glm_mat4_mul(proj, view, view_proj);
    eng_lighting_default(light);
    (void)eng_lighting_set_sun_angles(light, 40.0F, 50.0F);
    light->camera_position[0] = 0.0F;
    light->camera_position[1] = eye_y;
    light->camera_position[2] = eye_z;
}

static bool make_rock(Fixture *t, EngModelGpu *gpu, const EngModelMaterial *material)
{
    EngRockParams p = eng_rock_default_params();
    p.subdivisions = 2;
    p.material = material;
    EngModel model;
    if (!eng_rock_build(&t->level, &p, &model)) {
        return false;
    }
    return eng_model_gpu_create(gpu, &t->r, &t->permanent, &model, 0.0F);
}

/* OW_INSTANCING_DUMP=<dossier> : écrit l'image en PPM pour l'œil */
static void dump_ppm(const uint8_t *pixels, const char *name)
{
    const char *dir = SDL_getenv("OW_INSTANCING_DUMP");
    if (dir == NULL) {
        return;
    }
    char path[512];
    SDL_snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return;
    }
    (void)fprintf(f, "P6\n%u %u\n255\n", SIZE, SIZE);
    for (uint32_t i = 0; i < SIZE * SIZE; i++) {
        (void)fwrite(&pixels[(size_t)i * 4U], 1, 3, f);
    }
    (void)fclose(f);
}

static int lit_pixels(const uint8_t *pixels)
{
    int count = 0;
    for (size_t i = 0; i < (size_t)SIZE * SIZE; i++) {
        count += (pixels[i * 4] + pixels[(i * 4) + 1] + pixels[(i * 4) + 2]) > 6 ? 1 : 0;
    }
    return count;
}

static int worst_difference(const uint8_t *a, const uint8_t *b)
{
    int worst = 0;
    for (size_t i = 0; i < (size_t)SIZE * SIZE * 4U; i++) {
        const int d = abs((int)a[i] - (int)b[i]);
        worst = d > worst ? d : worst;
    }
    return worst;
}

#define GRID 9U

static void grid_instances(EngInstance *out, const uint8_t *tint)
{
    for (uint32_t i = 0; i < GRID; i++) {
        const float position[3] = {((float)(i % 3U) - 1.0F) * 3.2F, 0.0F,
                                   (floorf((float)i / 3.0F) - 1.0F) * 3.2F};
        eng_instance_make_yaw(&out[i], position, 0.7F + (0.12F * (float)i), 0.55F * (float)i, tint);
    }
}

/* N instances en UN dessin == N dessins ordinaires (la matrice de chaque instance). */
UTEST(eng_instancing_gpu, nine_instances_in_one_draw_equal_nine_plain_draws)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngModelGpu rock;
    ASSERT_TRUE(make_rock(&t, &rock, NULL));
    mat4 view_proj;
    EngLighting light;
    camera(7.0F, 11.0F, 100.0F, view_proj, &light);
    EngInstance instances[GRID];
    grid_instances(instances, NULL);
    DrawContext c = {&t.r};

    const EngBufferHandle buffer = eng_instances_upload(&t.r, instances, GRID);
    ASSERT_TRUE(eng_renderer_buffer_is_valid(&t.r, buffer));
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    const EngModelDrawOptions options = {.instance_buffer = buffer, .instance_count = GRID};
    mat4 identity;
    glm_mat4_identity(identity);
    EXPECT_EQ(
        eng_model_gpu_draw_ex(&rock, q, t.instanced, identity, view_proj, &light, NULL, &options),
        (uint32_t)1);
    EXPECT_EQ(q->count, 1U); /* un seul dessin pour les neuf copies */
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_pixels));
    const int lit = lit_pixels(g_pixels);
    dump_ppm(g_pixels, "instances_9");

    q = eng_renderer_begin_frame(&t.r);
    for (uint32_t i = 0; i < GRID; i++) {
        mat4 m;
        eng_instance_matrix(&instances[i], m);
        EXPECT_EQ(eng_model_gpu_draw(&rock, q, t.plain, m, view_proj, &light), (uint32_t)1);
    }
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_other));
    EXPECT_GT(lit, 1500); /* ce n'est pas une image vide : neuf rochers à l'écran */
    EXPECT_EQ(lit_pixels(g_other), lit);
    EXPECT_LE(worst_difference(g_pixels, g_other), 2);
    (void)eng_renderer_destroy_buffer(&t.r, buffer);
    eng_model_gpu_destroy(&rock, &t.r);
    CLOSE_FIXTURE(t);
}

/* La teinte d'instance multiplie la couleur de base : une instance de teinte (t / 255) donne le
 * rendu d'un maillage ordinaire dont le matériau porte ce facteur. */
UTEST(eng_instancing_gpu, the_instance_tint_multiplies_the_material_colour)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    const uint8_t tint[4] = {255, 128, 64, 255};
    EngModelMaterial tinted = eng_rock_default_material();
    for (int c = 0; c < 3; c++) {
        tinted.base_color[c] *= (float)tint[c] / 255.0F;
    }
    EngModelGpu rock;
    EngModelGpu rock_tinted;
    ASSERT_TRUE(make_rock(&t, &rock, NULL));
    ASSERT_TRUE(make_rock(&t, &rock_tinted, &tinted));
    mat4 view_proj;
    EngLighting light;
    camera(7.0F, 11.0F, 100.0F, view_proj, &light);
    EngInstance instances[GRID];
    grid_instances(instances, tint);
    DrawContext c = {&t.r};
    const EngBufferHandle buffer = eng_instances_upload(&t.r, instances, GRID);
    ASSERT_TRUE(eng_renderer_buffer_is_valid(&t.r, buffer));
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    const EngModelDrawOptions options = {.instance_buffer = buffer, .instance_count = GRID};
    mat4 identity;
    glm_mat4_identity(identity);
    EXPECT_EQ(
        eng_model_gpu_draw_ex(&rock, q, t.instanced, identity, view_proj, &light, NULL, &options),
        (uint32_t)1);
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_pixels));
    q = eng_renderer_begin_frame(&t.r);
    for (uint32_t i = 0; i < GRID; i++) {
        mat4 m;
        eng_instance_matrix(&instances[i], m);
        EXPECT_EQ(eng_model_gpu_draw(&rock_tinted, q, t.plain, m, view_proj, &light), (uint32_t)1);
    }
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_other));
    EXPECT_LE(worst_difference(g_pixels, g_other), 2);
    /* la teinte se voit : le rouge domine le bleu sur la pierre éclairée (gris -> orangé) */
    long red = 0;
    long blue = 0;
    for (size_t i = 0; i < (size_t)SIZE * SIZE; i++) {
        red += g_pixels[i * 4];
        blue += g_pixels[(i * 4) + 2];
    }
    EXPECT_GT(red, blue * 2);
    (void)eng_renderer_destroy_buffer(&t.r, buffer);
    eng_model_gpu_destroy(&rock_tinted, &t.r);
    eng_model_gpu_destroy(&rock, &t.r);
    CLOSE_FIXTURE(t);
}

#define BIG 50000U

/* Le critère de M3-07 : 50 000 rochers en UN dessin. Les statistiques du renderer le prouvent (un
 * dessin, 50 000 instances) ; l'image n'est pas vide. Le temps d'image est ÉCRIT mais pas exigé :
 * SwiftShader est un rasteriseur logiciel, son temps ne dit rien d'un vrai GPU. */
UTEST(eng_instancing_gpu, fifty_thousand_rocks_are_one_draw)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngRockParams rp = eng_rock_default_params();
    rp.subdivisions = 1; /* 80 triangles : 4 millions de triangles au total */
    EngModel model;
    ASSERT_TRUE(eng_rock_build(&t.level, &rp, &model));
    EngModelGpu rock;
    ASSERT_TRUE(eng_model_gpu_create(&rock, &t.r, &t.permanent, &model, 0.0F));
    EngInstance *instances = eng_arena_alloc(&t.level, (size_t)BIG * sizeof(EngInstance), 16);
    ASSERT_TRUE(instances != NULL);
    uint32_t seed = 12345U;
    for (uint32_t i = 0; i < BIG; i++) {
        seed = (seed * 1664525U) + 1013904223U;
        const float x = ((float)(seed >> 8) / 16777216.0F - 0.5F) * 200.0F;
        seed = (seed * 1664525U) + 1013904223U;
        const float z = ((float)(seed >> 8) / 16777216.0F - 0.5F) * 200.0F;
        seed = (seed * 1664525U) + 1013904223U;
        const float scale = 0.4F + (((float)(seed >> 8) / 16777216.0F) * 0.8F);
        const float position[3] = {x, 0.0F, z};
        eng_instance_make_yaw(&instances[i], position, scale, (float)(seed >> 16) * 1e-4F, NULL);
    }
    mat4 view_proj;
    EngLighting light;
    camera(60.0F, 120.0F, 600.0F, view_proj, &light);
    const EngBufferHandle buffer = eng_instances_upload(&t.r, instances, BIG);
    ASSERT_TRUE(eng_renderer_buffer_is_valid(&t.r, buffer));
    DrawContext c = {&t.r};
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    const EngModelDrawOptions options = {.instance_buffer = buffer, .instance_count = BIG};
    mat4 identity;
    glm_mat4_identity(identity);
    EXPECT_EQ(
        eng_model_gpu_draw_ex(&rock, q, t.instanced, identity, view_proj, &light, NULL, &options),
        (uint32_t)1);
    EXPECT_EQ(q->count, 1U);
    const EngRendererStats before = t.r.stats;
    const uint64_t start = SDL_GetPerformanceCounter();
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_pixels));
    const double ms = (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
                      (double)SDL_GetPerformanceFrequency();
    EXPECT_EQ(t.r.stats.draws - before.draws, (uint64_t)1);
    EXPECT_EQ(t.r.stats.instances - before.instances, (uint64_t)BIG);
    EXPECT_EQ(t.r.stats.rejected_commands - before.rejected_commands, (uint64_t)0);
    dump_ppm(g_pixels, "instances_50000");
    EXPECT_GT(lit_pixels(g_pixels), 4000); /* un champ de rochers couvre le sol visible */
    (void)fprintf(stderr,
                  "instancing : %u rochers (%u triangles chacun), 1 dessin, %.0f ms sur "
                  "SwiftShader (logiciel)\n",
                  BIG, eng_rock_triangle_count(1), ms);
    (void)eng_renderer_destroy_buffer(&t.r, buffer);
    eng_model_gpu_destroy(&rock, &t.r);
    CLOSE_FIXTURE(t);
}

/* Dessins incohérents : rejetés par le renderer, rien n'est lu hors du buffer d'instances. */
UTEST(eng_instancing_gpu, inconsistent_instance_draws_are_rejected)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngModelGpu rock;
    ASSERT_TRUE(make_rock(&t, &rock, NULL));
    mat4 view_proj;
    EngLighting light;
    camera(7.0F, 11.0F, 100.0F, view_proj, &light);
    EngInstance instances[4];
    for (uint32_t i = 0; i < 4; i++) {
        const float position[3] = {(float)i, 0.0F, 0.0F};
        eng_instance_make_yaw(&instances[i], position, 1.0F, 0.0F, NULL);
    }
    const EngBufferHandle buffer = eng_instances_upload(&t.r, instances, 4);
    ASSERT_TRUE(eng_renderer_buffer_is_valid(&t.r, buffer));
    DrawContext c = {&t.r};
    mat4 identity;
    glm_mat4_identity(identity);
    /* 1 : plus d'instances que le buffer n'en contient */
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    EngModelDrawOptions options = {.instance_buffer = buffer, .instance_count = 5};
    (void)eng_model_gpu_draw_ex(&rock, q, t.instanced, identity, view_proj, &light, NULL, &options);
    uint64_t before = t.r.stats.rejected_commands;
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_pixels));
    EXPECT_EQ(t.r.stats.rejected_commands - before, (uint64_t)1);
    EXPECT_EQ(lit_pixels(g_pixels), 0);
    /* 2 : exactement la capacité : accepté */
    q = eng_renderer_begin_frame(&t.r);
    options.instance_count = 4;
    (void)eng_model_gpu_draw_ex(&rock, q, t.instanced, identity, view_proj, &light, NULL, &options);
    before = t.r.stats.rejected_commands;
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_pixels));
    EXPECT_EQ(t.r.stats.rejected_commands - before, (uint64_t)0);
    EXPECT_GT(lit_pixels(g_pixels), 100);
    /* 3 : pipeline instancié sans buffer d'instances */
    q = eng_renderer_begin_frame(&t.r);
    (void)eng_model_gpu_draw_ex(&rock, q, t.instanced, identity, view_proj, &light, NULL, NULL);
    before = t.r.stats.rejected_commands;
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_pixels));
    EXPECT_EQ(t.r.stats.rejected_commands - before, (uint64_t)1);
    /* 4 : instance_count nul avec un buffer */
    q = eng_renderer_begin_frame(&t.r);
    options.instance_count = 0;
    (void)eng_model_gpu_draw_ex(&rock, q, t.instanced, identity, view_proj, &light, NULL, &options);
    before = t.r.stats.rejected_commands;
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_pixels));
    EXPECT_EQ(t.r.stats.rejected_commands - before, (uint64_t)1);
    /* 5 : buffer d'instances sur un pipeline qui n'en a pas */
    q = eng_renderer_begin_frame(&t.r);
    options.instance_count = 4;
    (void)eng_model_gpu_draw_ex(&rock, q, t.plain, identity, view_proj, &light, NULL, &options);
    before = t.r.stats.rejected_commands;
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_pixels));
    EXPECT_EQ(t.r.stats.rejected_commands - before, (uint64_t)1);
    /* 6 : buffer d'instances détruit (périmé) */
    const EngBufferHandle stale = buffer;
    (void)eng_renderer_destroy_buffer(&t.r, buffer);
    q = eng_renderer_begin_frame(&t.r);
    options.instance_buffer = stale;
    (void)eng_model_gpu_draw_ex(&rock, q, t.instanced, identity, view_proj, &light, NULL, &options);
    before = t.r.stats.rejected_commands;
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_pixels));
    EXPECT_EQ(t.r.stats.rejected_commands - before, (uint64_t)1);
    eng_model_gpu_destroy(&rock, &t.r);
    CLOSE_FIXTURE(t);
}

/* Descriptions de pipeline et d'instances incohérentes : refusées à la création. */
UTEST(eng_instancing_gpu, invalid_instance_streams_and_instances_are_refused)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngPipelineDesc base;
    eng_instanced_pipeline_desc(&base);
    base.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    EngPipelineDesc d = base;
    d.instance_attributes[1].location = 4; /* deux attributs d'instance à la même place */
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &d)));
    d = base;
    d.instance_attributes[0].location = 2; /* celle d'un attribut de sommet */
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &d)));
    d = base;
    d.instance_attributes[2].offset = 36; /* hors de l'instance */
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &d)));
    d = base;
    d.instance_attribute_count = 0; /* un stride sans attribut */
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &d)));
    d = base;
    d.instance_stride = 0; /* des attributs sans stride */
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &d)));
    d = base;
    d.instance_attributes[1].format = ENG_VERTEX_NONE;
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &d)));
    d = base;
    d.attribute_count = 0; /* flux d'instances sans flux de sommets */
    d.vertex_stride = 0;
    EXPECT_FALSE(eng_renderer_pipeline_is_valid(&t.r, eng_renderer_create_pipeline(&t.r, &d)));

    /* instances refusées à l'envoi */
    EngInstance one;
    const float origin[3] = {0, 0, 0};
    eng_instance_make_yaw(&one, origin, 1.0F, 0.0F, NULL);
    EXPECT_FALSE(eng_renderer_buffer_is_valid(&t.r, eng_instances_upload(&t.r, &one, 0)));
    EXPECT_FALSE(eng_renderer_buffer_is_valid(&t.r, eng_instances_upload(&t.r, NULL, 1)));
    /* un nombre dont la taille en octets déborderait uint32 : refusé avant toute lecture */
    EXPECT_FALSE(eng_renderer_buffer_is_valid(&t.r, eng_instances_upload(&t.r, &one, UINT32_MAX)));
    EngInstance two[2] = {one, one};
    two[1].scale = 0.0F;
    EXPECT_FALSE(eng_renderer_buffer_is_valid(&t.r, eng_instances_upload(&t.r, two, 2)));
    two[1] = one;
    two[1].rotation[3] = 2.0F;
    EXPECT_FALSE(eng_renderer_buffer_is_valid(&t.r, eng_instances_upload(&t.r, two, 2)));
    CLOSE_FIXTURE(t);
}

#endif
