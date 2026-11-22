#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/grass.h"
#include "engine/model_gpu.h"
#include "test_render_util.h"
#include "utest.h"

/* Herbe animée (M3-09), pour de vrai : le vertex shader grass.vert doit donner, pixel pour pixel (à
 * l'arrondi 8 bits près), la même image qu'un maillage ordinaire dont CHAQUE sommet a été déplacé
 * par la référence CPU eng_grass_vertex_world (vent, fondu, rotation). Sans GPU ou sans shaders
 * compilés, ces tests s'arrêtent là. */

#ifdef OW_SHADER_DIR

#define SIZE 128U

typedef struct Fixture {
    RenderFixture f;
    EngArena permanent;
    EngArena level;
    EngRenderer r;
    EngPipelineHandle plain;
    EngPipelineHandle grass;
} Fixture;

static bool open_fixture(Fixture *t)
{
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "grass-gpu", (size_t)8 << 20) ||
        !eng_arena_init(&t->level, "grass-level", (size_t)8 << 20)) {
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
    eng_grass_pipeline_desc(&pipeline);
    pipeline.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    t->grass = eng_renderer_create_pipeline(&t->r, &pipeline);
    return true;
}

static uint32_t close_fixture(Fixture *t)
{
    (void)eng_renderer_destroy_pipeline(&t->r, t->grass);
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

static const float EYE[3] = {0.0F, 1.5F, 7.0F};

static void camera(mat4 view_proj, EngLighting *light)
{
    mat4 view;
    mat4 proj;
    glm_lookat((vec3){EYE[0], EYE[1], EYE[2]}, (vec3){0.0F, 1.2F, 0.0F}, (vec3){0.0F, 1.0F, 0.0F},
               view);
    glm_perspective(glm_rad(50.0F), 1.0F, 0.5F, 200.0F, proj);
    glm_mat4_mul(proj, view, view_proj);
    eng_lighting_default(light);
    (void)eng_lighting_set_sun_angles(light, 40.0F, 20.0F);
    light->camera_position[0] = EYE[0];
    light->camera_position[1] = EYE[1];
    light->camera_position[2] = EYE[2];
}

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

#define BLADES 6U

static void make_instances(EngInstance *out)
{
    for (uint32_t i = 0; i < BLADES; i++) {
        const float position[3] = {((float)i - 2.5F) * 0.9F, 0.0F, (float)(i % 3U) * -0.6F};
        eng_instance_make_yaw(&out[i], position, 1.6F + (0.15F * (float)i), 0.5F * (float)i, NULL);
    }
}

/* Dessine les lames avec le vertex shader, puis chaque lame séparément avec des sommets déplacés
 * par la référence CPU ; renvoie la différence maximale entre les deux images. */
static bool compare(Fixture *t, const EngGrassStyle *style, double time_seconds,
                    const float *camera_pos, int *worst, int *lit)
{
    EngModel blade;
    if (!eng_grass_blade_build(&t->level, 4, 0.35F, NULL, &blade)) {
        return false;
    }
    EngModelGpu gpu;
    if (!eng_model_gpu_create(&gpu, &t->r, &t->permanent, &blade, 0.0F)) {
        return false;
    }
    mat4 view_proj;
    EngLighting light;
    camera(view_proj, &light);
    EngInstance instances[BLADES];
    make_instances(instances);
    const EngBufferHandle buffer = eng_instances_upload(&t->r, instances, BLADES);
    if (!eng_renderer_buffer_is_valid(&t->r, buffer)) {
        return false;
    }
    float tail[ENG_GRASS_UNIFORM_FLOATS];
    if (!eng_grass_pack(style, camera_pos, time_seconds, tail)) {
        return false;
    }
    DrawContext c = {&t->r};
    mat4 identity;
    glm_mat4_identity(identity);
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    const EngModelDrawOptions options = {.instance_buffer = buffer,
                                         .instance_count = BLADES,
                                         .vertex_tail = tail,
                                         .vertex_tail_floats = ENG_GRASS_UNIFORM_FLOATS};
    if (eng_model_gpu_draw_ex(&gpu, q, t->grass, identity, view_proj, &light, NULL, &options) !=
            1U ||
        q->count != 1U ||
        !render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue, &c, SIZE,
                                g_pixels)) {
        return false;
    }
    *lit = lit_pixels(g_pixels);

    /* référence : un maillage déplacé par lame */
    q = eng_renderer_begin_frame(&t->r);
    EngModelGpu displaced[BLADES];
    for (uint32_t i = 0; i < BLADES; i++) {
        EngModel copy;
        const EngArenaMark mark = eng_arena_mark(&t->level);
        copy = blade;
        copy.vertices = ENG_ARENA_NEW_ARRAY(&t->level, EngModelVertex, blade.vertex_count);
        if (copy.vertices == NULL) {
            return false;
        }
        for (uint32_t v = 0; v < blade.vertex_count; v++) {
            copy.vertices[v] = blade.vertices[v];
            float world[3];
            eng_grass_vertex_world(style, camera_pos, time_seconds, &instances[i],
                                   blade.vertices[v].position, world);
            for (int k = 0; k < 3; k++) {
                copy.vertices[v].position[k] = world[k];
            }
            /* normale et tangente tournées par le cap de l'instance (rotation autour de Y) */
            const float qy = instances[i].rotation[1];
            const float qw = instances[i].rotation[3];
            const float cs = (qw * qw) - (qy * qy);
            const float sn = 2.0F * qw * qy;
            const float nx = blade.vertices[v].normal[0];
            const float nz = blade.vertices[v].normal[2];
            copy.vertices[v].normal[0] = (cs * nx) + (sn * nz);
            copy.vertices[v].normal[2] = (-sn * nx) + (cs * nz);
            const float tx = blade.vertices[v].tangent[0];
            const float tz = blade.vertices[v].tangent[2];
            copy.vertices[v].tangent[0] = (cs * tx) + (sn * tz);
            copy.vertices[v].tangent[2] = (-sn * tx) + (cs * tz);
        }
        if (!eng_model_gpu_create(&displaced[i], &t->r, &t->permanent, &copy, 0.0F)) {
            return false;
        }
        if (eng_model_gpu_draw(&displaced[i], q, t->plain, identity, view_proj, &light) != 1U) {
            return false;
        }
        eng_arena_reset_to(&t->level, mark);
    }
    const bool ok = render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue,
                                           &c, SIZE, g_other);
    *worst = worst_difference(g_pixels, g_other);
    for (uint32_t i = 0; i < BLADES; i++) {
        eng_model_gpu_destroy(&displaced[i], &t->r);
    }
    (void)eng_renderer_destroy_buffer(&t->r, buffer);
    eng_model_gpu_destroy(&gpu, &t->r);
    return ok;
}

UTEST(eng_grass_gpu, wind_matches_the_cpu_reference_pixel_for_pixel)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngGrassStyle style = eng_grass_style_default();
    style.wind_direction[0] = 1.0F;
    style.wind_direction[1] = 0.6F;
    style.wind_strength = 0.7F;
    style.wind_frequency = 0.5F;
    style.wave_scale = 0.9F;
    int worst = 0;
    int lit = 0;
    ASSERT_TRUE(compare(&t, &style, 0.37, EYE, &worst, &lit));
    dump_ppm(g_pixels, "grass_wind");
    EXPECT_GT(lit, 300); /* des brins à l'écran */
    EXPECT_LE(worst, 3);
    /* le vent se voit : l'image diffère de celle sans vent */
    uint8_t still[SIZE * SIZE * 4];
    EngGrassStyle calm = style;
    calm.wind_strength = 0.0F;
    memcpy(still, g_pixels, sizeof still);
    ASSERT_TRUE(compare(&t, &calm, 0.37, EYE, &worst, &lit));
    EXPECT_LE(worst, 3);
    EXPECT_GT(worst_difference(still, g_pixels), 40);
    CLOSE_FIXTURE(t);
}

UTEST(eng_grass_gpu, fading_blades_match_the_cpu_reference_and_vanish_beyond_fade_end)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngGrassStyle style = eng_grass_style_default();
    style.wind_strength = 0.3F;
    style.fade_start = 4.0F;
    style.fade_end = 9.0F; /* la caméra est à ~7 m des lames : fondu partiel */
    int worst = 0;
    int lit_fading = 0;
    ASSERT_TRUE(compare(&t, &style, 1.1, EYE, &worst, &lit_fading));
    dump_ppm(g_pixels, "grass_fade");
    EXPECT_LE(worst, 3);
    int lit_full = 0;
    style.fade_start = 50.0F;
    style.fade_end = 90.0F;
    ASSERT_TRUE(compare(&t, &style, 1.1, EYE, &worst, &lit_full));
    EXPECT_LE(worst, 3);
    EXPECT_GT(lit_full, 300);
    EXPECT_LT(lit_fading, lit_full); /* des brins plus petits couvrent moins de pixels */
    EXPECT_GT(lit_fading, 0);
    /* au-delà de fade_end : plus rien */
    style.fade_start = 1.0F;
    style.fade_end = 2.0F;
    int lit_gone = 99;
    ASSERT_TRUE(compare(&t, &style, 1.1, EYE, &worst, &lit_gone));
    EXPECT_EQ(lit_gone, 0);
    CLOSE_FIXTURE(t);
}

#define FIELD 20000U

/* Un champ dense : 20 000 brins en UN dessin ; les statistiques du renderer le prouvent. Le temps
 * est ÉCRIT mais pas exigé (SwiftShader est un rasteriseur logiciel). */
UTEST(eng_grass_gpu, a_dense_field_is_one_draw)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngModel blade;
    ASSERT_TRUE(eng_grass_blade_build(&t.level, 3, 0.12F, NULL, &blade));
    EngModelGpu gpu;
    ASSERT_TRUE(eng_model_gpu_create(&gpu, &t.r, &t.permanent, &blade, 0.0F));
    EngInstance *field = eng_arena_alloc(&t.level, (size_t)FIELD * sizeof(EngInstance), 16);
    ASSERT_TRUE(field != NULL);
    uint32_t seed = 99U;
    for (uint32_t i = 0; i < FIELD; i++) {
        seed = (seed * 1664525U) + 1013904223U;
        const float x = ((float)(seed >> 8) / 16777216.0F - 0.5F) * 30.0F;
        seed = (seed * 1664525U) + 1013904223U;
        const float z = ((float)(seed >> 8) / 16777216.0F - 0.5F) * 30.0F;
        seed = (seed * 1664525U) + 1013904223U;
        const float scale = 0.5F + (((float)(seed >> 8) / 16777216.0F) * 0.6F);
        const float position[3] = {x, 0.0F, z};
        eng_instance_make_yaw(&field[i], position, scale, (float)(seed >> 16) * 1e-4F, NULL);
    }
    const EngBufferHandle buffer = eng_instances_upload(&t.r, field, FIELD);
    ASSERT_TRUE(eng_renderer_buffer_is_valid(&t.r, buffer));
    mat4 view_proj;
    EngLighting light;
    camera(view_proj, &light);
    const EngGrassStyle style = eng_grass_style_default();
    float tail[ENG_GRASS_UNIFORM_FLOATS];
    ASSERT_TRUE(eng_grass_pack(&style, EYE, 3.0, tail));
    DrawContext c = {&t.r};
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    mat4 identity;
    glm_mat4_identity(identity);
    const EngModelDrawOptions options = {.instance_buffer = buffer,
                                         .instance_count = FIELD,
                                         .vertex_tail = tail,
                                         .vertex_tail_floats = ENG_GRASS_UNIFORM_FLOATS};
    EXPECT_EQ(eng_model_gpu_draw_ex(&gpu, q, t.grass, identity, view_proj, &light, NULL, &options),
              (uint32_t)1);
    EXPECT_EQ(q->count, 1U);
    const EngRendererStats before = t.r.stats;
    const uint64_t start = SDL_GetPerformanceCounter();
    ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                       SIZE, g_pixels));
    const double ms = (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
                      (double)SDL_GetPerformanceFrequency();
    EXPECT_EQ(t.r.stats.draws - before.draws, (uint64_t)1);
    EXPECT_EQ(t.r.stats.instances - before.instances, (uint64_t)FIELD);
    EXPECT_EQ(t.r.stats.rejected_commands - before.rejected_commands, (uint64_t)0);
    dump_ppm(g_pixels, "grass_field");
    EXPECT_GT(lit_pixels(g_pixels), 1500);
    (void)fprintf(stderr,
                  "herbe : %u brins (%u triangles chacun), 1 dessin, %.0f ms sur SwiftShader "
                  "(logiciel)\n",
                  FIELD, eng_grass_blade_triangle_count(3), ms);
    (void)eng_renderer_destroy_buffer(&t.r, buffer);
    eng_model_gpu_destroy(&gpu, &t.r);
    CLOSE_FIXTURE(t);
}

/* Un bloc de uniforms de vertex trop grand est borné, et sans lui le dessin reste valide. */
UTEST(eng_grass_gpu, an_oversized_or_missing_vertex_tail_is_harmless)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngModel blade;
    ASSERT_TRUE(eng_grass_blade_build(&t.level, 3, 0.3F, NULL, &blade));
    EngModelGpu gpu;
    ASSERT_TRUE(eng_model_gpu_create(&gpu, &t.r, &t.permanent, &blade, 0.0F));
    EngInstance one;
    const float position[3] = {0.0F, 0.0F, 0.0F};
    eng_instance_make_yaw(&one, position, 2.0F, 0.0F, NULL);
    const EngBufferHandle buffer = eng_instances_upload(&t.r, &one, 1);
    mat4 view_proj;
    EngLighting light;
    camera(view_proj, &light);
    float big[ENG_MODEL_MAX_VERTEX_TAIL_FLOATS + 64U] = {0};
    const EngGrassStyle style = eng_grass_style_default();
    ASSERT_TRUE(eng_grass_pack(&style, EYE, 0.0, big));
    mat4 identity;
    glm_mat4_identity(identity);
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    const EngModelDrawOptions options = {.instance_buffer = buffer,
                                         .instance_count = 1,
                                         .vertex_tail = big,
                                         .vertex_tail_floats =
                                             ENG_MODEL_MAX_VERTEX_TAIL_FLOATS + 64U};
    EXPECT_EQ(eng_model_gpu_draw_ex(&gpu, q, t.grass, identity, view_proj, &light, NULL, &options),
              (uint32_t)1);
    /* queue sur un dessin non instancié : ignorée */
    q = eng_renderer_begin_frame(&t.r);
    const EngModelDrawOptions plain_options = {.vertex_tail = big, .vertex_tail_floats = 12};
    EXPECT_EQ(
        eng_model_gpu_draw_ex(&gpu, q, t.plain, identity, view_proj, &light, NULL, &plain_options),
        (uint32_t)1);
    (void)eng_renderer_destroy_buffer(&t.r, buffer);
    eng_model_gpu_destroy(&gpu, &t.r);
    CLOSE_FIXTURE(t);
}

#endif /* OW_SHADER_DIR */
