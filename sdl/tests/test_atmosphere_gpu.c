#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/atmosphere.h"
#include "engine/renderer.h"
#include "test_render_util.h"
#include "utest.h"

/* Ciel atmosphérique dessiné pour de vrai (M3-05) : le shader atmosphere.frag recopie les formules
 * de engine/src/atmosphere.c (HLSL n'a pas d'#include) ; ces tests comparent, pixel par pixel, le
 * rendu à la version C évaluée dans la direction de chaque pixel, pour plusieurs hauteurs du
 * soleil et orientations de caméra. La cible est R8G8B8A8_UNORM : les valeurs sont linéaires et
 * tronquées à [0, 1], d'où l'échelle d'exposition qui ramène le ciel sous 1. Sans GPU ou sans
 * shaders compilés, ces tests s'arrêtent là (OW_REQUIRE_GPU=1 : échec). */

#ifdef OW_SHADER_DIR

#define SIZE 48U

typedef struct Fixture {
    RenderFixture f;
    EngArena permanent;
    EngRenderer r;
    EngPipelineHandle pipeline;
    EngBufferHandle dummy;
} Fixture;

static bool open_fixture(Fixture *t)
{
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "atmosphere-test", (size_t)2 << 20)) {
        close_render_fixture(&t->f);
        return false;
    }
    const EngRendererDesc desc = {
        .offline_dir = OW_SHADER_DIR, .max_commands = 16, .max_uniform_bytes = 4096};
    if (!eng_renderer_init(&t->r, &t->f.gpu, &t->permanent, &t->f.scratch, &desc)) {
        eng_arena_shutdown(&t->permanent);
        close_render_fixture(&t->f);
        return false;
    }
    EngPipelineDesc pipeline;
    eng_atmosphere_pipeline_desc(&pipeline);
    pipeline.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    t->pipeline = eng_renderer_create_pipeline(&t->r, &pipeline);
    const float dummy[3] = {0.0F, 0.0F, 0.0F};
    const EngBufferDesc buffer = {.name = "atmosphere-test-dummy",
                                  .usage = ENG_BUFFER_VERTEX,
                                  .size = sizeof dummy,
                                  .data = dummy};
    t->dummy = eng_renderer_create_buffer(&t->r, &buffer);
    return true;
}

static uint32_t close_fixture(Fixture *t)
{
    (void)eng_renderer_destroy_buffer(&t->r, t->dummy);
    (void)eng_renderer_destroy_pipeline(&t->r, t->pipeline);
    const uint32_t leaks = eng_renderer_shutdown(&t->r);
    eng_arena_shutdown(&t->permanent);
    close_render_fixture(&t->f);
    return leaks;
}

typedef struct Ctx {
    EngRenderer *renderer;
} Ctx;

static void execute_queue(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    Ctx *c = (Ctx *)context;
    (void)eng_renderer_execute(c->renderer, &c->renderer->queue, commands, pass);
}

static uint8_t g_pixels[SIZE * SIZE * 4];

static void dir(float elevation_deg, float azimuth_deg, float out[3])
{
    const float e = elevation_deg * (3.14159265F / 180.0F);
    const float a = azimuth_deg * (3.14159265F / 180.0F);
    out[0] = cosf(e) * sinf(a);
    out[1] = sinf(e);
    out[2] = cosf(e) * cosf(a);
}

static const float EYE[3] = {7.0F, 30.0F, -3.0F};

/* Dessine le ciel pour la caméra (regard `forward`, champ vertical `fov_deg`) et le soleil. */
static bool draw(Fixture *t, const EngAtmosphere *a, const float sun[3], const float forward[3],
                 float fov_deg, float scale, mat4 view_proj)
{
    mat4 view;
    mat4 proj;
    glm_lookat((vec3){EYE[0], EYE[1], EYE[2]},
               (vec3){EYE[0] + forward[0], EYE[1] + forward[1], EYE[2] + forward[2]},
               (vec3){0.0F, 1.0F, 0.0F}, view);
    glm_perspective(glm_rad(fov_deg), 1.0F, 0.1F, 100.0F, proj);
    glm_mat4_mul(proj, view, view_proj);
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    Ctx c = {&t->r};
    return eng_atmosphere_draw(a, q, t->pipeline, t->dummy, view_proj, EYE, sun, scale) &&
           render_to_pixels(&t->f, execute_queue, &c, SIZE, g_pixels);
}

/* Direction monde du centre du pixel (x, y) : celle du vertex shader (point à la profondeur 0,5,
 * dé-projeté, moins la caméra), recalculée ici avec cglm. */
static void pixel_direction(mat4 view_proj, uint32_t x, uint32_t y, float out[3])
{
    mat4 inverse;
    glm_mat4_inv(view_proj, inverse);
    const float ndc_x = ((((float)x + 0.5F) / (float)SIZE) * 2.0F) - 1.0F;
    const float ndc_y = 1.0F - ((((float)y + 0.5F) / (float)SIZE) * 2.0F);
    vec4 clip = {ndc_x, ndc_y, 0.5F, 1.0F};
    vec4 world;
    glm_mat4_mulv(inverse, clip, world);
    for (int c = 0; c < 3; c++) {
        out[c] = (world[c] / world[3]) - EYE[c];
    }
}

static int byte_of(float linear)
{
    const float v = linear < 0.0F ? 0.0F : (linear > 1.0F ? 1.0F : linear);
    return (int)((v * 255.0F) + 0.5F);
}

/* Compare chaque pixel au calcul C ; renvoie le nombre de pixels qui diffèrent de plus de
 * `tolerance` (en niveaux de 8 bits) et la plus grande différence. */
static uint32_t compare(mat4 view_proj, const EngAtmosphere *a, const float sun[3], float scale,
                        int tolerance, int *worst)
{
    uint32_t bad = 0;
    *worst = 0;
    for (uint32_t y = 0; y < SIZE; y++) {
        for (uint32_t x = 0; x < SIZE; x++) {
            float d[3];
            pixel_direction(view_proj, x, y, d);
            float rgb[3];
            if (!eng_atmosphere_radiance(a, sun, d, rgb)) {
                continue;
            }
            const uint8_t *p = &g_pixels[(((size_t)y * SIZE) + x) * 4U];
            for (int c = 0; c < 3; c++) {
                const int want = byte_of(rgb[c] * scale);
                const int diff = abs((int)p[c] - want);
                if (diff > *worst) {
                    *worst = diff;
                }
                if (diff > tolerance) {
                    bad++;
                    break;
                }
            }
        }
    }
    return bad;
}

#define CLOSE_FIXTURE(t) EXPECT_EQ(close_fixture(&(t)), (uint32_t)0)

UTEST(eng_atmosphere_gpu, the_shader_matches_the_c_sky_for_several_suns_and_cameras)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, t.pipeline));
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    /* regard : horizon vers -Z, légèrement vers le haut (le sol en bas de l'image) */
    const float suns[][2] = {{60, 10}, {30, 200}, {8, 0}, {1, 180}, {-4, 90}, {-20, 0}};
    const float looks[][2] = {{12, 180}, {-30, 40}, {70, 0}};
    for (size_t i = 0; i < sizeof suns / sizeof suns[0]; i++) {
        for (size_t k = 0; k < sizeof looks / sizeof looks[0]; k++) {
            float sun[3];
            float forward[3];
            dir(suns[i][0], suns[i][1], sun);
            dir(looks[k][0], looks[k][1], forward);
            mat4 view_proj;
            ASSERT_TRUE(draw(&t, &a, sun, forward, 70.0F, 0.2F, view_proj));
            int worst;
            const uint32_t bad = compare(view_proj, &a, sun, 0.2F, 2, &worst);
            if (bad != 0) {
                (void)fprintf(stderr, "soleil %.0f/%.0f regard %.0f/%.0f : %u pixels, pire %d\n",
                              (double)suns[i][0], (double)suns[i][1], (double)looks[k][0],
                              (double)looks[k][1], bad, worst);
            }
            EXPECT_EQ(bad, (uint32_t)0);
        }
    }
    CLOSE_FIXTURE(t);
}

UTEST(eng_atmosphere_gpu, the_sun_disc_is_drawn_where_the_c_code_puts_it)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    a.sun_radius_deg = 6.0F; /* gros disque : plusieurs pixels, bord compris */
    float sun[3];
    float forward[3];
    dir(25, 0, sun);
    dir(25, 5, forward); /* soleil à 5° du centre de l'image */
    mat4 view_proj;
    /* échelle 0,01 : le disque (radiance ~ 50) reste sous 1 */
    ASSERT_TRUE(draw(&t, &a, sun, forward, 40.0F, 0.01F, view_proj));
    int worst;
    const uint32_t bad = compare(view_proj, &a, sun, 0.01F, 2, &worst);
    /* quelques pixels du bord peuvent basculer (comparaison de cosinus en flottant) */
    EXPECT_LE(bad, (uint32_t)6);
    /* et le disque est bien là : un pixel plus brillant que tout le ciel alentour */
    int brightest = 0;
    int brightest_x = 0;
    int brightest_y = 0;
    for (uint32_t y = 0; y < SIZE; y++) {
        for (uint32_t x = 0; x < SIZE; x++) {
            const int v = g_pixels[(((size_t)y * SIZE) + x) * 4U];
            if (v > brightest) {
                brightest = v;
                brightest_x = (int)x;
                brightest_y = (int)y;
            }
        }
    }
    float d[3];
    pixel_direction(view_proj, (uint32_t)brightest_x, (uint32_t)brightest_y, d);
    const float len = sqrtf((d[0] * d[0]) + (d[1] * d[1]) + (d[2] * d[2]));
    const float cos_angle = ((d[0] * sun[0]) + (d[1] * sun[1]) + (d[2] * sun[2])) / len;
    EXPECT_GT(cos_angle, cosf(glm_rad(a.sun_radius_deg)));
    CLOSE_FIXTURE(t);
}

UTEST(eng_atmosphere_gpu, the_exposure_scale_multiplies_the_sky_and_a_bad_atmosphere_is_refused)
{
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    float sun[3];
    float forward[3];
    dir(40, 100, sun);
    dir(30, 280, forward);
    mat4 view_proj;
    ASSERT_TRUE(draw(&t, &a, sun, forward, 30.0F, 0.2F, view_proj));
    static uint8_t full[SIZE * SIZE * 4];
    memcpy(full, g_pixels, sizeof full);
    ASSERT_TRUE(draw(&t, &a, sun, forward, 30.0F, 0.1F, view_proj));
    uint32_t checked = 0;
    for (size_t i = 0; i < (size_t)SIZE * SIZE; i++) {
        for (int c = 0; c < 3; c++) {
            const int whole = full[(i * 4U) + (size_t)c];
            const int half = g_pixels[(i * 4U) + (size_t)c];
            EXPECT_LE(abs((whole / 2) - half), 2);
            checked += whole > 20 ? 1U : 0U;
        }
    }
    EXPECT_GT(checked, (uint32_t)1000); /* le test n'est pas sur du noir */
    /* échelle nulle : noir */
    ASSERT_TRUE(draw(&t, &a, sun, forward, 30.0F, 0.0F, view_proj));
    for (size_t i = 0; i < (size_t)SIZE * SIZE; i++) {
        ASSERT_EQ(g_pixels[i * 4U], 0);
    }
    /* échelle négative : traitée comme 0 */
    ASSERT_TRUE(draw(&t, &a, sun, forward, 30.0F, -3.0F, view_proj));
    EXPECT_EQ(g_pixels[(((size_t)(SIZE / 2) * SIZE) + (SIZE / 2)) * 4U], 0);
    /* atmosphère invalide : refusée, rien en file */
    EngAtmosphere bad = a;
    bad.mie = NAN;
    EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
    const uint32_t before = q->count;
    EXPECT_FALSE(eng_atmosphere_draw(&bad, q, t.pipeline, t.dummy, view_proj, EYE, sun, 1.0F));
    EXPECT_EQ(q->count, before);
    /* direction du soleil nulle : repli au zénith, dessin accepté */
    const float zero[3] = {0.0F, 0.0F, 0.0F};
    EXPECT_TRUE(eng_atmosphere_draw(&a, q, t.pipeline, t.dummy, view_proj, EYE, zero, 1.0F));
    CLOSE_FIXTURE(t);
}

UTEST(eng_atmosphere_gpu, dump_the_sky_for_the_eye)
{
    const char *dir_name = SDL_getenv("OW_TERRAIN_DUMP");
    if (dir_name == NULL) {
        return;
    }
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    EngAtmosphere a;
    eng_atmosphere_default(&a);
    const float suns[] = {60.0F, 25.0F, 8.0F, 1.0F, -5.0F, -20.0F};
    for (size_t i = 0; i < sizeof suns / sizeof suns[0]; i++) {
        float sun[3];
        float forward[3];
        dir(suns[i], 0, sun);
        dir(8, 0, forward); /* on regarde vers le soleil, un peu au-dessus de l'horizon */
        mat4 view_proj;
        ASSERT_TRUE(draw(&t, &a, sun, forward, 90.0F, 0.35F, view_proj));
        char path[512];
        SDL_snprintf(path, sizeof path, "%s/atmosphere_%zu.ppm", dir_name, i);
        FILE *f = fopen(path, "wb");
        if (f != NULL) {
            (void)fprintf(f, "P6\n%u %u\n255\n", SIZE, SIZE);
            for (uint32_t k = 0; k < SIZE * SIZE; k++) {
                (void)fwrite(&g_pixels[(size_t)k * 4U], 1, 3, f);
            }
            (void)fclose(f);
        }
    }
    CLOSE_FIXTURE(t);
}

#endif /* OW_SHADER_DIR */
