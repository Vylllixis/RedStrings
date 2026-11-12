#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/environment.h"
#include "engine/ibl.h"
#include "engine/model.h"
#include "engine/model_gpu.h"
#include "engine/pbr.h"
#include "test_gltf.h"
#include "test_render_util.h"
#include "test_ibl_ref.h"
#include "utest.h"

/* Modèles glTF dessinés pour de vrai (M1-10, matériaux PBR M1-15) : un modèle fabriqué en mémoire
 * (test_gltf.h), envoyé au GPU, dessiné avec les shaders model.*, puis les pixels sont lus. Le
 * triangle de référence couvre le quart supérieur droit de la cible (NDC (0,0) (1,0) (0,1))
 * quand les matrices sont l'identité ; le pixel de test (IN_TRIANGLE_X, IN_TRIANGLE_Y) est à la
 * position monde (0,390625 ; 0,234375 ; 0). Les valeurs attendues viennent de TROIS sources : la
 * référence CPU eng_pbr_shade (que le shader doit égaler), une implémentation Python indépendante
 * (valeurs « Python » ci-dessous, voir test_pbr.c) et des calculs à la main quand la formule se
 * simplifie (émissif, occlusion). Sans GPU ou sans shaders compilés, ces tests s'arrêtent là. */

#ifdef OW_SHADER_DIR

#define SIZE 64U
#define TOL 14
/* éclairage calculé : une marge pour l'arrondi 8 bits et les flottants du GPU */
#define TOL_LIT 3

/* Centre du pixel de test, en coordonnées monde. */
#define PIXEL_WORLD_X 0.390625F
#define PIXEL_WORLD_Y 0.234375F

/* Éclairage commun : soleil blanc d'intensité 0,7 dans la direction (0,4 ; 0,8 ; 0,45)
 * normalisée (longueur 1,0012), ambiante blanche 0,3, caméra en (0, 0, 5). */
static EngLighting test_light(void)
{
    EngLighting l = {0};
    const float len = sqrtf((0.4F * 0.4F) + (0.8F * 0.8F) + (0.45F * 0.45F));
    l.sun_direction[0] = 0.4F / len;
    l.sun_direction[1] = 0.8F / len;
    l.sun_direction[2] = 0.45F / len;
    for (int c = 0; c < 3; c++) {
        l.sun_color[c] = 1.0F;
        l.ambient_color[c] = 1.0F;
    }
    l.sun_intensity = 0.7F;
    l.ambient_intensity = 0.3F;
    l.camera_position[2] = 5.0F;
    return l;
}

typedef struct TestModel {
    RenderFixture f;
    EngArena permanent;
    EngArena level;
    EngRenderer r;
    EngPipelineHandle pipeline;
    uint8_t file[BUF_SIZE];
} TestModel;

static bool open_model(TestModel *t)
{
    if (!open_render_fixture(&t->f)) {
        return false;
    }
    if (!eng_arena_init(&t->permanent, "model-test", (size_t)8 << 20) ||
        !eng_arena_init(&t->level, "model-level", (size_t)1 << 20)) {
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
    t->pipeline = eng_renderer_create_pipeline(&t->r, &pipeline);
    return true;
}

static uint32_t close_model(TestModel *t)
{
    (void)eng_renderer_destroy_pipeline(&t->r, t->pipeline);
    const uint32_t leaks = eng_renderer_shutdown(&t->r);
    eng_arena_shutdown(&t->level);
    eng_arena_shutdown(&t->permanent);
    close_render_fixture(&t->f);
    return leaks;
}

typedef struct DrawContext {
    EngRenderer *renderer;
} DrawContext;

static void execute_queue(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    DrawContext *c = (DrawContext *)context;
    (void)eng_renderer_execute(c->renderer, &c->renderer->queue, commands, pass);
}

static uint8_t g_pixels[SIZE * SIZE * 4];
/* Environnement imposé aux modèles dessinés par draw_spec_lit (NULL : le défaut, ciel uniforme). */
static const EngEnvironment *g_env;

/* Charge le modèle décrit par `spec`, le dessine avec `model_matrix` (identité si NULL) et lit
 * les pixels dans g_pixels. Renvoie false si une étape échoue ; détruit le modèle GPU avant de
 * rendre la main (les fuites sont comptées à la fermeture). */
static bool draw_spec_lit(TestModel *t, const Spec *spec, mat4 model_matrix,
                          const EngLighting *lighting)
{
    const size_t size = build(spec, t->file, sizeof t->file, NULL, NULL);
    EngModel model;
    if (!eng_model_parse(&t->level, &t->f.scratch, t->file, size, NULL, &model)) {
        return false;
    }
    EngModelGpu gpu;
    const bool created = eng_model_gpu_create(&gpu, &t->r, &t->permanent, &model, 0.0F);
    eng_model_free(&model);
    if (!created || (g_env != NULL && !eng_model_gpu_set_environment(&gpu, &t->r, g_env))) {
        if (created) {
            eng_model_gpu_destroy(&gpu, &t->r);
        }
        return false;
    }
    mat4 identity;
    glm_mat4_identity(identity);
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    const uint32_t accepted = eng_model_gpu_draw(
        &gpu, q, t->pipeline, model_matrix != NULL ? model_matrix : identity, identity, lighting);
    DrawContext c = {&t->r};
    const bool ok = accepted == gpu.part_count &&
                    render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue,
                                           &c, SIZE, g_pixels);
    eng_model_gpu_destroy(&gpu, &t->r);
    return ok;
}

/* Éclairage commun de test_light(). */
static bool draw_spec(TestModel *t, const Spec *spec, mat4 model_matrix)
{
    const EngLighting light = test_light();
    return draw_spec_lit(t, spec, model_matrix, &light);
}

#define CLOSE_MODEL(t) EXPECT_EQ(close_model(&(t)), (uint32_t)0)

/* Diélectrique rouge mat : métal 0, rugosité 1 (les défauts de glTF seraient métal 1, rugosité 1)
 */
#define RED_MATERIAL                                                                               \
    "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,0,0,1],\"metallicFactor\":0,"               \
    "\"roughnessFactor\":1}}"
#define TEXTURED "{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}"

/* Pixel dans le triangle de référence (quart supérieur droit), loin de l'hypoténuse. */
#define TOL_TEXEL 30
#define IN_TRIANGLE_X 44U
#define IN_TRIANGLE_Y 24U

/* Valeur 8 bits attendue (arrondie) d'une composante en [0, 1]. */
static int level(float value)
{
    const float v = value < 0.0F ? 0.0F : (value > 1.0F ? 1.0F : value);
    return (int)((v * 255.0F) + 0.5F);
}

/* Position monde (= NDC, les matrices du test sont l'identité) du centre du pixel (px, py). */
static void pixel_world(uint32_t px, uint32_t py, float out[3])
{
    out[0] = (((float)px + 0.5F) / (float)SIZE * 2.0F) - 1.0F;
    out[1] = 1.0F - (((float)py + 0.5F) / (float)SIZE * 2.0F);
    out[2] = 0.0F;
}

static EngPbrSurface surf(float r, float g, float b, float metallic, float roughness)
{
    EngPbrSurface s;
    eng_pbr_surface_default(&s);
    s.base_color[0] = r;
    s.base_color[1] = g;
    s.base_color[2] = b;
    s.metallic = metallic;
    s.roughness = roughness;
    return s;
}

/* Matériau JSON d'un diélectrique/métal uni (facteurs seuls). */
static void factors_material(char *out, size_t size, float r, float g, float b, float metallic,
                             float roughness)
{
    SDL_snprintf(out, size,
                 "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[%g,%g,%g,1],"
                 "\"metallicFactor\":%g,\"roughnessFactor\":%g}}",
                 (double)r, (double)g, (double)b, (double)metallic, (double)roughness);
}

/* Le pixel (px, py) de g_pixels vaut la référence CPU (surface, normale) à `tolerance` près.
 * Macro : EXPECT_* ne s'utilise que dans un test (utest). */
#define EXPECT_PIXEL_IS_PBR(light, surface, normal, px, py, tolerance)                             \
    do {                                                                                           \
        float position_[3];                                                                        \
        float expected_[3];                                                                        \
        pixel_world((px), (py), position_);                                                        \
        test_shade((light), (surface), (normal), position_, expected_);                            \
        const uint8_t *got_ = g_pixels + ((((size_t)(py) * SIZE) + (px)) * 4);                     \
        EXPECT_NEAR((float)got_[0], (float)level(expected_[0]), (float)(tolerance));               \
        EXPECT_NEAR((float)got_[1], (float)level(expected_[1]), (float)(tolerance));               \
        EXPECT_NEAR((float)got_[2], (float)level(expected_[2]), (float)(tolerance));               \
    } while (0)

/* Le pixel vaut ces trois niveaux à `tolerance` près (valeurs à la main ou de Python). */
#define EXPECT_PIXEL_LEVELS(px, py, r, g, b, tolerance)                                            \
    do {                                                                                           \
        const uint8_t *p_ = g_pixels + ((((size_t)(py) * SIZE) + (px)) * 4);                       \
        EXPECT_NEAR((float)p_[0], (float)(r), (float)(tolerance));                                 \
        EXPECT_NEAR((float)p_[1], (float)(g), (float)(tolerance));                                 \
        EXPECT_NEAR((float)p_[2], (float)(b), (float)(tolerance));                                 \
    } while (0)

/* ── Éclairage et matériau de base ── */

UTEST(eng_model_gpu, a_colored_material_is_lit_by_the_normal)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    ASSERT_TRUE(eng_renderer_pipeline_is_valid(&t.r, t.pipeline));
    const EngLighting light = test_light();
    const EngPbrSurface red = surf(1.0F, 0.0F, 0.0F, 0.0F, 1.0F);
    /* n = +Z ; valeurs de pbr_ref.py (double précision) : (156, 2, 2) */
    Spec s = {.material = RED_MATERIAL};
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 156, 2, 2, TOL_LIT);
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    EXPECT_PIXEL_IS_PBR(&light, &red, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, 16, 48, 0, 0, 0, 0)); /* hors du triangle : fond noir */
    /* n = +Y (face au soleil) : (224, 10, 10) */
    s.normal[0] = 0.0F;
    s.normal[1] = 1.0F;
    s.normal[2] = 0.0F;
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 224, 10, 10, TOL_LIT);
    /* n = -Y : le soleil est derrière, il ne reste que l'ambiante : (78, 1, 1) */
    s.normal[1] = -1.0F;
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 82, 5, 5, TOL_LIT);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, the_model_matrix_moves_the_triangle_and_turns_its_normals)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Rotation de 90° autour de Z : le triangle passe dans le quart supérieur GAUCHE ; la normale
     * (1,0,0) du fichier devient (0,1,0) -> (224, 10, 10) (Python). Avec la matrice du modèle
     * oubliée, la normale resterait (1,0,0) : 0,3 ambiante + 0,7 x 0,4 / 1,0012 diffus, nettement
     * plus sombre. */
    mat4 rotate;
    glm_rotate_make(rotate, (float)GLM_PI_2, (vec3){0.0F, 0.0F, 1.0F});
    const Spec s = {.material = RED_MATERIAL, .normal = {1.0F, 0.0F, 0.0F}};
    ASSERT_TRUE(draw_spec(&t, &s, rotate));
    EXPECT_PIXEL_LEVELS(SIZE - 1U - IN_TRIANGLE_X, IN_TRIANGLE_Y, 224, 10, 10, TOL_LIT);
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, IN_TRIANGLE_X, IN_TRIANGLE_Y, 0, 0, 0, 0));
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, metallic_and_roughness_factors_match_the_cpu_reference)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const EngLighting light = test_light();
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    const float cases[5][2] = {
        {0.0F, 1.0F}, {0.0F, 0.3F}, {1.0F, 0.3F}, {1.0F, 1.0F}, {0.5F, 0.5F}};
    for (int i = 0; i < 5; i++) {
        char json[256];
        factors_material(json, sizeof json, 0.8F, 0.6F, 0.2F, cases[i][0], cases[i][1]);
        const Spec s = {.material = json};
        ASSERT_TRUE(draw_spec(&t, &s, NULL));
        const EngPbrSurface surface = surf(0.8F, 0.6F, 0.2F, cases[i][0], cases[i][1]);
        EXPECT_PIXEL_IS_PBR(&light, &surface, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    }
    /* un métal de base (0,8 ; 0,6 ; 0,2), rugosité 0,3 : (65, 49, 16) (Python) ; le même
     * diélectrique à rugosité 0,6 et métal 0,3 : (105, 81, 30) */
    char json[256];
    factors_material(json, sizeof json, 0.8F, 0.6F, 0.2F, 1.0F, 0.3F);
    Spec s = {.material = json};
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 65, 49, 16, TOL_LIT);
    factors_material(json, sizeof json, 0.8F, 0.6F, 0.2F, 0.3F, 0.6F);
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 105, 81, 30, TOL_LIT);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, the_specular_peak_follows_the_camera_and_the_light)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Métal lisse (rugosité 0,25) éclairé par un soleil en +Z, sans ambiante : le reflet est un
     * pic autour de la bissectrice. Caméra à la verticale du pixel : au pic ; caméra à 5 m sur le
     * côté : le pic s'est éteint (valeurs obtenues par la référence CPU, que le test impose aussi
     * plus de 100 niveaux en dessous). */
    char json[256];
    factors_material(json, sizeof json, 0.5F, 0.5F, 0.5F, 1.0F, 0.25F);
    const Spec s = {.material = json};
    const EngPbrSurface metal = surf(0.5F, 0.5F, 0.5F, 1.0F, 0.25F);
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    EngLighting light = test_light();
    light.sun_direction[0] = 0.0F;
    light.sun_direction[1] = 0.0F;
    light.sun_direction[2] = 1.0F;
    light.ambient_intensity = 0.0F;
    light.sun_intensity = 0.05F; /* pic ~ 0,05 x 0,5 x 0,25 / alpha^2 (alpha^2 = 0,0039) = 1,6 */
    float over[3];
    pixel_world(IN_TRIANGLE_X, IN_TRIANGLE_Y, over);
    light.camera_position[0] = over[0];
    light.camera_position[1] = over[1];
    light.camera_position[2] = 5.0F;
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &light));
    EXPECT_PIXEL_IS_PBR(&light, &metal, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    const int peak = g_pixels[(((size_t)IN_TRIANGLE_Y * SIZE) + IN_TRIANGLE_X) * 4];
    EXPECT_EQ(peak, 255); /* HDR > 1 écrêté par la cible 8 bits de ce test */
    light.camera_position[0] = over[0] + 5.0F;
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &light));
    EXPECT_PIXEL_IS_PBR(&light, &metal, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    EXPECT_LT((int)g_pixels[(((size_t)IN_TRIANGLE_Y * SIZE) + IN_TRIANGLE_X) * 4], peak - 100);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, no_direct_light_reaches_a_face_turned_away_from_the_sun)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* soleil à 100° de la normale : N.L < 0, mais N.H reste positif ; sans la garde N.L > 0 un
     * reflet apparaîtrait. Ne reste que l'ambiante (référence CPU) */
    const float rad = 0.017453292F;
    EngLighting light = test_light();
    light.sun_direction[0] = sinf(100.0F * rad);
    light.sun_direction[1] = 0.0F;
    light.sun_direction[2] = cosf(100.0F * rad);
    light.sun_intensity = 4.0F;
    float over[3];
    pixel_world(IN_TRIANGLE_X, IN_TRIANGLE_Y, over);
    light.camera_position[0] = over[0];
    light.camera_position[1] = over[1];
    char json[256];
    /* métal 0,5 : un diffus (c_diff = 0,25) ET un spéculaire que la garde doit éteindre */
    factors_material(json, sizeof json, 0.5F, 0.5F, 0.5F, 0.5F, 0.3F);
    const Spec s = {.material = json};
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &light));
    const EngPbrSurface surface = surf(0.5F, 0.5F, 0.5F, 0.5F, 0.3F);
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    EngLighting no_sun = light;
    no_sun.sun_intensity = 0.0F;
    float expected[3];
    pixel_world(IN_TRIANGLE_X, IN_TRIANGLE_Y, over);
    test_shade(&no_sun, &surface, up_z, over, expected);
    EXPECT_PIXEL_IS_PBR(&light, &surface, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, level(expected[0]), level(expected[1]),
                        level(expected[2]), TOL_LIT);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, the_world_position_and_the_camera_position_drive_the_view_vector)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Le triangle est translaté de 0,5 en z (profondeur 0,5, visible) : la position MONDE du
     * pixel a z = 0,5. Caméra proche, en (x + 1, y, 1) : V = (1, 0, 0,5) / 1,118 depuis le point
     * monde, (1, 0, 1) / 1,414 avec la position locale. Le test exige un écart net entre les deux
     * hypothèses, puis que le shader suive la bonne. */
    mat4 lift;
    glm_translate_make(lift, (vec3){0.0F, 0.0F, 0.5F});
    float base_pos[3];
    pixel_world(IN_TRIANGLE_X, IN_TRIANGLE_Y, base_pos);
    EngLighting light = test_light();
    light.sun_direction[0] = 0.0F;
    light.sun_direction[1] = 0.0F;
    light.sun_direction[2] = 1.0F;
    light.ambient_intensity = 0.0F;
    light.sun_intensity = 1.0F;
    light.camera_position[0] = base_pos[0] + 1.0F;
    light.camera_position[1] = base_pos[1];
    light.camera_position[2] = 1.0F;
    char json[256];
    factors_material(json, sizeof json, 0.5F, 0.5F, 0.5F, 1.0F, 0.5F);
    const Spec s = {.material = json};
    ASSERT_TRUE(draw_spec_lit(&t, &s, lift, &light));
    const EngPbrSurface surface = surf(0.5F, 0.5F, 0.5F, 1.0F, 0.5F);
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    const float world_pos[3] = {base_pos[0], base_pos[1], 0.5F};
    float with_world[3];
    float with_local[3];
    test_shade(&light, &surface, up_z, world_pos, with_world);
    test_shade(&light, &surface, up_z, base_pos, with_local);
    EXPECT_GT(fabsf(with_world[0] - with_local[0]) * 255.0F, 20.0F); /* le test est discriminant */
    const uint8_t *got = g_pixels + ((((size_t)IN_TRIANGLE_Y * SIZE) + IN_TRIANGLE_X) * 4);
    EXPECT_NEAR((float)got[0], (float)level(with_world[0]), (float)TOL_LIT);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, the_sun_and_ambient_colors_and_intensities_scale_the_result)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* soleil orangé (1 ; 0,5 ; 0) d'intensité 0,5 en +Z, ambiante bleue (0 ; 0 ; 1) d'intensité
     * 0,2, caméra de face, diélectrique blanc mat : référence CPU, et les canaux sont bien
     * indépendants (le rouge ne reçoit rien de l'ambiante bleue, le bleu rien du soleil orangé) */
    EngLighting light = {.sun_direction = {0.0F, 0.0F, 1.0F},
                         .sun_color = {1.0F, 0.5F, 0.0F},
                         .sun_intensity = 0.5F,
                         .ambient_color = {0.0F, 0.0F, 1.0F},
                         .ambient_intensity = 0.2F,
                         .camera_position = {0.390625F, 0.234375F, 5.0F}};
    char json[256];
    factors_material(json, sizeof json, 1.0F, 1.0F, 1.0F, 0.0F, 1.0F);
    const Spec s = {.material = json};
    const EngPbrSurface white = surf(1.0F, 1.0F, 1.0F, 0.0F, 1.0F);
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &light));
    EXPECT_PIXEL_IS_PBR(&light, &white, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    const uint8_t *p = g_pixels + ((((size_t)IN_TRIANGLE_Y * SIZE) + IN_TRIANGLE_X) * 4);
    EXPECT_GT((int)p[0], 100); /* le soleil donne du rouge ... */
    EXPECT_GT((int)p[0], (int)p[1]);
    EXPECT_GT((int)p[2], 40); /* ... l'ambiante du bleu */
    /* un éclairage invalide est corrigé plutôt que transmis tel quel : direction nulle -> +Y (le
     * soleil n'éclaire plus la normale +Z : ambiante seule), intensité négative -> 0 */
    light.sun_direction[2] = 0.0F;
    light.sun_intensity = -3.0F;
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &light));
    EngLighting fixed = light;
    EXPECT_FALSE(eng_lighting_sanitize(&fixed));
    EXPECT_PIXEL_IS_PBR(&fixed, &white, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    EXPECT_EQ((int)g_pixels[(((size_t)IN_TRIANGLE_Y * SIZE) + IN_TRIANGLE_X) * 4], 0);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, a_null_lighting_uses_the_default_and_both_uniform_blocks_are_pushed)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const uint64_t before = t.r.stats.uniform_pushes;
    const Spec s = {.material = RED_MATERIAL};
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, NULL));
    /* un seul dessin : un envoi pour le vertex shader, un pour le fragment shader */
    EXPECT_EQ(t.r.stats.uniform_pushes - before, (uint64_t)2);
    EngLighting def;
    eng_lighting_default(&def);
    const EngPbrSurface red = surf(1.0F, 0.0F, 0.0F, 0.0F, 1.0F);
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    EXPECT_PIXEL_IS_PBR(&def, &red, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, a_viewer_behind_the_surface_gets_a_finite_color_like_the_reference)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* caméra sous le plan du triangle (z = -5) : N.V < 0. Le plancher de N.V évite une division
     * par zéro : pas de NaN, et le shader égale la référence */
    EngLighting light = test_light();
    light.camera_position[2] = -5.0F;
    char json[256];
    factors_material(json, sizeof json, 0.8F, 0.6F, 0.2F, 1.0F, 0.3F);
    const Spec s = {.material = json};
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &light));
    const EngPbrSurface surface = surf(0.8F, 0.6F, 0.2F, 1.0F, 0.3F);
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    EXPECT_PIXEL_IS_PBR(&light, &surface, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, a_non_unit_scale_does_not_change_the_shading_of_a_flat_face)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Échelle x2 : la matrice normale (inverse transposée) donne des normales de longueur 0,5 ; le
     * shader doit les renormaliser. Le triangle devient (0,0) (2,0) (0,2) et couvre toujours le
     * pixel de test : même résultat que sans échelle, à la position monde près. */
    mat4 scale;
    glm_scale_make(scale, (vec3){2.0F, 2.0F, 2.0F});
    const Spec s = {.material = RED_MATERIAL};
    ASSERT_TRUE(draw_spec(&t, &s, scale));
    const EngLighting light = test_light();
    const EngPbrSurface red = surf(1.0F, 0.0F, 0.0F, 0.0F, 1.0F);
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    EXPECT_PIXEL_IS_PBR(&light, &red, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

/* Remplace la valeur d'un champ de la commande, pour fabriquer une file incohérente. */
typedef enum Tamper {
    TAMPER_NONE,
    TAMPER_FRAGMENT_OFFSET_PAST_THE_END,
    TAMPER_FRAGMENT_SIZE_PAST_THE_END,
    TAMPER_FRAGMENT_SIZE_ABOVE_THE_MAXIMUM,
    TAMPER_VERTEX_OFFSET_PAST_THE_END,
    TAMPER_VERTEX_SIZE_PAST_THE_END,
} Tamper;

/* Dessine un triangle rouge avec une commande modifiée ; renvoie le nombre de dessins rejetés par
 * le renderer, et laisse le résultat dans g_pixels. */
static uint64_t run_tampered(TestModel *t, Tamper tamper)
{
    const size_t size =
        build(&(Spec){.material = RED_MATERIAL}, t->file, sizeof t->file, NULL, NULL);
    EngModel model;
    if (!eng_model_parse(&t->level, &t->f.scratch, t->file, size, NULL, &model)) {
        return 99;
    }
    EngModelGpu gpu;
    const bool created = eng_model_gpu_create(&gpu, &t->r, &t->permanent, &model, 0.0F);
    eng_model_free(&model);
    if (!created) {
        return 99;
    }
    mat4 identity;
    glm_mat4_identity(identity);
    EngLighting light = test_light();
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    (void)eng_model_gpu_draw(&gpu, q, t->pipeline, identity, identity, &light);
    EngDrawCmd *cmd = &q->commands[0];
    switch (tamper) {
    case TAMPER_NONE:
        break;
    case TAMPER_FRAGMENT_OFFSET_PAST_THE_END:
        cmd->fragment_uniform_offset = q->uniform_used + 16U;
        break;
    case TAMPER_FRAGMENT_SIZE_PAST_THE_END:
        cmd->fragment_uniform_size = q->uniform_used; /* offset > 0 : dépasse la zone utilisée */
        break;
    case TAMPER_FRAGMENT_SIZE_ABOVE_THE_MAXIMUM:
        q->uniform_used = ENG_MAX_UNIFORM_BYTES * 2U; /* de la place : seul le maximum refuse */
        q->uniform_capacity = ENG_MAX_UNIFORM_BYTES * 2U;
        cmd->fragment_uniform_size = ENG_MAX_UNIFORM_BYTES + 16U;
        break;
    case TAMPER_VERTEX_OFFSET_PAST_THE_END:
        cmd->uniform_offset = q->uniform_used + 16U;
        break;
    case TAMPER_VERTEX_SIZE_PAST_THE_END:
        cmd->uniform_size = q->uniform_used + 16U; /* offset 0 : un octet de trop suffit */
        break;
    }
    const uint64_t before = t->r.stats.rejected_commands;
    DrawContext c = {&t->r};
    const bool ok = render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue,
                                           &c, SIZE, g_pixels);
    eng_model_gpu_destroy(&gpu, &t->r);
    return ok ? t->r.stats.rejected_commands - before : 99;
}

UTEST(eng_model_gpu, a_command_whose_uniform_blocks_leave_the_queue_zone_is_rejected)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* contrôle : la commande intacte est dessinée (156 : valeur Python), rien n'est rejeté */
    EXPECT_EQ(run_tampered(&t, TAMPER_NONE), (uint64_t)0);
    EXPECT_TRUE(pixel_is(g_pixels, SIZE, IN_TRIANGLE_X, IN_TRIANGLE_Y, 156, 2, 2, TOL_LIT));
    const Tamper bad[] = {TAMPER_FRAGMENT_OFFSET_PAST_THE_END, TAMPER_FRAGMENT_SIZE_PAST_THE_END,
                          TAMPER_FRAGMENT_SIZE_ABOVE_THE_MAXIMUM, TAMPER_VERTEX_OFFSET_PAST_THE_END,
                          TAMPER_VERTEX_SIZE_PAST_THE_END};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        EXPECT_EQ(run_tampered(&t, bad[i]), (uint64_t)1);
        EXPECT_TRUE(pixel_is(g_pixels, SIZE, IN_TRIANGLE_X, IN_TRIANGLE_Y, 0, 0, 0, 0)); /* rien */
    }
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

/* ── Textures : un rôle, un format ── */

#define TEXTURED_MATTE                                                                             \
    "{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0},\"metallicFactor\":0,"          \
    "\"roughnessFactor\":1}}"

UTEST(eng_model_gpu, a_base_color_texture_follows_the_uv_coordinates)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Texture 2 x 2 : rouge, vert / bleu, blanc ; UV (0,0) (1,0) (0,1) aux trois sommets, donc
     * (u, v) = (x, y) en monde. Le pixel (40, 24) est dans le texel rouge, (54, 25) dans le vert,
     * (38, 9) dans le bleu : chacun est la référence CPU de sa couleur. */
    const Spec s = {.material = TEXTURED_MATTE, .texture = true};
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    const EngLighting light = test_light();
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    const EngPbrSurface red = surf(1.0F, 0.0F, 0.0F, 0.0F, 1.0F);
    const EngPbrSurface green = surf(0.0F, 1.0F, 0.0F, 0.0F, 1.0F);
    const EngPbrSurface blue = surf(0.0F, 0.0F, 1.0F, 0.0F, 1.0F);
    /* le filtrage bilinéaire mélange un peu les texels voisins : tolérance plus large, qui sépare
     * toujours nettement les trois couleurs (150 niveaux d'écart) */
    EXPECT_PIXEL_IS_PBR(&light, &red, up_z, 40U, 24U, TOL_TEXEL);
    EXPECT_PIXEL_IS_PBR(&light, &green, up_z, 54U, 25U, TOL_TEXEL);
    EXPECT_PIXEL_IS_PBR(&light, &blue, up_z, 38U, 9U, TOL_TEXEL);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, color_textures_are_decoded_from_srgb)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Texel gris 128 : en sRGB, 0,2158 en linéaire. Lu comme linéaire (128/255 = 0,502), le rendu
     * serait presque deux fois plus clair. */
    const Spec s = {.material = TEXTURED_MATTE, .texture = true, .gray_texture = true};
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    const EngLighting light = test_light();
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    const EngPbrSurface decoded = surf(0.21586F, 0.21586F, 0.21586F, 0.0F, 1.0F);
    const EngPbrSurface raw = surf(0.50196F, 0.50196F, 0.50196F, 0.0F, 1.0F);
    float position[3];
    float a[3];
    float b[3];
    pixel_world(IN_TRIANGLE_X, IN_TRIANGLE_Y, position);
    test_shade(&light, &decoded, up_z, position, a);
    test_shade(&light, &raw, up_z, position, b);
    EXPECT_GT((float)(level(b[0]) - level(a[0])), 30.0F); /* le test sait distinguer les deux */
    EXPECT_PIXEL_IS_PBR(&light, &decoded, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, the_metallic_roughness_texture_is_read_as_data_not_as_a_color)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Texel gris 128 : rugosité = G = 0,502 et métal = B = 0,502 (facteurs 1). Une lecture sRGB
     * (à tort) donnerait 0,2158 : (92, 92, 92) contre (104, 104, 104) selon Python. */
    const Spec s = {.material = "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1],"
                                "\"metallicFactor\":1,\"roughnessFactor\":1,"
                                "\"metallicRoughnessTexture\":{\"index\":0}}}",
                    .texture = true,
                    .gray_texture = true};
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 92, 92, 92, TOL_LIT);
    const EngLighting light = test_light();
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    const EngPbrSurface surface = surf(0.8F, 0.8F, 0.8F, 0.50196F, 0.50196F);
    EXPECT_PIXEL_IS_PBR(&light, &surface, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    /* les facteurs multiplient la texture : métal 0, rugosité 0,5 -> métal 0, rugosité 0,251 */
    const Spec dielectric = {.material =
                                 "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1],"
                                 "\"metallicFactor\":0,\"roughnessFactor\":0.5,"
                                 "\"metallicRoughnessTexture\":{\"index\":0}}}",
                             .texture = true,
                             .gray_texture = true};
    ASSERT_TRUE(draw_spec(&t, &dielectric, NULL));
    const EngPbrSurface product = surf(0.8F, 0.8F, 0.8F, 0.0F, 0.25098F);
    EXPECT_PIXEL_IS_PBR(&light, &product, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, a_normal_map_tilts_the_shading_normal_in_tangent_space)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Texel gris 128 -> (0,0039 ; 0,0039 ; 0,0039) en espace tangent, soit la direction
     * (1, 1, 1) / racine 3 = (0,577 ; 0,577 ; 0,577). Tangente du fichier (1,0,0), w = +1 :
     * B = cross(N, T) = +Y. Normale monde (0,577 ; 0,577 ; 0,577), donc plus de soleil :
     * (246, 246, 246) au lieu de (158, 158, 158) sans normal map (Python). */
    const float tangents[12] = {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1};
    const char *base = "\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,1,1,1],"
                       "\"metallicFactor\":0,\"roughnessFactor\":0.5}";
    char with_map[512];
    SDL_snprintf(with_map, sizeof with_map, "{%s,\"normalTexture\":{\"index\":0}}", base);
    Spec s = {
        .material = with_map, .texture = true, .gray_texture = true, .tangent_values = tangents};
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 246, 246, 246, TOL_LIT);
    const EngLighting light = test_light();
    const EngPbrSurface white = surf(1.0F, 1.0F, 1.0F, 0.0F, 0.5F);
    const float tilted[3] = {0.57735F, 0.57735F, 0.57735F};
    EXPECT_PIXEL_IS_PBR(&light, &white, tilted, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);

    /* échelle 0,5 : x et y divisés par deux -> (238, 238, 238) */
    char scaled[512];
    SDL_snprintf(scaled, sizeof scaled, "{%s,\"normalTexture\":{\"index\":0,\"scale\":0.5}}", base);
    s.material = scaled;
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 238, 238, 238, TOL_LIT);

    /* la même géométrie SANS normal map ignore les tangentes : normale du sommet, (158, 158, 158)
     */
    char flat[512];
    SDL_snprintf(flat, sizeof flat, "{%s}", base);
    s.material = flat;
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 158, 158, 158, TOL_LIT);

    /* w = -1 retourne la bitangente : la normale devient (0,577 ; -0,577 ; 0,577) */
    const float mirrored[12] = {1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, -1};
    s.material = with_map;
    s.tangent_values = mirrored;
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    const float tilted_down[3] = {0.57735F, -0.57735F, 0.57735F};
    EXPECT_PIXEL_IS_PBR(&light, &white, tilted_down, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, a_normal_map_without_file_tangents_uses_the_computed_ones)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* UV du triangle de référence : T = (1,0,0), w = -1 (v croît vers +Y, voir test_model.c) :
     * B = cross(N, T) x w = -Y. Normale monde (0,577 ; -0,577 ; 0,577). */
    const Spec s = {.material = "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,1,1,1],"
                                "\"metallicFactor\":0,\"roughnessFactor\":0.5},"
                                "\"normalTexture\":{\"index\":0}}",
                    .texture = true,
                    .gray_texture = true};
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    const EngLighting light = test_light();
    const EngPbrSurface white = surf(1.0F, 1.0F, 1.0F, 0.0F, 0.5F);
    const float tilted[3] = {0.57735F, -0.57735F, 0.57735F};
    EXPECT_PIXEL_IS_PBR(&light, &white, tilted, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

/* Éclairage par image : le shader lit l'irradiance, la préfiltrée et la LUT d'un VRAI ciel. La
 * référence est eng_pbr_shade_ibl sur des tableaux CPU calculés avec les mêmes fonctions et les
 * mêmes réglages que eng_environment_create (ciel 32, irradiance 8, préfiltré 32 à 2 niveaux).
 * Écarts attendus : demi-flottants du GPU, LUT à 512 échantillons (moteur) contre 1024 (test),
 * filtrage inter-faces du GPU (le CPU reste dans la face). */
UTEST(eng_model_gpu, an_image_based_environment_matches_the_cpu_reference)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngSkyDesc sky = {.zenith = {0.1F, 0.3F, 0.9F},
                      .horizon = {0.8F, 0.7F, 0.6F},
                      .ground = {0.4F, 0.2F, 0.05F},
                      .sun_direction = {0.0F, 1.0F, 0.0F},
                      .sun_radiance = {0, 0, 0},
                      .sun_angular_radius_deg = 5.0F};
    EngEnvironmentDesc desc = {
        .sky = &sky, .sky_size = 32, .irradiance_size = 8, .prefiltered_size = 32, .samples = 64};
    EngEnvironment env;
    ASSERT_TRUE(eng_environment_create(&env, &t.r, &desc));
    ASSERT_EQ(env.prefiltered_levels, (uint32_t)2); /* 32 puis 16 */
    /* tableaux statiques (pas d'allocation à libérer si une ASSERT sort du test) */
    static float src[32768];
    static float irr[2048];
    static float pre[32768];
    static float lut[ENG_IBL_BRDF_LUT_SIZE * ENG_IBL_BRDF_LUT_SIZE * 2U];
    ASSERT_TRUE(eng_ibl_cube_floats(32U, 6U) <= 32768U && eng_ibl_cube_floats(8U, 1U) <= 2048U &&
                eng_ibl_cube_floats(32U, 2U) <= 32768U);
    memset(src, 0, sizeof src);
    memset(irr, 0, sizeof irr);
    memset(pre, 0, sizeof pre);
    eng_ibl_make_sky(src, 32U, 6U, &sky);
    eng_ibl_build_mips(src, 32U, 6U);
    eng_ibl_irradiance(src, 32U, 6U, irr, 8U);
    eng_ibl_prefilter(src, 32U, 6U, pre, 32U, 2U, 64U);
    eng_ibl_brdf_lut(lut, ENG_IBL_BRDF_LUT_SIZE, 1024U);
    const EngIbl ibl = {irr, 8U, pre, 32U, 2U, lut};
    EngLighting light = test_light();
    light.ambient_intensity = 1.0F;
    g_env = &env;
    /* le 5e (diélectrique, normale inclinée à 45° vers le haut) distingue l'irradiance lue le long
     * de la normale de celle lue le long de la réflexion (ici le zénith) : écart de rouge ~40
     * niveaux */
    const float cases[5][3] = {{0.0F, 0.70711F, 0.70711F},
                               {0.0F, -0.70711F, 0.70711F},
                               {0.0F, 0.0F, 1.0F},
                               {0.6F, 0.0F, 0.8F},
                               {0.0F, 0.70711F, 0.70711F}};
    const float params[5][2] = {
        {1.0F, 0.3F}, {1.0F, 0.3F}, {0.5F, 0.6F}, {0.0F, 0.8F}, {0.0F, 0.5F}};
    for (int i = 0; i < 5; i++) {
        char json[256];
        factors_material(json, sizeof json, 0.8F, 0.6F, 0.2F, params[i][0], params[i][1]);
        Spec s = {.material = json};
        s.normal[0] = cases[i][0];
        s.normal[1] = cases[i][1];
        s.normal[2] = cases[i][2];
        ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &light));
        const EngPbrSurface surface = surf(0.8F, 0.6F, 0.2F, params[i][0], params[i][1]);
        float position[3];
        float expected[3];
        pixel_world(IN_TRIANGLE_X, IN_TRIANGLE_Y, position);
        eng_pbr_shade_ibl(&light, &ibl, &surface, cases[i], position, expected);
        const uint8_t *got = g_pixels + ((((size_t)IN_TRIANGLE_Y * SIZE) + IN_TRIANGLE_X) * 4);
        for (int c = 0; c < 3; c++) {
            EXPECT_NEAR((float)got[c], (float)level(expected[c]), 6.0F);
        }
    }
    /* le test est discriminant : un métal qui reflète le ciel (normale inclinée vers le haut) et un
     * métal qui reflète le sol (inclinée vers le bas) donnent des bleus très différents */
    float up[3];
    float down[3];
    float position[3];
    pixel_world(IN_TRIANGLE_X, IN_TRIANGLE_Y, position);
    const EngPbrSurface metal = surf(1.0F, 1.0F, 1.0F, 1.0F, 0.3F);
    eng_pbr_shade_ibl(&light, &ibl, &metal, cases[0], position, up);
    eng_pbr_shade_ibl(&light, &ibl, &metal, cases[1], position, down);
    EXPECT_GT(fabsf(up[2] - down[2]) * 255.0F, 20.0F);
    g_env = NULL;
    eng_environment_destroy(&env, &t.r);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, setting_an_environment_replaces_the_default_sky)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const float rgb[3] = {0.5F, 0.5F, 0.5F};
    EngEnvironment env;
    ASSERT_TRUE(eng_environment_create_uniform(&env, &t.r, rgb));
    g_env = &env;
    EngLighting light = test_light();
    light.sun_intensity = 0.0F;
    light.ambient_intensity = 1.0F;
    const Spec s = {.material = RED_MATERIAL};
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &light));
    /* rouge mat, ciel uniforme de radiance 0,5 : 0,5 de diffus + 0,5 x (0,04 x A + B) ~ 0,016 de
     * spéculaire : (132, 4, 4) ; le ciel blanc par défaut donnerait (255, 8, 8) */
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 132, 4, 4, 3);
    g_env = NULL;
    /* un environnement vide est refusé et le modèle n'est pas modifié */
    EngEnvironment empty = {0};
    EXPECT_FALSE(eng_environment_is_valid(&empty, &t.r));
    eng_environment_destroy(&env, &t.r);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, occlusion_dims_the_ambient_light_only)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Ambiante seule (pas de soleil) : occlusion = 1 + force x (R - 1) avec R = 0,502 : 0,502 à
     * force 1 -> (31, 31, 31), 0,751 à force 0,5 -> (47, 47, 47) (Python) */
    EngLighting ambient_only = test_light();
    ambient_only.sun_intensity = 0.0F;
    const char *matte = "\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1],"
                        "\"metallicFactor\":0,\"roughnessFactor\":1}";
    char json[512];
    SDL_snprintf(json, sizeof json, "{%s,\"occlusionTexture\":{\"index\":0}}", matte);
    Spec s = {.material = json, .texture = true, .gray_texture = true};
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &ambient_only));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 31, 31, 31, TOL_LIT);
    SDL_snprintf(json, sizeof json, "{%s,\"occlusionTexture\":{\"index\":0,\"strength\":0.5}}",
                 matte);
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &ambient_only));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 47, 47, 47, TOL_LIT);

    /* avec le soleil, seule l'ambiante baisse : le direct est le même avec ou sans occlusion */
    const EngLighting both = test_light();
    EngPbrSurface occluded = surf(0.8F, 0.8F, 0.8F, 0.0F, 1.0F);
    occluded.occlusion = 0.50196F;
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    SDL_snprintf(json, sizeof json, "{%s,\"occlusionTexture\":{\"index\":0}}", matte);
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &both));
    EXPECT_PIXEL_IS_PBR(&both, &occluded, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    EngPbrSurface open = occluded;
    open.occlusion = 1.0F;
    float position[3];
    float with[3];
    float without[3];
    pixel_world(IN_TRIANGLE_X, IN_TRIANGLE_Y, position);
    test_shade(&both, &occluded, up_z, position, with);
    test_shade(&both, &open, up_z, position, without);
    EXPECT_GT((float)(level(without[0]) - level(with[0])), 10.0F); /* l'occlusion se voit */
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, emissive_is_added_after_the_lighting_and_its_texture_is_srgb)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Base noire, ni soleil ni ambiante : il ne reste que l'émissif = facteur x texture. Facteur
     * (1 ; 0,5 ; 0), texel gris 128 décodé sRGB (0,2158) : (0,2158 ; 0,1079 ; 0) -> (55, 28, 0), à
     * la main. Lu comme linéaire, (128, 64, 0). */
    EngLighting dark = test_light();
    dark.sun_intensity = 0.0F;
    dark.ambient_intensity = 0.0F;
    Spec s = {.material = "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0,0,0,1],"
                          "\"metallicFactor\":0,\"roughnessFactor\":1},"
                          "\"emissiveFactor\":[1,0.5,0],\"emissiveTexture\":{\"index\":0}}",
              .texture = true,
              .gray_texture = true};
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &dark));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 55, 28, 0, 2);
    /* sans texture, le facteur seul compte : (1 ; 0,5 ; 0) -> (255, 128, 0) */
    s.material = "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0,0,0,1],\"metallicFactor\":0,"
                 "\"roughnessFactor\":1},\"emissiveFactor\":[1,0.5,0]}";
    s.texture = false;
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &dark));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 255, 128, 0, 2);
    /* KHR_materials_emissive_strength x 0,5 : (0,5 ; 0,25 ; 0) -> (128, 64, 0) */
    s.extra_top = "\"extensionsUsed\":[\"KHR_materials_emissive_strength\"]";
    s.material = "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0,0,0,1],\"metallicFactor\":0,"
                 "\"roughnessFactor\":1},\"emissiveFactor\":[1,0.5,0],"
                 "\"extensions\":{\"KHR_materials_emissive_strength\":{\"emissiveStrength\":0.5}}}";
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &dark));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 128, 64, 0, 2);
    /* l'émissif s'ajoute à l'éclairage, il ne le remplace pas : base blanche + lumière + émissif
     * (0,1 ; 0 ; 0) = référence CPU */
    s.extra_top = NULL;
    s.material = "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.5,0.5,0.5,1],"
                 "\"metallicFactor\":0,\"roughnessFactor\":1},\"emissiveFactor\":[0.1,0,0]}";
    const EngLighting light = test_light();
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &light));
    EngPbrSurface glowing = surf(0.5F, 0.5F, 0.5F, 0.0F, 1.0F);
    glowing.emissive[0] = 0.1F;
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    EXPECT_PIXEL_IS_PBR(&light, &glowing, up_z, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, metal_and_roughness_come_from_the_blue_and_green_channels)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Texture 2 x 2 rouge / vert, bleu / blanc comme texture métal-rugosité (facteurs 1) : un texel
     * rouge donne (métal B = 0, rugosité G = 0), un vert (0, 1), un bleu (1, 0). Une texture grise
     * ne distinguerait pas les canaux. Pixels proches des centres de texels. */
    const Spec s = {.material = "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1],"
                                "\"metallicFactor\":1,\"roughnessFactor\":1,"
                                "\"metallicRoughnessTexture\":{\"index\":0}}}",
                    .texture = true};
    /* soleil en +Z, caméra à la verticale du texel vert : là, H = N, et la rugosité décide du pic
     * (rugosité 1 : terne ; la rugosité lue dans un autre canal, 0 : blanc) */
    EngLighting light = test_light();
    light.sun_direction[0] = 0.0F;
    light.sun_direction[1] = 0.0F;
    light.sun_direction[2] = 1.0F;
    float over[3];
    pixel_world(55U, 24U, over);
    light.camera_position[0] = over[0];
    light.camera_position[1] = over[1];
    light.camera_position[2] = 5.0F;
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &light));
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    const EngPbrSurface red = surf(0.8F, 0.8F, 0.8F, 0.0F, 0.0F);
    const EngPbrSurface green = surf(0.8F, 0.8F, 0.8F, 0.0F, 1.0F);
    const EngPbrSurface blue = surf(0.8F, 0.8F, 0.8F, 1.0F, 0.0F);
    EXPECT_PIXEL_IS_PBR(&light, &red, up_z, 40U, 24U, TOL_TEXEL);
    EXPECT_PIXEL_IS_PBR(&light, &green, up_z, 55U, 24U, TOL_TEXEL);
    EXPECT_PIXEL_IS_PBR(&light, &blue, up_z, 39U, 8U, TOL_TEXEL);
    /* les trois cas se distinguent (le test est discriminant) */
    float position[3];
    float a[3];
    float b[3];
    pixel_world(55U, 24U, position);
    test_shade(&light, &green, up_z, position, a);
    const EngPbrSurface swapped = surf(0.8F, 0.8F, 0.8F, 1.0F, 0.0F); /* G et B échangés */
    test_shade(&light, &swapped, up_z, position, b);
    EXPECT_GT((float)abs(level(a[0]) - level(b[0])), 40.0F);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, occlusion_comes_from_the_red_channel)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Ambiante seule, texture 2 x 2 rouge / vert, bleu / blanc en occlusion : R vaut 1 sur le texel
     * rouge (aucune occlusion) et 0 sur le vert (ambiante éteinte). */
    EngLighting ambient_only = test_light();
    ambient_only.sun_intensity = 0.0F;
    const Spec s = {.material = "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1],"
                                "\"metallicFactor\":0,\"roughnessFactor\":1},"
                                "\"occlusionTexture\":{\"index\":0}}",
                    .texture = true};
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &ambient_only));
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    EngPbrSurface open = surf(0.8F, 0.8F, 0.8F, 0.0F, 1.0F);
    EngPbrSurface closed = open;
    closed.occlusion = 0.0F;
    EXPECT_PIXEL_IS_PBR(&ambient_only, &open, up_z, 40U, 24U, TOL_TEXEL);
    EXPECT_PIXEL_IS_PBR(&ambient_only, &closed, up_z, 55U, 24U, TOL_TEXEL);
    float position[3];
    float lit[3];
    pixel_world(40U, 24U, position);
    test_shade(&ambient_only, &open, up_z, position, lit);
    EXPECT_GT((float)level(lit[0]), 40.0F); /* le texel rouge est bien éclairé : discriminant */
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, a_file_tangent_that_is_not_orthogonal_is_projected_in_the_shader)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Tangente (1, 0, 1, 1) du fichier sur une normale +Z : le chargeur ne fait que la normaliser,
     * c'est le shader qui la projette sur le plan tangent (Gram-Schmidt) avant de s'en servir. */
    const float tangents[12] = {1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1};
    const Spec s = {.material = "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,1,1,1],"
                                "\"metallicFactor\":0,\"roughnessFactor\":0.5},"
                                "\"normalTexture\":{\"index\":0}}",
                    .texture = true,
                    .gray_texture = true,
                    .tangent_values = tangents};
    ASSERT_TRUE(draw_spec(&t, &s, NULL));
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    const float tangent[4] = {1.0F, 0.0F, 1.0F, 1.0F};
    const float sample[3] = {0.50196F, 0.50196F, 0.50196F};
    float normal[3];
    eng_pbr_perturb_normal(up_z, tangent, sample, 1.0F, normal);
    const EngLighting light = test_light();
    const EngPbrSurface white = surf(1.0F, 1.0F, 1.0F, 0.0F, 0.5F);
    EXPECT_PIXEL_IS_PBR(&light, &white, normal, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    /* sans projection, la normale serait nettement autre */
    const float raw_t[3] = {0.70711F, 0.0F, 0.70711F};
    const float raw_b[3] = {0.0F, 0.70711F, 0.0F};
    float wrong[3];
    for (int c = 0; c < 3; c++) {
        wrong[c] = (0.57735F * raw_t[c]) + (0.57735F * raw_b[c]) + (0.57735F * up_z[c]);
    }
    float position[3];
    float good[3];
    float bad[3];
    pixel_world(IN_TRIANGLE_X, IN_TRIANGLE_Y, position);
    test_shade(&light, &white, normal, position, good);
    test_shade(&light, &white, wrong, position, bad);
    EXPECT_GT((float)abs(level(good[0]) - level(bad[0])), 8.0F);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, tangents_follow_the_model_matrix_not_the_normal_matrix)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Échelle (2, 1, 1) : la normale +Z reste +Z, mais une tangente (1, 1, 0) devient (2, 1, 0) par
     * la matrice du modèle (directions de la surface) et (0,5 ; 1 ; 0) par la matrice normale
     * (inverse transposée, faite pour les normales). */
    mat4 stretch;
    glm_scale_make(stretch, (vec3){2.0F, 1.0F, 1.0F});
    const float tangents[12] = {1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1};
    const Spec s = {.material = "{\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,1,1,1],"
                                "\"metallicFactor\":0,\"roughnessFactor\":0.5},"
                                "\"normalTexture\":{\"index\":0}}",
                    .texture = true,
                    .gray_texture = true,
                    .tangent_values = tangents};
    ASSERT_TRUE(draw_spec(&t, &s, stretch));
    const float up_z[3] = {0.0F, 0.0F, 1.0F};
    const float world_tangent[4] = {2.0F, 1.0F, 0.0F, 1.0F};
    const float wrong_tangent[4] = {0.5F, 1.0F, 0.0F, 1.0F};
    const float sample[3] = {0.50196F, 0.50196F, 0.50196F};
    float normal[3];
    float wrong[3];
    eng_pbr_perturb_normal(up_z, world_tangent, sample, 1.0F, normal);
    eng_pbr_perturb_normal(up_z, wrong_tangent, sample, 1.0F, wrong);
    const EngLighting light = test_light();
    const EngPbrSurface white = surf(1.0F, 1.0F, 1.0F, 0.0F, 0.5F);
    EXPECT_PIXEL_IS_PBR(&light, &white, normal, IN_TRIANGLE_X, IN_TRIANGLE_Y, TOL_LIT);
    float position[3];
    float good[3];
    float bad[3];
    pixel_world(IN_TRIANGLE_X, IN_TRIANGLE_Y, position);
    test_shade(&light, &white, normal, position, good);
    test_shade(&light, &white, wrong, position, bad);
    EXPECT_GT((float)abs(level(good[0]) - level(bad[0])), 15.0F);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, a_perfect_mirror_has_a_finite_peak_thanks_to_the_roughness_floor)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* Rugosité 0, soleil et caméra à la verticale du pixel : H = N. Sans plancher (0,045), alpha
     * = 0 donne D = 0 / 0 = NaN, écrit comme noir ; avec lui D est énorme mais fini : blanc. */
    EngLighting light = test_light();
    light.sun_direction[0] = 0.0F;
    light.sun_direction[1] = 0.0F;
    light.sun_direction[2] = 1.0F;
    light.ambient_intensity = 0.0F;
    float over[3];
    pixel_world(IN_TRIANGLE_X, IN_TRIANGLE_Y, over);
    light.camera_position[0] = over[0];
    light.camera_position[1] = over[1];
    light.camera_position[2] = 5.0F;
    char json[256];
    factors_material(json, sizeof json, 0.8F, 0.8F, 0.8F, 1.0F, 0.0F);
    const Spec s = {.material = json};
    ASSERT_TRUE(draw_spec_lit(&t, &s, NULL, &light));
    EXPECT_PIXEL_LEVELS(IN_TRIANGLE_X, IN_TRIANGLE_Y, 255, 255, 255, 0);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_model_gpu, creation_failures_leave_no_gpu_resource_behind)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    /* image 0 : couleur de base ET métal/rugosité (une texture sRGB et une linéaire) ; image 1 :
     * normale seulement (linéaire) */
    const Spec s = {.material = "{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0},"
                                "\"metallicRoughnessTexture\":{\"index\":0}},"
                                "\"normalTexture\":{\"index\":1,\"scale\":0.5}}",
                    .texture = true};
    const size_t size = build(&s, t.file, sizeof t.file, NULL, NULL);
    EngModel model;
    ASSERT_TRUE(eng_model_parse(&t.level, &t.f.scratch, t.file, size, NULL, &model));
    EngModelGpu gpu;

    uint8_t storage[16];
    EngArena tiny;
    eng_arena_init_buffer(&tiny, "tiny", storage, sizeof storage);
    EXPECT_FALSE(eng_model_gpu_create(&gpu, &t.r, &tiny, &model, 0.0F));
    EXPECT_TRUE(gpu.parts == NULL);

    EngModel empty = {0};
    EXPECT_FALSE(eng_model_gpu_create(&gpu, &t.r, &t.permanent, &empty, 0.0F));

    /* un modèle correct passe, et sa destruction (deux fois) rend tout */
    ASSERT_TRUE(eng_model_gpu_create(&gpu, &t.r, &t.permanent, &model, 16.0F));
    EXPECT_EQ(gpu.part_count, (uint32_t)1);
    EXPECT_EQ(gpu.material_count, (uint32_t)2);
    const EngModelGpuMaterial *m = &gpu.materials[1];
    /* les cinq textures sont valides ; sans image, c'est la blanche (celle du matériau 0) */
    EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, m->texture));
    EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, m->metallic_roughness_texture));
    EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, m->normal_texture));
    EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, m->occlusion_texture));
    EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, m->emissive_texture));
    EXPECT_EQ(m->occlusion_texture.handle.index, gpu.white.handle.index);
    EXPECT_EQ(m->emissive_texture.handle.index, gpu.white.handle.index);
    EXPECT_TRUE(m->has_normal_map);
    EXPECT_FALSE(gpu.materials[0].has_normal_map);
    EXPECT_NEAR(m->normal_scale, 0.5F, 1e-6F);
    /* la même image sert deux familles : deux textures GPU distinctes */
    EXPECT_FALSE(m->texture.handle.index == m->metallic_roughness_texture.handle.index);
    EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, gpu.textures[0]));
    EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, gpu.linear_textures[0]));
    /* l'image 1 ne sert que de normale : pas de version sRGB */
    EXPECT_FALSE(eng_renderer_texture_is_valid(&t.r, gpu.textures[1]));
    EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, gpu.linear_textures[1]));
    EXPECT_EQ(eng_renderer_texture_mip_levels(&t.r, m->texture), (uint32_t)2);
    EXPECT_EQ(eng_renderer_texture_mip_levels(&t.r, gpu.materials[0].texture), (uint32_t)1);
    EXPECT_NEAR(eng_renderer_sampler_anisotropy(&t.r, gpu.sampler), 16.0F, 1e-5F);
    EXPECT_NEAR(gpu.bounds_max[1], 1.0F, 1e-5F);
    eng_model_gpu_destroy(&gpu, &t.r);
    eng_model_gpu_destroy(&gpu, &t.r);
    eng_model_free(&model);
    CLOSE_MODEL(t);
}

/* Hors validation : sans OW_HELMET (chemin d'un .gltf) et OW_DUMP (dossier), ne fait rien. Sinon
 * rend le modèle sous quatre vues / soleils en .ppm linéaire, pour la comparaison avec three.js
 * (tools/reference/README.md). */
UTEST(eng_model_gpu, dump_a_model_for_the_reference_viewer)
{
    const char *gltf = getenv("OW_HELMET");
    const char *dir = getenv("OW_DUMP");
    if (gltf == NULL || dir == NULL) {
        return;
    }
    TestModel t;
    if (!open_model(&t)) {
        return;
    }
    EngArena big_level;
    EngArena big_scratch;
    EngArena big_perm;
    ASSERT_TRUE(eng_arena_init(&big_level, "h-level", (size_t)96 << 20));
    ASSERT_TRUE(eng_arena_init(&big_scratch, "h-scratch", (size_t)96 << 20));
    ASSERT_TRUE(eng_arena_init(&big_perm, "h-perm", (size_t)4 << 20));
    EngModel model;
    ASSERT_TRUE(eng_model_load(&big_level, &big_scratch, gltf, &model));
    EngModelGpu gpu;
    ASSERT_TRUE(eng_model_gpu_create(&gpu, &t.r, &big_perm, &model, 16.0F));
    fprintf(stderr, "HELMET verts=%u parts=%u mats=%u images=%u bounds=(%f %f %f)-(%f %f %f)\n",
            model.vertex_count, model.part_count, model.material_count, model.image_count,
            model.bounds_min[0], model.bounds_min[1], model.bounds_min[2], model.bounds_max[0],
            model.bounds_max[1], model.bounds_max[2]);
    eng_model_free(&model);
    /* OW_ENV (un .hdr équirectangulaire) : éclairage par image seul, sans soleil (ambiante 1) ;
     * OW_ENV_SKY : taille du ciel (défaut 128), le préfiltré en prend la moitié. */
    const char *hdr_path = getenv("OW_ENV");
    EngEnvironment env_gpu = {0};
    float *hdr = NULL;
    if (hdr_path != NULL) {
        uint32_t hw;
        uint32_t hh;
        ASSERT_TRUE(eng_environment_load_hdr(hdr_path, &hdr, &hw, &hh));
        const char *sky_text = getenv("OW_ENV_SKY");
        const uint32_t sky_size = sky_text != NULL ? (uint32_t)atoi(sky_text) : 128U;
        const EngEnvironmentDesc desc = {.equirect_rgb = hdr,
                                         .equirect_width = hw,
                                         .equirect_height = hh,
                                         .sky_size = sky_size,
                                         .irradiance_size = 32,
                                         .prefiltered_size = sky_size / 2U,
                                         .samples = 512};
        ASSERT_TRUE(eng_environment_create(&env_gpu, &t.r, &desc));
        ASSERT_TRUE(eng_model_gpu_set_environment(&gpu, &t.r, &env_gpu));
    }
    /* OW_UNIFORM : ciel uniforme de radiance 1 (le défaut d'un modèle), ambiante 1, sans soleil */
    const bool uniform_sky = getenv("OW_UNIFORM") != NULL;
    static uint8_t big[512 * 512 * 4];
    const struct {
        float eye[3];
        float sun[3];
        float intensity;
    } cases[4] = {{{2.2F, 0.6F, 3.2F}, {0.4F, 0.5F, 0.75F}, 1.5F},
                  {{-2.5F, 0.3F, 2.8F}, {-0.7F, 0.3F, 0.65F}, 1.5F},
                  {{0.0F, 0.2F, 4.0F}, {0.0F, 0.9F, 0.45F}, 1.5F},
                  {{3.0F, 1.0F, -2.0F}, {0.5F, 0.6F, -0.6F}, 1.5F}};
    for (int i = 0; i < 4; i++) {
        mat4 proj;
        mat4 view;
        mat4 vp;
        mat4 model_m;
        glm_perspective(glm_rad(40.0F), 1.0F, 0.1F, 50.0F, proj);
        glm_lookat((float *)cases[i].eye, (float[3]){0, 0, -0.187F}, (float[3]){0, 1, 0}, view);
        glm_mat4_mul(proj, view, vp);
        glm_mat4_identity(model_m);
        EngLighting l = {0};
        float len =
            sqrtf((cases[i].sun[0] * cases[i].sun[0]) + (cases[i].sun[1] * cases[i].sun[1]) +
                  (cases[i].sun[2] * cases[i].sun[2]));
        for (int c = 0; c < 3; c++) {
            l.sun_direction[c] = cases[i].sun[c] / len;
            l.sun_color[c] = 1.0F;
            l.ambient_color[c] = 1.0F;
            l.camera_position[c] = cases[i].eye[c];
        }
        l.sun_intensity = (hdr != NULL || uniform_sky) ? 0.0F : cases[i].intensity;
        l.ambient_intensity = (hdr != NULL || uniform_sky) ? 1.0F : 0.0F;
        EngRenderQueue *q = eng_renderer_begin_frame(&t.r);
        (void)eng_model_gpu_draw(&gpu, q, t.pipeline, model_m, vp, &l);
        fprintf(stderr, "HELMET case %d: queued %u dropped %u\n", i, q->count, q->dropped);
        DrawContext c = {&t.r};
        ASSERT_TRUE(render_to_pixels_depth(&t.f, eng_renderer_depth_format(&t.r), execute_queue, &c,
                                           512, big));
        char path[512];
        SDL_snprintf(path, sizeof path, "%s/engine_%d.ppm", dir, i);
        FILE *out = fopen(path, "wb");
        if (out != NULL) {
            fprintf(out, "P6\n512 512\n255\n");
            for (int p = 0; p < 512 * 512; p++) {
                fwrite(big + ((size_t)p * 4U), 1, 3, out);
            }
            fclose(out);
        }
    }
    eng_model_gpu_destroy(&gpu, &t.r);
    if (hdr != NULL) {
        eng_environment_destroy(&env_gpu, &t.r);
        eng_environment_free_hdr(hdr);
    }
    eng_arena_shutdown(&big_perm);
    eng_arena_shutdown(&big_scratch);
    eng_arena_shutdown(&big_level);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

#endif /* OW_SHADER_DIR */
