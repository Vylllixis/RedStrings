#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/platform.h"
#include "engine/shader.h"
#include "test_util.h"
#include "utest.h"

/* OW_SHADER_DIR : dossier des shaders compilés par la build (défini par CMake, absent si
 * OW_BUILD_SHADERS=OFF). Les tests qui lisent ces fichiers s'arrêtent là si le dossier n'existe
 * pas. */

/* ── Choix du format selon le pilote : pur, testable partout (y compris D3D12 et Metal) ── */

UTEST(eng_shader, format_follows_what_the_device_accepts)
{
    EngShaderFormat f;
    ASSERT_TRUE(eng_shader_choose_format(SDL_GPU_SHADERFORMAT_SPIRV, &f)); /* Vulkan */
    EXPECT_STREQ(f.extension, "spv");
    EXPECT_STREQ(f.entry_point, "main");
    EXPECT_EQ(f.sdl_format, (uint32_t)SDL_GPU_SHADERFORMAT_SPIRV);

    ASSERT_TRUE(eng_shader_choose_format(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_DXBC,
                                         &f)); /* Direct3D 12 */
    EXPECT_STREQ(f.extension, "dxil");
    EXPECT_STREQ(f.entry_point, "main");
    EXPECT_EQ(f.sdl_format, (uint32_t)SDL_GPU_SHADERFORMAT_DXIL);

    ASSERT_TRUE(eng_shader_choose_format(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
                                         &f)); /* Metal */
    EXPECT_STREQ(f.extension, "msl");
    EXPECT_STREQ(f.entry_point, "main0"); /* nom imposé par la conversion SPIR-V -> MSL */
    EXPECT_EQ(f.sdl_format, (uint32_t)SDL_GPU_SHADERFORMAT_MSL);
}

UTEST(eng_shader, spirv_is_preferred_when_several_formats_are_accepted)
{
    EngShaderFormat f;
    ASSERT_TRUE(eng_shader_choose_format(
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV, &f));
    EXPECT_STREQ(f.extension, "spv");
    ASSERT_TRUE(eng_shader_choose_format(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_DXIL, &f));
    EXPECT_STREQ(f.extension, "dxil");
}

UTEST(eng_shader, no_usable_format_is_reported)
{
    EngShaderFormat f = {0};
    EXPECT_FALSE(eng_shader_choose_format(0, &f));
    EXPECT_FALSE(
        eng_shader_choose_format(SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_PRIVATE, &f));
}

/* ── Lecture de la réflexion ── */

UTEST(eng_shader, counts_are_read_from_real_shadercross_output)
{
    /* Sortie réelle de shadercross -d JSON pour un fragment avec 1 texture + 1 tampon uniforme. */
    const char *json =
        "{ \"samplers\": 1, \"storage_textures\": 0, \"storage_buffers\": 2, "
        "\"uniform_buffers\": 3, \"inputs\": [{ \"name\": \"in.var.TEXCOORD0\", \"type\": "
        "\"float2\", \"location\": 0 }], \"outputs\": [] }";
    EngShaderCounts c = {99, 99, 99, 99};
    ASSERT_TRUE(eng_shader_parse_counts(json, &c));
    EXPECT_EQ(c.samplers, (uint32_t)1);
    EXPECT_EQ(c.storage_textures, (uint32_t)0);
    EXPECT_EQ(c.storage_buffers, (uint32_t)2);
    EXPECT_EQ(c.uniform_buffers, (uint32_t)3);
}

UTEST(eng_shader, counts_do_not_depend_on_key_order_or_spacing)
{
    const char *json = "{\"uniform_buffers\":4,\n \"storage_buffers\" :\t5,"
                       "\"storage_textures\":6,\"samplers\":7}";
    EngShaderCounts c = {0};
    ASSERT_TRUE(eng_shader_parse_counts(json, &c));
    EXPECT_EQ(c.samplers, (uint32_t)7);
    EXPECT_EQ(c.storage_textures, (uint32_t)6);
    EXPECT_EQ(c.storage_buffers, (uint32_t)5);
    EXPECT_EQ(c.uniform_buffers, (uint32_t)4);
}

UTEST(eng_shader, unreadable_counts_are_rejected_and_leave_the_output_untouched)
{
    const char *bad[] = {
        "",
        "{}",
        "{ \"samplers\": 1, \"storage_textures\": 0, \"storage_buffers\": 0 }", /* clé manquante */
        "{ \"samplers\": -1, \"storage_textures\": 0, \"storage_buffers\": 0, "
        "\"uniform_buffers\": 0 }",
        "{ \"samplers\": \"1\", \"storage_textures\": 0, \"storage_buffers\": 0, "
        "\"uniform_buffers\": 0 }",
        "{ \"samplers\": 5000, \"storage_textures\": 0, \"storage_buffers\": 0, "
        "\"uniform_buffers\": 0 }",
        "{ \"samplers\" 1, \"storage_textures\": 0, \"storage_buffers\": 0, \"uniform_buffers\": 0 "
        "}",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        EngShaderCounts c = {11, 22, 33, 44};
        EXPECT_FALSE(eng_shader_parse_counts(bad[i], &c));
        EXPECT_EQ(c.samplers, (uint32_t)11);
        EXPECT_EQ(c.uniform_buffers, (uint32_t)44);
    }
}

/* ── Fichiers produits par la build ── */

#ifdef OW_SHADER_DIR
static char *read_shader_file(const char *name, size_t *size)
{
    char path[512];
    SDL_snprintf(path, sizeof path, OW_SHADER_DIR "/%s", name);
    return (char *)SDL_LoadFile(path, size);
}

UTEST(eng_shader, the_build_produces_every_format_for_every_shader)
{
    const char *shaders[] = {"triangle.vert", "triangle.frag"};
    for (size_t i = 0; i < sizeof shaders / sizeof shaders[0]; i++) {
        char name[128];
        size_t size = 0;

        SDL_snprintf(name, sizeof name, "%s.spv", shaders[i]);
        uint8_t *spirv = (uint8_t *)read_shader_file(name, &size);
        ASSERT_TRUE(spirv != NULL);
        ASSERT_TRUE(size >= 20 && size % 4 == 0);
        EXPECT_EQ(spirv[0], 0x03); /* magie SPIR-V 0x07230203, petit-boutiste */
        EXPECT_EQ(spirv[1], 0x02);
        EXPECT_EQ(spirv[2], 0x23);
        EXPECT_EQ(spirv[3], 0x07);
        SDL_free(spirv);

        SDL_snprintf(name, sizeof name, "%s.dxil", shaders[i]);
        char *dxil = read_shader_file(name, &size);
        ASSERT_TRUE(dxil != NULL);
        ASSERT_TRUE(size > 4);
        EXPECT_EQ(memcmp(dxil, "DXBC", 4), 0); /* conteneur DXIL */
        SDL_free(dxil);

        SDL_snprintf(name, sizeof name, "%s.msl", shaders[i]);
        char *msl = read_shader_file(name, &size);
        ASSERT_TRUE(msl != NULL);
        EXPECT_TRUE(test_contains(msl, "metal_stdlib"));
        EXPECT_TRUE(test_contains(msl, "main0"));
        SDL_free(msl);

        SDL_snprintf(name, sizeof name, "%s.json", shaders[i]);
        char *json = read_shader_file(name, &size);
        ASSERT_TRUE(json != NULL);
        EngShaderCounts c;
        EXPECT_TRUE(eng_shader_parse_counts(json, &c));
        SDL_free(json);
    }
}

/* ── Création réelle auprès du pilote (même règle que test_gpu.c : sans GPU, on s'arrête là) ── */

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
        .title = "test shader", .width = 64, .height = 64, .no_software_surface = true};
    if (!eng_platform_init(&f->platform, &window)) {
        return false;
    }
    const EngGpuDesc desc = {.debug = false, .present_mode = ENG_PRESENT_VSYNC};
    if (!eng_gpu_init(&f->gpu, f->platform.window, &desc)) {
        eng_platform_shutdown(&f->platform);
        return false;
    }
    if (!eng_arena_init(&f->scratch, "shader-test", (size_t)1 << 20)) {
        eng_gpu_shutdown(&f->gpu);
        eng_platform_shutdown(&f->platform);
        return false;
    }
    return true;
}

static void close_fixture(Fixture *f)
{
    eng_arena_shutdown(&f->scratch);
    eng_gpu_shutdown(&f->gpu);
    eng_platform_shutdown(&f->platform);
}

UTEST(eng_shader, the_driver_accepts_the_compiled_shaders)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    const size_t before = eng_arena_remaining(&f.scratch);
    SDL_GPUShader *vertex =
        eng_shader_load(&f.gpu, &f.scratch, OW_SHADER_DIR, "triangle.vert", ENG_SHADER_VERTEX);
    SDL_GPUShader *fragment =
        eng_shader_load(&f.gpu, &f.scratch, OW_SHADER_DIR, "triangle.frag", ENG_SHADER_FRAGMENT);
    EXPECT_TRUE(vertex != NULL);
    EXPECT_TRUE(fragment != NULL);
    EXPECT_EQ(eng_arena_remaining(&f.scratch), before); /* l'arène de travail est restituée */
    eng_shader_release(&f.gpu, vertex);
    eng_shader_release(&f.gpu, fragment);
    eng_shader_release(&f.gpu, NULL); /* sans effet */
    close_fixture(&f);
}

UTEST(eng_shader, missing_or_unusable_shaders_return_null)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    const size_t before = eng_arena_remaining(&f.scratch);
    EXPECT_TRUE(eng_shader_load(&f.gpu, &f.scratch, OW_SHADER_DIR, "absent.vert",
                                ENG_SHADER_VERTEX) == NULL);
    EXPECT_TRUE(eng_shader_load(&f.gpu, &f.scratch, OW_SHADER_DIR "/dossier_inexistant", "x.vert",
                                ENG_SHADER_VERTEX) == NULL);
    EXPECT_EQ(eng_arena_remaining(&f.scratch), before);
    close_fixture(&f);
    EXPECT_TRUE(eng_shader_load(&f.gpu, &f.scratch, OW_SHADER_DIR, "triangle.vert",
                                ENG_SHADER_VERTEX) == NULL); /* GPU arrêté */
}
#endif
