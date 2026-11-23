#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/files.h"
#include "engine/mesh_file.h"
#include "engine/model.h"
#include "engine/model_gpu.h"
#include "engine/tex_file.h"
#include "test_gltf.h"
#include "test_render_util.h"
#include "utest.h"

/* Rendu identique (M2-02) : un modèle chargé depuis le glTF et le MÊME modèle cuit puis rechargé
 * depuis le disque (.mesh + .tex) sont dessinés avec les mêmes shaders, matrices et éclairage ; les
 * deux images doivent être égales OCTET POUR OCTET. Aucune tolérance : le chargeur cuit remplit le
 * même EngModel que le chargeur glTF, le GPU reçoit donc exactement les mêmes données. Sans GPU ou
 * sans shaders compilés, ces tests s'arrêtent là. */

#ifdef OW_SHADER_DIR

#define SIZE 64U

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
    if (!eng_arena_init(&t->permanent, "cook-perm", (size_t)8 << 20) ||
        !eng_arena_init(&t->level, "cook-level", (size_t)4 << 20)) {
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

#define CLOSE_MODEL(t) EXPECT_EQ(close_model(&(t)), (uint32_t)0)

typedef struct DrawContext {
    EngRenderer *renderer;
} DrawContext;

static void execute_queue(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass, void *context)
{
    DrawContext *c = (DrawContext *)context;
    (void)eng_renderer_execute(c->renderer, &c->renderer->queue, commands, pass);
}

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

/* Dessine `model` (envoyé au GPU puis détruit) et lit les pixels dans `pixels`. */
static bool render_model(TestModel *t, const EngModel *model, mat4 model_matrix, uint8_t *pixels)
{
    EngModelGpu gpu;
    if (!eng_model_gpu_create(&gpu, &t->r, &t->permanent, model, 0.0F)) {
        return false;
    }
    mat4 identity;
    glm_mat4_identity(identity);
    const EngLighting light = test_light();
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    const uint32_t accepted =
        eng_model_gpu_draw(&gpu, q, t->pipeline, model_matrix, identity, &light);
    DrawContext c = {&t->r};
    const bool ok = accepted == gpu.part_count &&
                    render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue,
                                           &c, SIZE, pixels);
    eng_model_gpu_destroy(&gpu, &t->r);
    return ok;
}

static bool write_file(const char *relative, const void *data, size_t size)
{
    char path[512];
    return eng_path_resolve(path, sizeof path, relative) && SDL_SaveFile(path, data, size);
}

/* Cuit `source` dans mesh_gpu_test/<stem>.mesh (+ ses .tex), comme le fait tools/assetc, puis le
 * recharge dans `loaded`. */
static bool cook_and_reload(TestModel *t, const EngModel *source, const char *stem, bool compress,
                            EngModel *loaded)
{
    char dir[512];
    if (!eng_path_resolve(dir, sizeof dir, "mesh_gpu_test") || !SDL_CreateDirectory(dir)) {
        return false;
    }
    EngMeshPlan plan;
    if (!eng_mesh_plan(&t->level, source, stem, &plan)) {
        return false;
    }
    const void *file = NULL;
    size_t size = 0;
    char path[512];
    SDL_snprintf(path, sizeof path, "mesh_gpu_test/%s.mesh", stem);
    if (!eng_mesh_encode(&t->level, &t->f.scratch, &plan.model, plan.textures, plan.texture_count,
                         &file, &size) ||
        !write_file(path, file, size)) {
        return false;
    }
    for (uint32_t i = 0; i < plan.texture_count; i++) {
        const void *tex = NULL;
        size_t tex_size = 0;
        SDL_snprintf(path, sizeof path, "mesh_gpu_test/%s", plan.textures[i].path);
        const EngImage *image = &source->images[plan.source_images[i]];
        const bool encoded = compress ? eng_tex_encode_bc7(&t->level, &t->f.scratch, image,
                                                           plan.textures[i].srgb, &tex, &tex_size)
                                      : eng_tex_encode(&t->level, &t->f.scratch, image,
                                                       plan.textures[i].srgb, &tex, &tex_size);
        if (!encoded || !write_file(path, tex, tex_size)) {
            return false;
        }
    }
    SDL_snprintf(path, sizeof path, "mesh_gpu_test/%s.mesh", stem);
    return eng_mesh_load(&t->level, &t->f.scratch, path, loaded);
}

static uint32_t count_lit_pixels(const uint8_t *pixels)
{
    uint32_t lit = 0;
    for (uint32_t i = 0; i < SIZE * SIZE; i++) {
        const uint8_t *px = pixels + (size_t)i * 4U;
        if (px[0] != 0U || px[1] != 0U || px[2] != 0U) {
            lit++;
        }
    }
    return lit;
}

static uint8_t g_gltf[SIZE * SIZE * 4];
static uint8_t g_cooked[SIZE * SIZE * 4];

/* Matériau qui utilise la MÊME image dans les deux espaces de couleur : couleur de base (sRGB) et
 * normale (linéaire), plus l'émissif (sRGB) : le cuiseur en tire deux textures. Le GPU choisit le
 * format d'après le RÔLE de la texture dans le matériau pour les images RGBA8 ; pour les images BC7
 * (M2-03) l'espace de couleur est celui du fichier et doit correspondre au rôle. */
#define MULTI_TEXTURE_MATERIAL                                                                     \
    "{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0},\"metallicFactor\":0.2,"        \
    "\"roughnessFactor\":0.6},\"normalTexture\":{\"index\":0,\"scale\":0.7},"                      \
    "\"emissiveTexture\":{\"index\":0},\"emissiveFactor\":[0.3,0.2,0.1]}"

UTEST(eng_mesh_file_gpu, a_cooked_textured_model_renders_like_the_gltf)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const Spec spec = {.material = MULTI_TEXTURE_MATERIAL, .texture = true};
    const size_t size = build(&spec, t.file, sizeof t.file, NULL, NULL);
    EngModel source;
    ASSERT_TRUE(eng_model_parse(&t.level, &t.f.scratch, t.file, size, NULL, &source));
    ASSERT_TRUE(source.image_count >= 1);

    EngModel loaded;
    ASSERT_TRUE(cook_and_reload(&t, &source, "tri", false, &loaded));
    ASSERT_EQ(loaded.image_count, (uint32_t)2); /* une image, deux espaces de couleur */

    mat4 identity;
    glm_mat4_identity(identity);
    ASSERT_TRUE(render_model(&t, &source, identity, g_gltf));
    ASSERT_TRUE(render_model(&t, &loaded, identity, g_cooked));
    EXPECT_GT(count_lit_pixels(g_gltf), (uint32_t)300); /* le triangle est bien dessiné */
    EXPECT_EQ(memcmp(g_gltf, g_cooked, sizeof g_gltf), 0);

    /* témoin : le banc voit une différence quand les texels changent (sinon l'égalité ci-dessus ne
     * prouverait rien) */
    for (uint32_t i = 0; i < loaded.image_count; i++) {
        for (uint32_t k = 0; k < loaded.images[i].width * loaded.images[i].height * 4U; k++) {
            loaded.images[i].pixels[k] ^= 0x5AU;
        }
    }
    static uint8_t other[SIZE * SIZE * 4];
    ASSERT_TRUE(render_model(&t, &loaded, identity, other));
    EXPECT_NE(memcmp(g_gltf, other, sizeof g_gltf), 0);

    eng_model_free(&loaded);
    eng_model_free(&source);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_mesh_file_gpu, a_cooked_box_renders_like_the_source)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const float lo[3] = {-0.4F, -0.4F, 0.3F};
    const float hi[3] = {0.4F, 0.4F, 0.7F};
    EngModelMaterial material = {.base_color = {0.8F, 0.3F, 0.2F, 1.0F},
                                 .image = -1,
                                 .metallic = 0.1F,
                                 .roughness = 0.5F,
                                 .metallic_roughness_image = -1,
                                 .normal_scale = 1.0F,
                                 .normal_image = -1,
                                 .occlusion_strength = 1.0F,
                                 .occlusion_image = -1,
                                 .emissive_image = -1};
    EngModel source;
    ASSERT_TRUE(eng_model_make_box(&t.level, lo, hi, &material, &source));
    EngModel loaded;
    ASSERT_TRUE(cook_and_reload(&t, &source, "box", false, &loaded));
    EXPECT_EQ(loaded.image_count, (uint32_t)0);

    mat4 turn;
    glm_mat4_identity(turn);
    glm_rotate_y(turn, 0.5F, turn);
    glm_rotate_x(turn, 0.3F, turn);
    ASSERT_TRUE(render_model(&t, &source, turn, g_gltf));
    ASSERT_TRUE(render_model(&t, &loaded, turn, g_cooked));
    EXPECT_GT(count_lit_pixels(g_gltf), (uint32_t)300);
    EXPECT_EQ(memcmp(g_gltf, g_cooked, sizeof g_gltf), 0);
    eng_model_free(&loaded);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

/* ── Textures compressées BC7 (M2-03) ── */

/* PSNR (dB) entre deux rendus, sur les pixels éclairés dans l'un ou l'autre (le fond noir, égal des
 * deux côtés, ne doit pas masquer l'erreur). 200 si les rendus sont identiques. */
static double psnr_lit(const uint8_t *a, const uint8_t *b)
{
    double sum = 0.0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < SIZE * SIZE; i++) {
        const uint8_t *pa = a + (size_t)i * 4U;
        const uint8_t *pb = b + (size_t)i * 4U;
        if ((pa[0] | pa[1] | pa[2] | pb[0] | pb[1] | pb[2]) == 0U) {
            continue;
        }
        for (int c = 0; c < 3; c++) {
            const double d = (double)pa[c] - (double)pb[c];
            sum += d * d;
        }
        count++;
    }
    if (count == 0 || sum == 0.0) {
        return 200.0;
    }
    return 10.0 * log10(255.0 * 255.0 / (sum / ((double)count * 3.0)));
}

/* Remplace l'image `index` du modèle par un motif lisse mais détaillé, de taille QUELCONQUE (pas un
 * multiple de 4 : les blocs de bord sont complétés) : dégradés, anneaux et un bord franc. */
static bool replace_with_pattern(EngModel *m, uint32_t index, uint32_t w, uint32_t h)
{
    EngImage image;
    if (!eng_image_create(w, h, NULL, &image)) {
        return false;
    }
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t *px = image.pixels + ((size_t)y * w + x) * 4U;
            const float fx = (float)x / (float)w;
            const float fy = (float)y / (float)h;
            const float ring = 0.5F + 0.5F * sinf(14.0F * sqrtf(((fx - 0.5F) * (fx - 0.5F)) +
                                                                ((fy - 0.5F) * (fy - 0.5F))));
            px[0] = (uint8_t)(255.0F * fx);
            px[1] = (uint8_t)(255.0F * ring);
            px[2] = x > w / 2U ? (uint8_t)200 : (uint8_t)(255.0F * fy);
            px[3] = 255;
        }
    }
    eng_image_free(&m->images[index]);
    m->images[index] = image;
    return true;
}

UTEST(eng_mesh_file_gpu, a_bc7_cooked_model_renders_close_to_the_source)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const Spec spec = {.material = MULTI_TEXTURE_MATERIAL, .texture = true};
    const size_t size = build(&spec, t.file, sizeof t.file, NULL, NULL);
    EngModel source;
    ASSERT_TRUE(eng_model_parse(&t.level, &t.f.scratch, t.file, size, NULL, &source));
    ASSERT_TRUE(replace_with_pattern(&source, 0, 37, 21));

    EngModel loaded;
    ASSERT_TRUE(cook_and_reload(&t, &source, "bc7", true, &loaded));
    ASSERT_EQ(loaded.image_count, (uint32_t)2);
    /* une texture par espace de couleur, chacune dans son format BC7 */
    EXPECT_EQ(loaded.images[0].encoding, ENG_IMAGE_BC7_SRGB);
    EXPECT_EQ(loaded.images[1].encoding, ENG_IMAGE_BC7);
    EXPECT_EQ(loaded.images[0].mip_count, eng_image_mip_count(37, 21));
    EXPECT_EQ(loaded.images[0].width, (uint32_t)37);

    mat4 identity;
    glm_mat4_identity(identity);
    ASSERT_TRUE(render_model(&t, &source, identity, g_gltf));
    ASSERT_TRUE(render_model(&t, &loaded, identity, g_cooked));
    EXPECT_GT(count_lit_pixels(g_gltf), (uint32_t)300);
    EXPECT_GT(count_lit_pixels(g_cooked), (uint32_t)300);
    const double psnr = psnr_lit(g_gltf, g_cooked);
    fprintf(stderr, "BC7 : PSNR du rendu contre la source = %.1f dB\n", psnr);
    EXPECT_GT(psnr, 30.0);
    EXPECT_LT(psnr, 200.0); /* la compression est avec perte : un rendu identique serait suspect */

    /* témoin : une texture réellement différente donne un PSNR bien plus bas, donc la mesure
     * discrimine */
    EngModel other;
    ASSERT_TRUE(cook_and_reload(&t, &source, "bc7b", true, &other));
    for (uint32_t i = 0; i < other.image_count; i++) {
        for (size_t k = 0;
             k < eng_image_encoded_chain_size(other.images[i].width, other.images[i].height,
                                              other.images[i].mip_count);
             k++) {
            other.images[i].pixels[k] ^= 0x33U;
        }
    }
    static uint8_t damaged[SIZE * SIZE * 4];
    ASSERT_TRUE(render_model(&t, &other, identity, damaged));
    EXPECT_LT(psnr_lit(g_gltf, damaged), psnr - 5.0);

    /* le rôle doit correspondre à l'espace de couleur du fichier : une image BC7 linéaire utilisée
     * en couleur de base est refusée (pas de texture de couleur fausse en silence) */
    EngModelGpu gpu;
    loaded.images[0].encoding = ENG_IMAGE_BC7;
    EXPECT_FALSE(eng_model_gpu_create(&gpu, &t.r, &t.permanent, &loaded, 0.0F));
    loaded.images[0].encoding = ENG_IMAGE_BC7_SRGB;
    loaded.images[1].encoding = ENG_IMAGE_BC7_SRGB; /* texture de données lue comme couleur */
    EXPECT_FALSE(eng_model_gpu_create(&gpu, &t.r, &t.permanent, &loaded, 0.0F));
    loaded.images[1].encoding = ENG_IMAGE_BC7;

    eng_model_free(&other);
    eng_model_free(&loaded);
    eng_model_free(&source);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

UTEST(eng_mesh_file_gpu, the_renderer_creates_bc7_textures_of_any_size_and_refuses_bad_ones)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    static const uint32_t dims[5][2] = {{4, 4}, {17, 9}, {1, 1}, {3, 130}, {64, 64}};
    for (int k = 0; k < 5; k++) {
        const uint32_t w = dims[k][0];
        const uint32_t h = dims[k][1];
        const uint32_t levels = eng_image_mip_count(w, h);
        const size_t bytes = eng_image_encoded_chain_size(w, h, levels);
        uint8_t *chain = SDL_calloc(1, bytes);
        ASSERT_TRUE(chain != NULL);
        for (size_t i = 0; i < bytes; i += 16) {
            chain[i] = 0x40; /* mode 6 : un bloc valide, tout à zéro sinon */
        }
        EngTextureDataDesc desc = {.name = "bc7-test",
                                   .width = w,
                                   .height = h,
                                   .layers = 1,
                                   .mip_levels = levels,
                                   .format = ENG_TEXTURE_BC7_SRGB,
                                   .data = chain};
        const EngTextureHandle texture = eng_renderer_create_texture_data(&t.r, &desc);
        EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, texture));
        EXPECT_EQ(eng_renderer_texture_mip_levels(&t.r, texture), levels);
        EXPECT_TRUE(eng_renderer_destroy_texture(&t.r, texture));
        desc.format = ENG_TEXTURE_BC7;
        const EngTextureHandle linear = eng_renderer_create_texture_data(&t.r, &desc);
        EXPECT_TRUE(eng_renderer_texture_is_valid(&t.r, linear));
        EXPECT_TRUE(eng_renderer_destroy_texture(&t.r, linear));
        /* refus : aucun niveau, plus de niveaux que la chaîne, données absentes */
        desc.mip_levels = 0;
        EXPECT_FALSE(
            eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture_data(&t.r, &desc)));
        desc.mip_levels = levels + 1U;
        EXPECT_FALSE(
            eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture_data(&t.r, &desc)));
        desc.mip_levels = levels;
        desc.data = NULL;
        EXPECT_FALSE(
            eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture_data(&t.r, &desc)));
        SDL_free(chain);
    }
    /* la création « avec génération de mips » ne sait pas faire du BC7 */
    const uint8_t block[16] = {0x40};
    const EngTextureDesc generated = {.name = "bc7-generated",
                                      .width = 4,
                                      .height = 4,
                                      .format = ENG_TEXTURE_BC7,
                                      .pixels = block,
                                      .mip_levels = 1};
    EXPECT_FALSE(
        eng_renderer_texture_is_valid(&t.r, eng_renderer_create_texture(&t.r, &generated)));
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t);
}

#endif /* OW_SHADER_DIR */
