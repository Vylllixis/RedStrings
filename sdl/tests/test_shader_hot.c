#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/platform.h"
#include "engine/shader_hot.h"
#include "test_util.h"
#include "utest.h"

/* Rechargement à chaud des shaders, de bout en bout : un shader est écrit dans un fichier, rendu
 * dans une texture, modifié, rechargé, rendu de nouveau, et on lit le pixel obtenu. Comme
 * test_shader.c, les tests qui ont besoin d'un GPU s'arrêtent là quand il n'y en a pas
 * (sauf OW_REQUIRE_GPU=1). */

#define HOT_DIR TEST_TMP_DIR "/hot"

/* Les outils ci-dessous ne servent qu'aux tests compilés dans cette configuration. */
#if defined(ENG_SHADER_HOT_RELOAD) || defined(OW_SHADER_DIR)

#ifdef ENG_SHADER_HOT_RELOAD
static const char VERTEX_SRC[] = "float4 main(uint id : SV_VertexID) : SV_Position {\n"
                                 "    float2 uv = float2((id << 1) & 2, id & 2);\n"
                                 "    return float4(uv * 2.0 - 1.0, 0.0, 1.0);\n"
                                 "}\n";
static const char BROKEN_SRC[] = "float4 main( : SV_Target0 { return nonsense; }\n";
#endif
static const char RED_SRC[] = "float4 main() : SV_Target0 { return float4(1.0, 0.0, 0.0, 1.0); }\n";
static const char GREEN_SRC[] = "float4 main() : SV_Target0 { return float4(0, 1, 0, 1); }\n";

typedef struct Fixture {
    EngPlatform platform;
    EngGpu gpu;
    EngArena scratch;
} Fixture;

static bool gpu_required(void)
{
    const char *value = getenv("OW_REQUIRE_GPU");
    return value != NULL && value[0] == '1';
}

static bool open_fixture(Fixture *f)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    const EngWindowDesc window = {
        .title = "test shader hot", .width = 64, .height = 64, .no_software_surface = true};
    if (!eng_platform_init(&f->platform, &window)) {
        return false;
    }
    const EngGpuDesc desc = {.debug = false, .present_mode = ENG_PRESENT_VSYNC};
    if (!eng_gpu_init(&f->gpu, f->platform.window, &desc)) {
        eng_platform_shutdown(&f->platform);
        return false;
    }
    if (!eng_arena_init(&f->scratch, "shader-hot-test", (size_t)1 << 20)) {
        eng_gpu_shutdown(&f->gpu);
        eng_platform_shutdown(&f->platform);
        return false;
    }
    (void)SDL_CreateDirectory(TEST_TMP_DIR);
    (void)SDL_CreateDirectory(HOT_DIR);
    return true;
}

static void close_fixture(Fixture *f)
{
    eng_arena_shutdown(&f->scratch);
    eng_gpu_shutdown(&f->gpu);
    eng_platform_shutdown(&f->platform);
}

static void write_text(const char *name, const char *text)
{
    char path[256];
    SDL_snprintf(path, sizeof path, "%s/%s", HOT_DIR, name);
    (void)SDL_SaveFile(path, text, strlen(text));
}

static void path_of(const char *name, char *out, size_t size)
{
    SDL_snprintf(out, size, "%s/%s", HOT_DIR, name);
}

#ifdef ENG_SHADER_HOT_RELOAD

/* ── Rendu d'un pixel avec une paire de shaders ── */

typedef struct Pipe {
    SDL_GPUGraphicsPipeline *pipeline;
} Pipe;

static Pipe pipe_create(Fixture *f, SDL_GPUShader *vertex, SDL_GPUShader *fragment)
{
    const SDL_GPUColorTargetDescription target = {.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM};
    SDL_GPUGraphicsPipelineCreateInfo info;
    SDL_zero(info);
    info.vertex_shader = vertex;
    info.fragment_shader = fragment;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    return (Pipe){SDL_CreateGPUGraphicsPipeline(f->gpu.device, &info)};
}

/* Dessine un triangle plein écran dans une texture 4x4 et lit le pixel (0, 0). false si une
 * étape échoue. */
static bool pipe_draw_pixel(Fixture *f, Pipe pipe, uint8_t rgba[4])
{
    SDL_GPUDevice *device = f->gpu.device;
    SDL_GPUTextureCreateInfo texture_info;
    SDL_zero(texture_info);
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    texture_info.width = 4;
    texture_info.height = 4;
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device, &texture_info);

    const SDL_GPUTransferBufferCreateInfo buffer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = 4 * 4 * 4};
    SDL_GPUTransferBuffer *buffer = SDL_CreateGPUTransferBuffer(device, &buffer_info);

    bool ok = false;
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device);
    if (texture != NULL && buffer != NULL && commands != NULL && pipe.pipeline != NULL) {
        const SDL_GPUColorTargetInfo color = {.texture = texture,
                                              .clear_color = {0.0F, 0.0F, 0.0F, 1.0F},
                                              .load_op = SDL_GPU_LOADOP_CLEAR,
                                              .store_op = SDL_GPU_STOREOP_STORE};
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(commands, &color, 1, NULL);
        SDL_BindGPUGraphicsPipeline(pass, pipe.pipeline);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(pass);

        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
        const SDL_GPUTextureRegion region = {.texture = texture, .w = 4, .h = 4, .d = 1};
        const SDL_GPUTextureTransferInfo destination = {
            .transfer_buffer = buffer, .pixels_per_row = 4, .rows_per_layer = 4};
        SDL_DownloadFromGPUTexture(copy, &region, &destination);
        SDL_EndGPUCopyPass(copy);

        SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
        commands = NULL;
        if (fence != NULL) {
            ok = SDL_WaitForGPUFences(device, true, &fence, 1);
            SDL_ReleaseGPUFence(device, fence);
        }
        if (ok) {
            const uint8_t *mapped =
                (const uint8_t *)SDL_MapGPUTransferBuffer(device, buffer, false);
            ok = mapped != NULL;
            if (ok) {
                memcpy(rgba, mapped, 4);
                SDL_UnmapGPUTransferBuffer(device, buffer);
            }
        }
    }
    if (commands != NULL) {
        (void)SDL_CancelGPUCommandBuffer(commands);
    }
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    SDL_ReleaseGPUTexture(device, texture);
    return ok;
}

static void pipe_destroy(Fixture *f, Pipe pipe)
{
    SDL_ReleaseGPUGraphicsPipeline(f->gpu.device, pipe.pipeline);
}

#endif /* ENG_SHADER_HOT_RELOAD */

/* Appelle poll avec un temps qui avance par pas de 0,3 s (intervalle de contrôle : 0,25 s)
 * jusqu'à ce qu'il signale un rechargement ; `max_steps` pas au plus. */
static bool poll_until_reloaded(EngShaderHot *hot, Fixture *f, double *now, int max_steps)
{
    for (int i = 0; i < max_steps; i++) {
        *now += 0.3;
        if (eng_shader_hot_poll(hot, &f->gpu, &f->scratch, *now)) {
            return true;
        }
    }
    return false;
}

#endif /* outils de test */

/* Macro et non fonction : EXPECT_* n'existe que dans le corps d'un UTEST. */
#define EXPECT_PIXEL(rgba, r, g, b)                                                                \
    do {                                                                                           \
        EXPECT_EQ((rgba)[0], (r));                                                                 \
        EXPECT_EQ((rgba)[1], (g));                                                                 \
        EXPECT_EQ((rgba)[2], (b));                                                                 \
        EXPECT_EQ((rgba)[3], 255);                                                                 \
    } while (0)

UTEST(eng_shader_hot, availability_matches_the_build)
{
#ifdef ENG_SHADER_HOT_RELOAD
    EXPECT_TRUE(eng_shader_hot_available());
#else
    EXPECT_FALSE(eng_shader_hot_available());
#endif
}

#ifdef ENG_SHADER_HOT_RELOAD

UTEST(eng_shader_hot, editing_the_source_changes_what_is_drawn)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    write_text("vert.hlsl", VERTEX_SRC);
    write_text("frag.hlsl", RED_SRC);
    char vert_path[256];
    char frag_path[256];
    path_of("vert.hlsl", vert_path, sizeof vert_path);
    path_of("frag.hlsl", frag_path, sizeof frag_path);

    EngShaderHot vert;
    EngShaderHot frag;
    const size_t before = eng_arena_remaining(&f.scratch);
    ASSERT_TRUE(eng_shader_hot_init(&vert, &f.gpu, &f.scratch, vert_path, NULL, "absent.vert",
                                    ENG_SHADER_VERTEX));
    ASSERT_TRUE(eng_shader_hot_init(&frag, &f.gpu, &f.scratch, frag_path, NULL, "absent.frag",
                                    ENG_SHADER_FRAGMENT));
    EXPECT_EQ(eng_arena_remaining(&f.scratch), before); /* l'arène de travail est restituée */
    EXPECT_EQ(frag.generation, (uint32_t)1);
    EXPECT_TRUE(frag.hot);

    uint8_t pixel[4] = {0};
    Pipe first = pipe_create(&f, vert.shader, frag.shader);
    ASSERT_TRUE(first.pipeline != NULL);
    ASSERT_TRUE(pipe_draw_pixel(&f, first, pixel));
    EXPECT_PIXEL(pixel, 255, 0, 0); /* rouge */

    /* Fichier inchangé : rien ne se passe, même après plusieurs contrôles. */
    double now = 0.0;
    EXPECT_FALSE(poll_until_reloaded(&frag, &f, &now, 5));
    EXPECT_EQ(frag.generation, (uint32_t)1);

    /* Modification : détectée une fois le fichier stable, puis recompilée. */
    SDL_GPUShader *old_shader = frag.shader;
    write_text("frag.hlsl", GREEN_SRC);
    ASSERT_TRUE(poll_until_reloaded(&frag, &f, &now, 10));
    EXPECT_EQ(frag.generation, (uint32_t)2);
    EXPECT_TRUE(frag.shader != NULL);
    (void)old_shader; /* l'adresse peut être réutilisée par le pilote : on ne la compare pas */

    /* Le pipeline créé avant le rechargement vit toujours (son shader d'origine a été libéré). */
    ASSERT_TRUE(pipe_draw_pixel(&f, first, pixel));
    EXPECT_PIXEL(pixel, 255, 0, 0);

    /* Le pipeline recréé avec le nouveau shader dessine la nouvelle couleur. */
    Pipe second = pipe_create(&f, vert.shader, frag.shader);
    ASSERT_TRUE(second.pipeline != NULL);
    ASSERT_TRUE(pipe_draw_pixel(&f, second, pixel));
    EXPECT_PIXEL(pixel, 0, 255, 0); /* vert */

    pipe_destroy(&f, first);
    pipe_destroy(&f, second);
    eng_shader_hot_release(&vert, &f.gpu);
    eng_shader_hot_release(&frag, &f.gpu);
    eng_shader_hot_release(&frag, &f.gpu); /* sans effet */
    close_fixture(&f);
}

UTEST(eng_shader_hot, a_compile_error_keeps_the_old_shader_and_does_not_crash)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    write_text("vert.hlsl", VERTEX_SRC);
    write_text("frag.hlsl", RED_SRC);
    char vert_path[256];
    char frag_path[256];
    path_of("vert.hlsl", vert_path, sizeof vert_path);
    path_of("frag.hlsl", frag_path, sizeof frag_path);
    EngShaderHot vert;
    EngShaderHot frag;
    ASSERT_TRUE(eng_shader_hot_init(&vert, &f.gpu, &f.scratch, vert_path, NULL, "absent.vert",
                                    ENG_SHADER_VERTEX));
    ASSERT_TRUE(eng_shader_hot_init(&frag, &f.gpu, &f.scratch, frag_path, NULL, "absent.frag",
                                    ENG_SHADER_FRAGMENT));
    SDL_GPUShader *good = frag.shader;
    const size_t before = eng_arena_remaining(&f.scratch);

    double now = 0.0;
    write_text("frag.hlsl", BROKEN_SRC);
    EXPECT_FALSE(poll_until_reloaded(&frag, &f, &now, 10)); /* pas de nouveau shader... */
    EXPECT_EQ(frag.failed_reloads, (uint32_t)1);            /* ...mais l'échec est compté */
    EXPECT_EQ(frag.generation, (uint32_t)1);
    EXPECT_TRUE(frag.shader == good); /* l'ancien shader reste en service */
    EXPECT_EQ(eng_arena_remaining(&f.scratch), before);

    uint8_t pixel[4] = {0};
    Pipe pipe = pipe_create(&f, vert.shader, frag.shader);
    ASSERT_TRUE(pipe.pipeline != NULL);
    ASSERT_TRUE(pipe_draw_pixel(&f, pipe, pixel));
    EXPECT_PIXEL(pixel, 255, 0, 0); /* toujours rouge */
    pipe_destroy(&f, pipe);

    /* Le fichier est corrigé : le rechargement reprend, sans nouvelle erreur. */
    write_text("frag.hlsl", GREEN_SRC);
    ASSERT_TRUE(poll_until_reloaded(&frag, &f, &now, 10));
    EXPECT_EQ(frag.generation, (uint32_t)2);
    EXPECT_EQ(frag.failed_reloads, (uint32_t)1);
    pipe = pipe_create(&f, vert.shader, frag.shader);
    ASSERT_TRUE(pipe_draw_pixel(&f, pipe, pixel));
    EXPECT_PIXEL(pixel, 0, 255, 0);
    pipe_destroy(&f, pipe);

    eng_shader_hot_release(&vert, &f.gpu);
    eng_shader_hot_release(&frag, &f.gpu);
    close_fixture(&f);
}

UTEST(eng_shader_hot, includes_are_resolved_next_to_the_source)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    write_text("couleur.hlsli", "#define COULEUR float4(0.0, 0.0, 1.0, 1.0)\n");
    write_text("vert.hlsl", VERTEX_SRC);
    write_text("frag.hlsl",
               "#include \"couleur.hlsli\"\nfloat4 main() : SV_Target0 { return COULEUR; }\n");
    char vert_path[256];
    char frag_path[256];
    path_of("vert.hlsl", vert_path, sizeof vert_path);
    path_of("frag.hlsl", frag_path, sizeof frag_path);
    EngShaderHot vert;
    EngShaderHot frag;
    ASSERT_TRUE(eng_shader_hot_init(&vert, &f.gpu, &f.scratch, vert_path, NULL, "absent.vert",
                                    ENG_SHADER_VERTEX));
    ASSERT_TRUE(eng_shader_hot_init(&frag, &f.gpu, &f.scratch, frag_path, NULL, "absent.frag",
                                    ENG_SHADER_FRAGMENT));
    uint8_t pixel[4] = {0};
    Pipe pipe = pipe_create(&f, vert.shader, frag.shader);
    ASSERT_TRUE(pipe_draw_pixel(&f, pipe, pixel));
    EXPECT_PIXEL(pixel, 0, 0, 255);
    pipe_destroy(&f, pipe);
    eng_shader_hot_release(&vert, &f.gpu);
    eng_shader_hot_release(&frag, &f.gpu);
    close_fixture(&f);
}

UTEST(eng_shader_hot, a_missing_source_fails_unless_an_offline_shader_exists)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    EngShaderHot hot;
    char path[256];
    path_of("n_existe_pas.hlsl", path, sizeof path);
    EXPECT_FALSE(eng_shader_hot_init(&hot, &f.gpu, &f.scratch, path, NULL, "absent.frag",
                                     ENG_SHADER_FRAGMENT));
    EXPECT_TRUE(hot.shader == NULL);
    eng_shader_hot_release(&hot, &f.gpu); /* sans effet */
    close_fixture(&f);
}

#ifdef OW_SHADER_DIR
UTEST(eng_shader_hot, a_broken_source_at_startup_falls_back_to_the_offline_shader)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    write_text("frag.hlsl", BROKEN_SRC);
    char path[256];
    path_of("frag.hlsl", path, sizeof path);
    EngShaderHot hot;
    ASSERT_TRUE(eng_shader_hot_init(&hot, &f.gpu, &f.scratch, path, OW_SHADER_DIR, "triangle.frag",
                                    ENG_SHADER_FRAGMENT));
    EXPECT_TRUE(hot.shader != NULL);
    EXPECT_EQ(hot.generation, (uint32_t)1);
    eng_shader_hot_release(&hot, &f.gpu);
    close_fixture(&f);
}
#endif

#ifdef OW_SHADER_DIR
UTEST(eng_shader_hot, without_a_source_path_the_offline_shader_is_loaded_and_not_watched)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    EngShaderHot hot;
    ASSERT_TRUE(eng_shader_hot_init(&hot, &f.gpu, &f.scratch, NULL, OW_SHADER_DIR, "triangle.frag",
                                    ENG_SHADER_FRAGMENT));
    EXPECT_FALSE(hot.hot); /* même dans une build avec rechargement */
    EXPECT_TRUE(hot.shader != NULL);
    double now = 0.0;
    EXPECT_FALSE(poll_until_reloaded(&hot, &f, &now, 5));
    EXPECT_EQ(hot.generation, (uint32_t)1);
    eng_shader_hot_release(&hot, &f.gpu);
    close_fixture(&f);
}

#endif

UTEST(eng_shader_hot, compile_hlsl_rejects_garbage_without_crashing)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    EXPECT_TRUE(eng_shader_compile_hlsl(&f.gpu, BROKEN_SRC, NULL, ENG_SHADER_FRAGMENT, "garbage") ==
                NULL);
    EXPECT_TRUE(eng_shader_compile_hlsl(&f.gpu, "", NULL, ENG_SHADER_FRAGMENT, "vide") == NULL);
    EXPECT_TRUE(eng_shader_compile_hlsl(&f.gpu, NULL, NULL, ENG_SHADER_FRAGMENT, "null") == NULL);
    close_fixture(&f);
}

#else /* build sans rechargement à chaud */

#ifdef OW_SHADER_DIR
UTEST(eng_shader_hot, without_hot_reload_the_offline_shader_is_used_and_never_reloaded)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    write_text("frag.hlsl", RED_SRC);
    char path[256];
    path_of("frag.hlsl", path, sizeof path);
    EngShaderHot hot;
    ASSERT_TRUE(eng_shader_hot_init(&hot, &f.gpu, &f.scratch, path, OW_SHADER_DIR, "triangle.frag",
                                    ENG_SHADER_FRAGMENT));
    EXPECT_FALSE(hot.hot);
    write_text("frag.hlsl", GREEN_SRC);
    double now = 0.0;
    EXPECT_FALSE(poll_until_reloaded(&hot, &f, &now, 10));
    EXPECT_EQ(hot.generation, (uint32_t)1);
    eng_shader_hot_release(&hot, &f.gpu);
    close_fixture(&f);
}
#endif

#endif
