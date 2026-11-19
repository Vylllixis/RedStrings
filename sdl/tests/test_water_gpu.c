#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/atmosphere.h"
#include "engine/fog.h"
#include "engine/model_gpu.h"
#include "engine/water.h"
#include "test_render_util.h"
#include "utest.h"

/* Eau (M3-10), pour de vrai : water.frag doit donner, pixel pour pixel (à l'arrondi 8 bits près),
 * la couleur de eng_water_shade (puis de eng_fog_apply avec brouillard) au point du plan que voit
 * le pixel. Sans GPU ou sans shaders compilés, ces tests s'arrêtent là. */

#ifdef OW_SHADER_DIR

#define SIZE 128U
#define FIELD 33U

typedef struct Fixture {
    RenderFixture f;
    EngArena permanent;
    EngArena level;
    EngRenderer r;
    EngPipelineHandle water;
} Fixture;

static bool open_fixture(Fixture *t)
{
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "water-gpu", (size_t)8 << 20) ||
        !eng_arena_init(&t->level, "water-level", (size_t)8 << 20)) {
        close_render_fixture(&t->f);
        return false;
    }
    const EngRendererDesc desc = {.offline_dir = OW_SHADER_DIR,
                                  .max_commands = 16,
                                  .max_uniform_bytes = 16384,
                                  .depth_buffer = true};
    if (!eng_renderer_init(&t->r, &t->f.gpu, &t->permanent, &t->f.scratch, &desc)) {
        eng_arena_shutdown(&t->level);
        eng_arena_shutdown(&t->permanent);
        close_render_fixture(&t->f);
        return false;
    }
    EngPipelineDesc pipeline;
    eng_water_pipeline_desc(&pipeline);
    pipeline.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    t->water = eng_renderer_create_pipeline(&t->r, &pipeline);
    return true;
}

static uint32_t close_fixture(Fixture *t)
{
    (void)eng_renderer_destroy_pipeline(&t->r, t->water);
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
static uint16_t g_samples[FIELD * FIELD];
static EngHeightmap g_map;

static const float EYE[3] = {0.0F, 9.0F, 18.0F};

/* Lac de 32 m ; profondeur 1 + 0,1 x (x local) : 1 m à l'ouest, 4,2 m à l'est ; plan à l'altitude
 * 0. Un plan incliné : l'interpolation des profondeurs aux sommets est exacte. */
static void make_lake(void)
{
    for (uint32_t z = 0; z < FIELD; z++) {
        for (uint32_t x = 0; x < FIELD; x++) {
            const float altitude = -(1.0F + (0.1F * (float)x));
            g_samples[((size_t)z * FIELD) + x] =
                (uint16_t)(((altitude + 10.0F) / 10.0F) * 65535.0F + 0.5F);
        }
    }
    g_map = (EngHeightmap){.size = FIELD,
                           .spacing = 1.0F,
                           .altitude_min = -10.0F,
                           .altitude_max = 0.0F,
                           .samples = g_samples};
}

static void camera(mat4 view_proj, EngLighting *light)
{
    mat4 view;
    mat4 proj;
    glm_lookat((vec3){EYE[0], EYE[1], EYE[2]}, (vec3){0.0F, 0.0F, 0.0F}, (vec3){0.0F, 1.0F, 0.0F},
               view);
    glm_perspective(glm_rad(50.0F), 1.0F, 0.5F, 300.0F, proj);
    glm_mat4_mul(proj, view, view_proj);
    eng_lighting_default(light);
    (void)eng_lighting_set_sun_angles(light, 35.0F, 200.0F);
    light->camera_position[0] = EYE[0];
    light->camera_position[1] = EYE[1];
    light->camera_position[2] = EYE[2];
}

static void dump_ppm(const char *name)
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
        (void)fwrite(&g_pixels[(size_t)i * 4U], 1, 3, f);
    }
    (void)fclose(f);
}

typedef struct Scene {
    EngWaterParams params;
    EngWaterSky sky;
    double time;
    const EngFog *fog;
    const EngAtmosphere *atmosphere;
} Scene;

static bool draw(Fixture *t, const Scene *s)
{
    make_lake();
    EngModel model;
    if (!eng_water_build(&t->level, &g_map, &s->params, -16.0F, -16.0F, &model)) {
        return false;
    }
    EngModelGpu gpu;
    if (!eng_model_gpu_create(&gpu, &t->r, &t->permanent, &model, 0.0F)) {
        return false;
    }
    mat4 view_proj;
    EngLighting light;
    camera(view_proj, &light);
    float tail[ENG_FOG_UNIFORM_FLOATS + ENG_WATER_UNIFORM_FLOATS] = {0};
    if (s->fog != NULL) {
        eng_fog_pack(s->fog, s->atmosphere, 1.0F, tail);
    }
    if (!eng_water_pack(&s->params, &s->sky, s->time, tail + ENG_FOG_UNIFORM_FLOATS)) {
        return false;
    }
    DrawContext c = {&t->r};
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    mat4 identity;
    glm_mat4_identity(identity);
    const EngModelDrawOptions options = {.fragment_tail = tail,
                                         .fragment_tail_floats =
                                             ENG_FOG_UNIFORM_FLOATS + ENG_WATER_UNIFORM_FLOATS};
    const bool ok = eng_model_gpu_draw_ex(&gpu, q, t->water, identity, view_proj, &light, NULL,
                                          &options) == 1U &&
                    render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue,
                                           &c, SIZE, g_pixels);
    eng_model_gpu_destroy(&gpu, &t->r);
    return ok;
}

/* point du plan y = 0 vu par le pixel (px, py) ; false si le rayon ne le coupe pas */
static bool plane_point(uint32_t px, uint32_t py, float out[3])
{
    mat4 view_proj;
    EngLighting light;
    camera(view_proj, &light);
    mat4 inverse;
    glm_mat4_inv(view_proj, inverse);
    const float nx = (((float)px + 0.5F) / (float)SIZE * 2.0F) - 1.0F;
    const float ny = 1.0F - (((float)py + 0.5F) / (float)SIZE * 2.0F);
    vec4 near_point = {nx, ny, 0.0F, 1.0F};
    vec4 far_point = {nx, ny, 1.0F, 1.0F};
    vec4 a;
    vec4 b;
    glm_mat4_mulv(inverse, near_point, a);
    glm_mat4_mulv(inverse, far_point, b);
    for (int i = 0; i < 3; i++) {
        a[i] /= a[3];
        b[i] /= b[3];
    }
    const float dy = b[1] - a[1];
    if (fabsf(dy) < 1e-9F) {
        return false;
    }
    const float u = -a[1] / dy;
    if (u <= 0.0F) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        out[i] = a[i] + (u * (b[i] - a[i]));
    }
    return true;
}

static EngWaterLight water_light(const EngLighting *light)
{
    EngWaterLight w;
    for (int c = 0; c < 3; c++) {
        w.sun_direction[c] = light->sun_direction[c];
        w.sun_radiance[c] = light->sun_color[c] * light->sun_intensity;
        w.ambient[c] = light->ambient_color[c] * light->ambient_intensity;
    }
    return w;
}

static float to_byte(float v)
{
    return (v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v)) * 255.0F;
}

/* compare l'image à la référence CPU sur une grille de pixels ; renvoie l'écart maximal (niveaux
 * 8 bits) et le nombre de pixels comparés */
static float compare(const Scene *s, bool with_fog, uint32_t *compared)
{
    mat4 view_proj;
    EngLighting light;
    camera(view_proj, &light);
    const EngWaterLight wl = water_light(&light);
    float worst = 0.0F;
    *compared = 0;
    for (uint32_t py = 20; py < SIZE; py += 5) {
        for (uint32_t px = 10; px < SIZE - 8U; px += 5) {
            float point[3];
            if (!plane_point(px, py, point) || fabsf(point[0]) > 14.0F || fabsf(point[2]) > 14.0F) {
                continue;
            }
            const float depth = 1.0F + (0.1F * (point[0] + 16.0F));
            float color[3];
            eng_water_shade(&s->params, &s->sky, &wl, EYE, point, depth, s->time, color);
            if (with_fog) {
                float fogged[3];
                if (!eng_fog_apply(s->fog, s->atmosphere, wl.sun_direction, 1.0F, EYE, point, color,
                                   fogged)) {
                    return 1000.0F;
                }
                memcpy(color, fogged, sizeof color);
            }
            const uint8_t *got = &g_pixels[((size_t)py * SIZE + px) * 4U];
            for (int c = 0; c < 3; c++) {
                worst = fmaxf(worst, fabsf((float)got[c] - to_byte(color[c])));
            }
            (*compared)++;
        }
    }
    return worst;
}

static Scene base_scene(void)
{
    Scene s = {.params = eng_water_default(0.0F),
               .sky = {.horizon = {0.55F, 0.62F, 0.72F}, .zenith = {0.18F, 0.32F, 0.62F}},
               .time = 0.0};
    return s;
}

UTEST(eng_water_gpu, the_shader_matches_the_cpu_reference_on_a_calm_lake)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Scene s = base_scene();
    s.params.ripple_amplitude = 0.0F;
    ASSERT_TRUE(draw(&t, &s));
    dump_ppm("water_calm");
    uint32_t n = 0;
    EXPECT_LE(compare(&s, false, &n), 3.0F);
    EXPECT_GT(n, 120U);
    CLOSE_FIXTURE(t);
}

UTEST(eng_water_gpu, the_shader_matches_the_cpu_reference_with_ripples_and_time)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Scene s = base_scene();
    s.params.ripple_amplitude = 0.12F;
    s.params.ripple_scale = 1.1F;
    s.time = 3.7;
    ASSERT_TRUE(draw(&t, &s));
    dump_ppm("water_ripples");
    uint32_t n = 0;
    EXPECT_LE(compare(&s, false, &n), 4.0F);
    EXPECT_GT(n, 120U);
    /* le temps déplace les rides : l'image change */
    uint8_t before[SIZE * SIZE * 4];
    memcpy(before, g_pixels, sizeof before);
    s.time = 5.2;
    ASSERT_TRUE(draw(&t, &s));
    int changed = 0;
    for (size_t i = 0; i < sizeof before; i++) {
        changed += abs((int)before[i] - (int)g_pixels[i]) > 2 ? 1 : 0;
    }
    EXPECT_GT(changed, 500);
    EXPECT_LE(compare(&s, false, &n), 4.0F);
    CLOSE_FIXTURE(t);
}

UTEST(eng_water_gpu, the_water_darkens_with_depth_and_the_fog_matches_the_cpu_reference)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    Scene s = base_scene();
    s.params.ripple_amplitude = 0.0F;
    s.params.f0 = 0.0F; /* sans reflet, la couleur est celle du corps : elle suit la profondeur */
    s.sky.horizon[0] = s.sky.horizon[1] = s.sky.horizon[2] = 0.0F;
    s.sky.zenith[0] = s.sky.zenith[1] = s.sky.zenith[2] = 0.0F;
    ASSERT_TRUE(draw(&t, &s));
    /* même ligne de pixels : l'ouest (peu profond) est plus clair que l'est (profond) */
    float west[3];
    float east[3];
    ASSERT_TRUE(plane_point(40, 90, west));
    ASSERT_TRUE(plane_point(88, 90, east));
    ASSERT_LT(west[0], east[0]);
    const uint8_t *w = &g_pixels[((size_t)90 * SIZE + 40) * 4U];
    const uint8_t *e = &g_pixels[((size_t)90 * SIZE + 88) * 4U];
    const int west_sum = (int)w[0] + (int)w[1] + (int)w[2];
    const int east_sum = (int)e[0] + (int)e[1] + (int)e[2];
    EXPECT_GT(west_sum, east_sum + 3);

    /* brouillard : mêmes formules que eng_fog_apply */
    EngAtmosphere atmosphere;
    eng_atmosphere_default(&atmosphere);
    EngFog fog;
    eng_fog_default(&fog);
    fog.density = 0.02F;
    s = base_scene();
    s.params.ripple_amplitude = 0.05F;
    s.time = 1.3;
    s.fog = &fog;
    s.atmosphere = &atmosphere;
    ASSERT_TRUE(draw(&t, &s));
    dump_ppm("water_fog");
    uint32_t n = 0;
    EXPECT_LE(compare(&s, true, &n), 4.0F);
    EXPECT_GT(n, 120U);
    CLOSE_FIXTURE(t);
}

#endif
