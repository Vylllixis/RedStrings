#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engine/asset_cook.h"
#include "engine/asset_types.h"
#include "engine/assets.h"
#include "engine/files.h"
#include "engine/log.h"
#include "engine/mesh_file.h"
#include "engine/model.h"
#include "engine/model_gpu.h"
#include "engine/tex_file.h"
#include "test_gltf.h"
#include "test_render_util.h"
#include "utest.h"

/* Gestionnaire d'assets, côté GPU (M2-05) : un .mesh cuit est chargé PAR le gestionnaire, une seule
 * fois même demandé deux fois, dessiné comme le même modèle chargé directement (octet pour octet),
 * puis déchargé sans qu'il reste une seule ressource GPU. Sans GPU ou sans shaders compilés, ces
 * tests s'arrêtent là. */

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

static bool write_file(const char *relative, const void *data, size_t size)
{
    char path[512];
    return eng_path_resolve(path, sizeof path, relative) && SDL_SaveFile(path, data, size);
}

/* Cuit `source` dans assets_gpu_test/<stem>.mesh (+ ses .tex), comme le fait tools/assetc. */
static bool cook(TestModel *t, const EngModel *source, const char *stem)
{
    char dir[512];
    if (!eng_path_resolve(dir, sizeof dir, "assets_gpu_test") || !SDL_CreateDirectory(dir)) {
        return false;
    }
    EngMeshPlan plan;
    if (!eng_mesh_plan(&t->level, source, stem, &plan)) {
        return false;
    }
    const void *file = NULL;
    size_t size = 0;
    char path[512];
    SDL_snprintf(path, sizeof path, "assets_gpu_test/%s.mesh", stem);
    if (!eng_mesh_encode(&t->level, &t->f.scratch, &plan.model, plan.textures, plan.texture_count,
                         &file, &size) ||
        !write_file(path, file, size)) {
        return false;
    }
    for (uint32_t i = 0; i < plan.texture_count; i++) {
        const void *tex = NULL;
        size_t tex_size = 0;
        SDL_snprintf(path, sizeof path, "assets_gpu_test/%s", plan.textures[i].path);
        if (!eng_tex_encode(&t->level, &t->f.scratch, &source->images[plan.source_images[i]],
                            plan.textures[i].srgb, &tex, &tex_size) ||
            !write_file(path, tex, tex_size)) {
            return false;
        }
    }
    return true;
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

static uint8_t g_direct[SIZE * SIZE * 4];
static uint8_t g_managed[SIZE * SIZE * 4];

#define TEXTURED_MATERIAL                                                                          \
    "{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0},\"metallicFactor\":0.2,"        \
    "\"roughnessFactor\":0.6},\"normalTexture\":{\"index\":0,\"scale\":0.7}}"

/* Dessine un EngModelGpu déjà créé (qui reste à l'appelant). */
static bool render_gpu(TestModel *t, const EngModelGpu *gpu, uint8_t *pixels)
{
    mat4 identity;
    glm_mat4_identity(identity);
    const EngLighting light = test_light();
    EngRenderQueue *q = eng_renderer_begin_frame(&t->r);
    const uint32_t accepted = eng_model_gpu_draw(gpu, q, t->pipeline, identity, identity, &light);
    DrawContext c = {&t->r};
    return accepted == gpu->part_count &&
           render_to_pixels_depth(&t->f, eng_renderer_depth_format(&t->r), execute_queue, &c, SIZE,
                                  pixels);
}

UTEST(eng_assets_gpu, a_model_requested_twice_loads_once_and_unloads_without_leak)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    const Spec spec = {.material = TEXTURED_MATERIAL, .texture = true};
    const size_t size = build(&spec, t.file, sizeof t.file, NULL, NULL);
    EngModel source;
    ASSERT_TRUE(eng_model_parse(&t.level, &t.f.scratch, t.file, size, NULL, &source));
    ASSERT_TRUE(cook(&t, &source, "tri"));

    EngAssets assets;
    ASSERT_TRUE(eng_assets_init(&assets, 8));
    EngModelAssetContext context = {.renderer = &t.r};
    const uint32_t model_type = eng_model_assets_register(&assets, &context);
    ASSERT_EQ(model_type, (uint32_t)1);

    const EngAssetHandle a = eng_assets_acquire(&assets, model_type, "assets_gpu_test/tri.mesh");
    const EngAssetHandle b = eng_assets_acquire(&assets, model_type, "assets_gpu_test/tri.mesh");
    ASSERT_TRUE(a.generation != 0);
    EXPECT_TRUE(a.index == b.index && a.generation == b.generation);
    const EngAssetStats stats = eng_assets_stats(&assets);
    EXPECT_EQ(stats.loads, (uint32_t)1);
    EXPECT_EQ(stats.hits, (uint32_t)1);
    EXPECT_EQ(eng_assets_refs(&assets, a), (uint32_t)2);

    /* dépendances : le .mesh puis ses deux textures (une image, deux espaces de couleur) */
    ASSERT_EQ(eng_assets_dependency_count(&assets, a), (uint32_t)3);
    EXPECT_TRUE(strstr(eng_assets_dependency(&assets, a, 0), "assets_gpu_test/tri.mesh") != NULL);
    EXPECT_TRUE(strstr(eng_assets_dependency(&assets, a, 1), "tri.0.tex") != NULL);
    EXPECT_TRUE(strstr(eng_assets_dependency(&assets, a, 2), "tri.1.tex") != NULL);

    /* rendu identique au même modèle chargé directement */
    const EngModelGpu *managed = eng_model_asset_gpu(&assets, model_type, a);
    ASSERT_TRUE(managed != NULL);
    EngModel direct;
    ASSERT_TRUE(eng_mesh_load(&t.level, &t.f.scratch, "assets_gpu_test/tri.mesh", &direct));
    EngModelGpu direct_gpu;
    ASSERT_TRUE(eng_model_gpu_create(&direct_gpu, &t.r, &t.permanent, &direct, 0.0F));
    ASSERT_TRUE(render_gpu(&t, &direct_gpu, g_direct));
    ASSERT_TRUE(render_gpu(&t, managed, g_managed));
    EXPECT_GT(count_lit_pixels(g_direct), (uint32_t)300);
    EXPECT_EQ(memcmp(g_direct, g_managed, sizeof g_direct), 0);
    eng_model_gpu_destroy(&direct_gpu, &t.r);
    eng_model_free(&direct);

    /* mauvais type : refusé */
    EXPECT_TRUE(eng_model_asset_gpu(&assets, model_type + 1U, a) == NULL);

    EXPECT_TRUE(eng_assets_release(&assets, a));
    EXPECT_TRUE(eng_model_asset_gpu(&assets, model_type, b) != NULL); /* encore une référence */
    EXPECT_TRUE(eng_assets_release(&assets, b));
    EXPECT_TRUE(eng_model_asset_gpu(&assets, model_type, a) == NULL); /* déchargé : périmé */
    EXPECT_EQ(eng_assets_stats(&assets).unloads, (uint32_t)1);
    EXPECT_EQ(eng_assets_shutdown(&assets), (uint32_t)0);

    eng_model_free(&source);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t); /* aucune texture, aucun buffer, aucun sampler restant */
}

UTEST(eng_assets_gpu, a_broken_model_fails_cleanly_without_gpu_leak)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    EngAssets assets;
    ASSERT_TRUE(eng_assets_init(&assets, 4));
    EngModelAssetContext context = {.renderer = &t.r};
    const uint32_t model_type = eng_model_assets_register(&assets, &context);
    ASSERT_EQ(model_type, (uint32_t)1);

    EXPECT_TRUE(eng_assets_acquire(&assets, model_type, "assets_gpu_test/absent.mesh").generation ==
                0);

    /* un .mesh dont une texture a disparu : échec, et rien de créé côté GPU */
    const Spec spec = {.material = TEXTURED_MATERIAL, .texture = true};
    const size_t size = build(&spec, t.file, sizeof t.file, NULL, NULL);
    EngModel source;
    ASSERT_TRUE(eng_model_parse(&t.level, &t.f.scratch, t.file, size, NULL, &source));
    ASSERT_TRUE(cook(&t, &source, "holey"));
    char tex[512];
    ASSERT_TRUE(eng_path_resolve(tex, sizeof tex, "assets_gpu_test/holey.1.tex"));
    ASSERT_TRUE(SDL_RemovePath(tex));
    EXPECT_TRUE(eng_assets_acquire(&assets, model_type, "assets_gpu_test/holey.mesh").generation ==
                0);
    EXPECT_EQ(eng_assets_stats(&assets).failures, (uint32_t)2);
    EXPECT_EQ(eng_assets_stats(&assets).loaded, (uint32_t)0);
    EXPECT_EQ(eng_assets_shutdown(&assets), (uint32_t)0);

    eng_model_free(&source);
    CLOSE_MODEL(t);
}

/* ── Rechargement à chaud (M2-06) ── */

/* Lance `assetc` et attend sa fin. */
static bool run_assetc(const char *source, const char *output)
{
    const char *const argv[] = {OW_ASSETC_PATH, "mesh", source, output, "--raw", NULL};
    SDL_Process *process = SDL_CreateProcess(argv, false);
    if (process == NULL) {
        return false;
    }
    int code = -1;
    const bool waited = SDL_WaitProcess(process, true, &code);
    SDL_DestroyProcess(process);
    return waited && code == 0;
}

static bool write_gltf(const Spec *spec, TestModel *t, const char *path)
{
    const size_t size = build(spec, t->file, sizeof t->file, NULL, NULL);
    return size > 0 && SDL_SaveFile(path, t->file, size);
}

static double seconds(void)
{
    return (double)SDL_GetTicks() / 1000.0;
}

/* Critère d'acceptation M2-06 : une texture modifiée dans un logiciel de dessin est visible en jeu
 * en moins de 2 s. Ici : la source glTF (dont la texture est dans le fichier) est réécrite avec une
 * autre texture ; la chaîne de développement complète s'enchaîne : surveillance de la source,
 * `assetc` en processus séparé, surveillance des fichiers cuits, rechargement du modèle sur le GPU.
 * Temps mesuré de l'écriture de la source à l'image différente. */
UTEST(eng_assets_gpu, a_changed_source_texture_is_visible_within_two_seconds)
{
    TestModel t;
    if (!open_model(&t)) {
        EXPECT_FALSE(render_gpu_required());
        return;
    }
    char dir[512];
    char source[512];
    char output[512];
    ASSERT_TRUE(eng_path_resolve(dir, sizeof dir, "assets_hot_gpu"));
    ASSERT_TRUE(SDL_CreateDirectory(dir));
    SDL_snprintf(source, sizeof source, "%s/tri.gltf", dir);
    SDL_snprintf(output, sizeof output, "%s/tri.mesh", dir);

    const Spec before = {.material = TEXTURED_MATERIAL, .texture = true};
    const Spec after = {.material = TEXTURED_MATERIAL, .texture = true, .gray_texture = true};
    ASSERT_TRUE(write_gltf(&before, &t, source));
    ASSERT_TRUE(run_assetc(source, output));

    EngAssets assets;
    ASSERT_TRUE(eng_assets_init(&assets, 4));
    EngModelAssetContext context = {.renderer = &t.r};
    const uint32_t model_type = eng_model_assets_register(&assets, &context);
    const EngAssetHandle handle =
        eng_assets_acquire(&assets, model_type, "assets_hot_gpu/tri.mesh");
    ASSERT_TRUE(handle.generation != 0);
    const EngModelGpu *gpu = eng_model_asset_gpu(&assets, model_type, handle);
    ASSERT_TRUE(gpu != NULL);
    ASSERT_TRUE(render_gpu(&t, gpu, g_direct));
    EXPECT_GT(count_lit_pixels(g_direct), (uint32_t)300);

    const char *const argv[] = {OW_ASSETC_PATH, "mesh", source, output, "--raw", NULL};
    const EngCookRule rule = {.name = "tri", .source = source, .argv = argv};
    EngAssetCook cook;
    ASSERT_TRUE(eng_asset_cook_init(&cook, &rule, 1, ENG_FILE_WATCH_DEFAULT_INTERVAL));
    eng_assets_set_hot_reload(&assets, true, ENG_FILE_WATCH_DEFAULT_INTERVAL);
    (void)eng_assets_poll(&assets,
                          seconds()); /* crée les surveillances (version actuelle = référence) */

    /* un contrôle sans modification : rien ne change */
    SDL_Delay(300);
    (void)eng_asset_cook_poll(&cook, seconds());
    EXPECT_EQ(eng_assets_poll(&assets, seconds()), (uint32_t)0);

    /* la texture change dans « le logiciel de dessin » */
    ASSERT_TRUE(write_gltf(&after, &t, source));
    const double start = seconds();
    double visible_after = -1.0;
    while (seconds() - start < 6.0 && visible_after < 0.0) {
        (void)eng_asset_cook_poll(&cook, seconds());
        if (eng_assets_poll(&assets, seconds()) > 0) {
            ASSERT_TRUE(
                render_gpu(&t, eng_model_asset_gpu(&assets, model_type, handle), g_managed));
            if (memcmp(g_direct, g_managed, sizeof g_direct) != 0) {
                visible_after = seconds() - start;
            }
        }
        SDL_Delay(5);
    }
    ENG_LOG_INFO("texture modifiée visible après %.3f s", visible_after);
    EXPECT_TRUE(visible_after > 0.0);
    EXPECT_TRUE(visible_after < 2.0);
    EXPECT_GT(count_lit_pixels(g_managed), (uint32_t)300);
    EXPECT_TRUE(eng_model_asset_gpu(&assets, model_type, handle) == gpu); /* même payload */
    EXPECT_GE(eng_assets_stats(&assets).reloads, (uint32_t)1);

    /* un source cassée : la cuisson échoue, l'ancien modèle reste affiché */
    ASSERT_TRUE(SDL_SaveFile(source, "pas du glTF", 11));
    const double broken = seconds();
    while (seconds() - broken < 2.0 && cook.failed == 0) {
        (void)eng_asset_cook_poll(&cook, seconds());
        (void)eng_assets_poll(&assets, seconds());
        SDL_Delay(5);
    }
    EXPECT_EQ(cook.failed, (uint32_t)1);
    ASSERT_TRUE(render_gpu(&t, eng_model_asset_gpu(&assets, model_type, handle), g_direct));
    EXPECT_EQ(memcmp(g_direct, g_managed, sizeof g_direct), 0);

    eng_asset_cook_shutdown(&cook);
    EXPECT_TRUE(eng_assets_release(&assets, handle));
    EXPECT_EQ(eng_assets_shutdown(&assets), (uint32_t)0);
    eng_renderer_end_frame(&t.r);
    CLOSE_MODEL(t); /* chaque rechargement a rendu l'ancienne version : aucune fuite */
}

#endif /* OW_SHADER_DIR */
