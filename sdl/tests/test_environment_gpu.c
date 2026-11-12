#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/environment.h"
#include "engine/renderer.h"
#include "test_render_util.h"
#include "utest.h"

/* Environnement GPU et skybox (M1-16). Sans GPU ou sans shaders compilés, ces tests s'arrêtent là
 * (OW_REQUIRE_GPU=1 : échec). Ils vérifient la CONVENTION des faces d'un cube de bout en bout :
 * CPU (engine/ibl.h) -> envoi par couche (renderer) -> shader sky.* -> pixels. Les valeurs
 * attendues sont des couleurs choisies par le test (une par face), pas des sorties du code testé.
 */

#ifdef OW_SHADER_DIR

#define SIZE 64U

typedef struct Fixture {
    RenderFixture f;
    EngArena permanent;
    EngRenderer r;
    EngPipelineHandle sky_pipeline;
} Fixture;

static bool open_fixture(Fixture *t)
{
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "env-test", (size_t)2 << 20)) {
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
    eng_sky_pipeline_desc(&pipeline);
    pipeline.color_format = (uint32_t)SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    t->sky_pipeline = eng_renderer_create_pipeline(&t->r, &pipeline);
    return true;
}

static uint32_t close_fixture(Fixture *t)
{
    (void)eng_renderer_destroy_pipeline(&t->r, t->sky_pipeline);
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

/* Caméra regardant `forward` (up `up`), champ de vision 60° : dessine le ciel. */
static bool draw_sky(Fixture *t, const EngEnvironment *env, const float forward[3],
                     const float up[3], float intensity)
{
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    /* caméra loin de l'origine : la direction du ciel ne dépend que de l'orientation */
    const float eye[3] = {7.0F, -2.0F, 3.0F};
    glm_lookat((vec3){eye[0], eye[1], eye[2]},
               (vec3){eye[0] + forward[0], eye[1] + forward[1], eye[2] + forward[2]},
               (vec3){up[0], up[1], up[2]}, view);
    glm_perspective((float)GLM_PI / 3.0F, 1.0F, 0.1F, 100.0F, proj);
    glm_mat4_mul(proj, view, view_proj);
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    Ctx c = {&t->r};
    return eng_environment_draw_sky(env, q, t->sky_pipeline, view_proj, eye, intensity) &&
           render_to_pixels(&t->f, execute_queue, &c, SIZE, g_pixels);
}

/* Un cube dont chaque face est unie : +X rouge, -X cyan, +Y vert, -Y magenta, +Z bleu, -Z jaune. */
static const float FACE_COLORS[6][3] = {{1, 0, 0}, {0, 1, 1}, {0, 1, 0},
                                        {1, 0, 1}, {0, 0, 1}, {1, 1, 0}};

static bool make_face_cube(Fixture *t, EngEnvironment *env)
{
    const float white[3] = {1, 1, 1};
    if (!eng_environment_create_uniform(env, &t->r, white)) {
        return false;
    }
    (void)eng_renderer_destroy_texture(&t->r, env->sky);
    uint16_t half[6 * 4 * 4 * 4];
    for (uint32_t face = 0; face < 6; face++) {
        for (uint32_t i = 0; i < 16; i++) {
            for (uint32_t c = 0; c < 3; c++) {
                half[(((face * 16U) + i) * 4U) + c] = eng_float_to_half(FACE_COLORS[face][c]);
            }
            half[(((face * 16U) + i) * 4U) + 3U] = eng_float_to_half(1.0F);
        }
    }
    const EngTextureDataDesc desc = {.name = "faces",
                                     .width = 4,
                                     .height = 4,
                                     .layers = 6,
                                     .mip_levels = 1,
                                     .format = ENG_TEXTURE_RGBA16F,
                                     .data = half};
    env->sky = eng_renderer_create_texture_data(&t->r, &desc);
    return eng_renderer_texture_is_valid(&t->r, env->sky);
}

UTEST(eng_environment_gpu, each_cube_face_is_seen_along_its_axis)
{
    Fixture t;
    if (!open_fixture(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, t.sky_pipeline));
    EngEnvironment env;
    ASSERT_TRUE(make_face_cube(&t, &env));
    ASSERT_EQ(eng_renderer_texture_layers(&t.r, env.sky), (uint32_t)6);
    const float axes[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int face = 0; face < 6; face++) {
        /* « haut » : +Y sauf pour les faces +Y et -Y, où on prend -Z */
        const float up[3] = {0, face < 2 || face > 3 ? 1.0F : 0.0F,
                             face < 2 || face > 3 ? 0.0F : -1.0F};
        ASSERT_TRUE(draw_sky(&t, &env, axes[face], up, 1.0F));
        EXPECT_TRUE(pixel_is(
            g_pixels, SIZE, SIZE / 2, SIZE / 2, (int)(FACE_COLORS[face][0] * 255.0F),
            (int)(FACE_COLORS[face][1] * 255.0F), (int)(FACE_COLORS[face][2] * 255.0F), 2));
    }
    eng_environment_destroy(&env, &t.r);
    EXPECT_EQ(close_fixture(&t), (uint32_t)0);
}

UTEST(eng_environment_gpu, the_sky_intensity_scales_the_output)
{
    Fixture t;
    if (!open_fixture(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngEnvironment env;
    ASSERT_TRUE(make_face_cube(&t, &env));
    const float forward[3] = {1, 0, 0};
    const float up[3] = {0, 1, 0};
    ASSERT_TRUE(draw_sky(&t, &env, forward, up, 0.5F));
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, SIZE / 2, SIZE / 2, 128, 0, 0, 2));
    eng_environment_destroy(&env, &t.r);
    EXPECT_EQ(close_fixture(&t), (uint32_t)0);
}

UTEST(eng_environment_gpu, the_procedural_sky_has_the_zenith_above_and_the_ground_below)
{
    Fixture t;
    if (!open_fixture(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngSkyDesc sky = {.zenith = {0.0F, 0.0F, 0.9F},
                      .horizon = {0.5F, 0.5F, 0.5F},
                      .ground = {0.6F, 0.2F, 0.0F},
                      .sun_direction = {0, 1, 0},
                      .sun_radiance = {0, 0, 0},
                      .sun_angular_radius_deg = 1.0F};
    const EngEnvironmentDesc desc = {
        .sky = &sky, .sky_size = 32, .irradiance_size = 8, .prefiltered_size = 32, .samples = 16};
    EngEnvironment env;
    ASSERT_TRUE(eng_environment_create(&env, &t.r, &desc));
    ASSERT_TRUE(eng_environment_is_valid(&env, &t.r));
    EXPECT_EQ(env.prefiltered_levels, (uint32_t)2); /* 32 puis 16 */
    const float forward[3] = {0, 0, -1};
    const float up[3] = {0, 1, 0};
    ASSERT_TRUE(draw_sky(&t, &env, forward, up, 1.0F));
    /* tout en haut de l'image : bleu (zénith) ; tout en bas : le sol (orange) */
    const uint8_t *top = g_pixels + ((((size_t)2 * SIZE) + (SIZE / 2)) * 4);
    const uint8_t *bottom = g_pixels + ((((size_t)(SIZE - 3U) * SIZE) + (SIZE / 2)) * 4);
    EXPECT_GT((int)top[2], (int)top[0] + 60);
    EXPECT_EQ((int)bottom[0], 153); /* 0,6 : le sol uni */
    EXPECT_EQ((int)bottom[1], 51);
    EXPECT_EQ((int)bottom[2], 0);
    eng_environment_destroy(&env, &t.r);
    EXPECT_EQ(close_fixture(&t), (uint32_t)0);
}

UTEST(eng_environment_gpu, invalid_descriptions_are_refused_without_leaking)
{
    Fixture t;
    if (!open_fixture(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngEnvironment env;
    const EngEnvironmentDesc bad_size = {.sky_size = 30};
    EXPECT_FALSE(eng_environment_create(&env, &t.r, &bad_size));
    const EngEnvironmentDesc small_sky = {.sky_size = 16, .prefiltered_size = 64};
    EXPECT_FALSE(eng_environment_create(&env, &t.r, &small_sky));
    const float rgb[3] = {1, 1, 1};
    const EngEnvironmentDesc bad_equirect = {
        .equirect_rgb = rgb, .equirect_width = 1, .equirect_height = 1};
    EXPECT_FALSE(eng_environment_create(&env, &t.r, &bad_equirect));
    EXPECT_EQ(close_fixture(&t), (uint32_t)0);
}

UTEST(eng_environment_gpu, texture_data_refuses_bad_descriptions)
{
    Fixture t;
    if (!open_fixture(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    static const uint16_t data[6 * 4 * 4 * 4] = {0};
    EngTextureDataDesc d = {.name = "x",
                            .width = 4,
                            .height = 4,
                            .layers = 6,
                            .mip_levels = 1,
                            .format = ENG_TEXTURE_RGBA16F,
                            .data = data};
    EngTextureHandle h = eng_renderer_create_texture_data(&t.r, &d);
    EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, h));
    EXPECT_TRUE(eng_renderer_destroy_texture(&t.r, h));
    d.layers = 5; /* ni 1 ni 6 */
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture_data(&t.r, &d)));
    d.layers = 6;
    d.height = 8; /* cube non carré */
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture_data(&t.r, &d)));
    d.height = 4;
    d.mip_levels = 4; /* plus que la chaîne complète (3 niveaux pour 4 x 4) */
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture_data(&t.r, &d)));
    d.mip_levels = 1;
    d.data = NULL;
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture_data(&t.r, &d)));
    EXPECT_EQ(close_fixture(&t), (uint32_t)0);
}

/* Hors validation : sans OW_ENV (un .hdr équirectangulaire) et OW_DUMP (dossier), ne fait rien.
 * Sinon dessine le ciel sous quatre directions en .ppm linéaire 256 x 256, pour la comparaison de
 * l'ORIENTATION et de l'échelle avec three.js (tools/reference/README.md). */
UTEST(eng_environment_gpu, dump_the_sky_for_the_reference_viewer)
{
    const char *hdr_path = getenv("OW_ENV");
    const char *dir = getenv("OW_DUMP");
    if (hdr_path == NULL || dir == NULL) {
        return;
    }
    Fixture t;
    if (!open_fixture(&t)) {
        return;
    }
    float *hdr = NULL;
    uint32_t hw;
    uint32_t hh;
    ASSERT_TRUE(eng_environment_load_hdr(hdr_path, &hdr, &hw, &hh));
    const EngEnvironmentDesc desc = {.equirect_rgb = hdr,
                                     .equirect_width = hw,
                                     .equirect_height = hh,
                                     .sky_size = 256,
                                     .irradiance_size = 16,
                                     .prefiltered_size = 64,
                                     .samples = 16};
    EngEnvironment env;
    ASSERT_TRUE(eng_environment_create(&env, &t.r, &desc));
    const float dirs[5][3] = {{1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0.3F, 0}, {0.5F, 0.2F, -0.8F}};
    static uint8_t big[256 * 256 * 4];
    for (int i = 0; i < 5; i++) {
        mat4 view;
        mat4 proj;
        mat4 view_proj;
        glm_lookat((vec3){0, 0, 0}, (float *)dirs[i], (vec3){0, 1, 0}, view);
        glm_perspective(glm_rad(60.0F), 1.0F, 0.1F, 100.0F, proj);
        glm_mat4_mul(proj, view, view_proj);
        const float camera[3] = {0, 0, 0};
        EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
        Ctx c = {&t.r};
        ASSERT_TRUE(eng_environment_draw_sky(&env, q, t.sky_pipeline, view_proj, camera, 1.0F));
        ASSERT_TRUE(render_to_pixels(&t.f, execute_queue, &c, 256, big));
        char path[512];
        SDL_snprintf(path, sizeof path, "%s/sky_%d.ppm", dir, i);
        FILE *out = fopen(path, "wb");
        if (out != NULL) {
            fprintf(out, "P6\n256 256\n255\n");
            for (int p = 0; p < 256 * 256; p++) {
                fwrite(big + ((size_t)p * 4U), 1, 3, out);
            }
            fclose(out);
        }
    }
    eng_environment_destroy(&env, &t.r);
    eng_environment_free_hdr(hdr);
    EXPECT_EQ(close_fixture(&t), (uint32_t)0);
}

#else
UTEST(eng_environment_gpu, shaders_not_available)
{
    EXPECT_TRUE(true);
}
#endif
