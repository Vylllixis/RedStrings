#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "engine/gpu.h"
#include "engine/platform.h"
#include "utest.h"

/* Contexte GPU, de bout en bout : fenêtre sans écran (pilote vidéo « offscreen »), vrai pilote
 * Vulkan/D3D12/Metal. Sur une machine sans pilote GPU (CI), ces tests se terminent sans rien
 * vérifier ; avec OW_REQUIRE_GPU=1 l'absence de GPU devient une ÉCHEC (à utiliser en local :
 * `OW_REQUIRE_GPU=1 ctest -R gpu`). */

typedef struct Fixture {
    EngPlatform platform;
    EngGpu gpu;
    bool ready;
} Fixture;

static bool gpu_required(void)
{
    const char *value = getenv("OW_REQUIRE_GPU");
    return value != NULL && value[0] == '1';
}

/* false : GPU absent (le test doit s'arrêter là, sauf si le GPU est exigé). `frames` : images en
 * vol demandées (0 : valeur par défaut). */
static bool open_fixture_n(Fixture *f, uint32_t frames)
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    const EngWindowDesc window = {.title = "test gpu",
                                  .width = 320,
                                  .height = 200,
                                  .resizable = true,
                                  .no_software_surface = true};
    if (!eng_platform_init(&f->platform, &window)) {
        return false;
    }
    const EngGpuDesc desc = {
        .debug = false, .present_mode = ENG_PRESENT_VSYNC, .frames_in_flight = frames};
    f->ready = eng_gpu_init(&f->gpu, f->platform.window, &desc);
    if (!f->ready) {
        eng_platform_shutdown(&f->platform);
    }
    return f->ready;
}

static bool open_fixture(Fixture *f)
{
    return open_fixture_n(f, 0U);
}

static void close_fixture(Fixture *f)
{
    eng_gpu_shutdown(&f->gpu);
    eng_platform_shutdown(&f->platform);
}

static bool run_frames(Fixture *f, int count)
{
    for (int i = 0; i < count; i++) {
        (void)eng_platform_pump(&f->platform);
        if (!eng_gpu_begin_frame(&f->gpu)) {
            return false;
        }
        eng_gpu_end_frame(&f->gpu);
    }
    return true;
}

UTEST(eng_gpu, init_names_the_driver_and_shutdown_is_repeatable)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required()); /* pas de GPU : sans importance, sauf s'il est exigé */
        return;
    }
    EXPECT_TRUE(strlen(eng_gpu_driver_name(&f.gpu)) > 0);
    EXPECT_TRUE(strcmp(eng_gpu_driver_name(&f.gpu), "aucun") != 0);
    EXPECT_EQ(f.gpu.present_mode, ENG_PRESENT_VSYNC);
    close_fixture(&f);
    EXPECT_TRUE(f.gpu.device == NULL);
    EXPECT_STREQ(eng_gpu_driver_name(&f.gpu), "aucun");
    eng_gpu_shutdown(&f.gpu); /* deuxième arrêt : sans effet, sans crash */
}

UTEST(eng_gpu, frames_are_cleared_submitted_and_counted)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required()); /* pas de GPU : sans importance, sauf s'il est exigé */
        return;
    }
    eng_gpu_set_clear_color(&f.gpu, 0.2F, 0.4F, 0.6F, 1.0F);
    EXPECT_TRUE(run_frames(&f, 60));
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)60);
    EXPECT_EQ(f.gpu.frames_skipped, (uint64_t)0);
    close_fixture(&f);
}

UTEST(eng_gpu, begin_and_end_misuse_is_harmless)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required()); /* pas de GPU : sans importance, sauf s'il est exigé */
        return;
    }
    eng_gpu_end_frame(&f.gpu); /* sans begin : sans effet */
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)0);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    EXPECT_FALSE(eng_gpu_begin_frame(&f.gpu)); /* image déjà commencée */
    eng_gpu_end_frame(&f.gpu);                 /* l'image commencée se termine normalement */
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)1);
    close_fixture(&f);
    EXPECT_FALSE(eng_gpu_begin_frame(&f.gpu)); /* arrêté : refusé */
    eng_gpu_end_frame(&f.gpu);
}

UTEST(eng_gpu, the_render_pass_is_opened_once_per_frame_and_closed_by_end_frame)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required()); /* pas de GPU : sans importance, sauf s'il est exigé */
        return;
    }
    EXPECT_TRUE(eng_gpu_begin_render_pass(&f.gpu) == NULL); /* pas d'image en cours */
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
        EXPECT_TRUE(f.gpu.render_pass == NULL);
        EXPECT_TRUE(eng_gpu_begin_render_pass(&f.gpu) != NULL);
        EXPECT_TRUE(eng_gpu_begin_render_pass(&f.gpu) == NULL); /* déjà ouverte */
        eng_gpu_end_frame(&f.gpu);
        EXPECT_TRUE(f.gpu.render_pass == NULL); /* l'image suivante repart de zéro */
    }
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)3);
    EXPECT_EQ(f.gpu.frames_skipped, (uint64_t)0);
    close_fixture(&f);
    EXPECT_TRUE(eng_gpu_begin_render_pass(&f.gpu) == NULL); /* arrêté */
}

UTEST(eng_gpu, the_swapchain_format_is_known_while_running)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required()); /* pas de GPU : sans importance, sauf s'il est exigé */
        return;
    }
    EXPECT_TRUE(eng_gpu_swapchain_format(&f.gpu) != 0);
    close_fixture(&f);
    EXPECT_EQ(eng_gpu_swapchain_format(&f.gpu), (uint32_t)0);
}

UTEST(eng_gpu, present_modes_are_consistent_with_what_the_driver_supports)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required()); /* pas de GPU : sans importance, sauf s'il est exigé */
        return;
    }
    EXPECT_TRUE(eng_gpu_set_present_mode(&f.gpu, ENG_PRESENT_VSYNC)); /* toujours disponible */
    const EngPresentMode modes[] = {ENG_PRESENT_MAILBOX, ENG_PRESENT_IMMEDIATE, ENG_PRESENT_VSYNC};
    for (size_t i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        const EngPresentMode before = f.gpu.present_mode;
        if (eng_gpu_set_present_mode(&f.gpu, modes[i])) {
            EXPECT_EQ(f.gpu.present_mode, modes[i]);
        } else {
            EXPECT_EQ(f.gpu.present_mode, before); /* refusé : mode inchangé */
        }
        EXPECT_TRUE(run_frames(&f, 5)); /* quel que soit le mode actif, on présente */
    }
    close_fixture(&f);
}

UTEST(eng_gpu, a_resized_window_gives_a_swapchain_of_the_new_size)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required()); /* pas de GPU : sans importance, sauf s'il est exigé */
        return;
    }
    ASSERT_TRUE(run_frames(&f, 3));
    const int sizes[][2] = {{640, 360}, {200, 120}, {1024, 768}, {321, 199}, {800, 600}};
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        ASSERT_TRUE(SDL_SetWindowSize(f.platform.window, sizes[i][0], sizes[i][1]));
        (void)SDL_SyncWindow(f.platform.window);
        ASSERT_TRUE(run_frames(&f, 3)); /* laisse la swapchain suivre */
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(f.platform.window, &width, &height);
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
        EXPECT_EQ((int)f.gpu.swapchain_width, width);
        EXPECT_EQ((int)f.gpu.swapchain_height, height);
        eng_gpu_end_frame(&f.gpu);
    }
    EXPECT_EQ(f.gpu.frames_skipped, (uint64_t)0);
    close_fixture(&f);
}

UTEST(eng_gpu, many_rapid_resizes_never_fail)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required()); /* pas de GPU : sans importance, sauf s'il est exigé */
        return;
    }
    for (int i = 0; i < 100; i++) {
        const int width = 160 + (i * 37) % 700;
        const int height = 120 + (i * 53) % 500;
        ASSERT_TRUE(SDL_SetWindowSize(f.platform.window, width, height));
        (void)eng_platform_pump(&f.platform);
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu)); /* sans attendre la fin du redimensionnement */
        eng_gpu_end_frame(&f.gpu);
    }
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)100);
    EXPECT_EQ(f.gpu.frames_skipped, (uint64_t)0);
    close_fixture(&f);
}

/* ── Profondeur (M1-07) ── */

UTEST(eng_gpu, depth_is_off_until_enabled_then_follows_the_swapchain_size)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required()); /* pas de GPU : sans importance, sauf s'il est exigé */
        return;
    }
    EXPECT_EQ(eng_gpu_depth_format(&f.gpu), (uint32_t)0);
    ASSERT_TRUE(run_frames(&f, 2));
    EXPECT_TRUE(f.gpu.depth_texture == NULL); /* pas activée : aucune texture */

    ASSERT_TRUE(eng_gpu_enable_depth(&f.gpu));
    ASSERT_TRUE(eng_gpu_enable_depth(&f.gpu)); /* idempotent */
    EXPECT_TRUE(eng_gpu_depth_format(&f.gpu) != 0);
    const int sizes[][2] = {{640, 360}, {200, 120}, {1024, 768}, {321, 199}};
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        ASSERT_TRUE(SDL_SetWindowSize(f.platform.window, sizes[i][0], sizes[i][1]));
        (void)SDL_SyncWindow(f.platform.window);
        ASSERT_TRUE(run_frames(&f, 3));
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
        EXPECT_TRUE(f.gpu.depth_texture != NULL);
        EXPECT_EQ(f.gpu.depth_width, f.gpu.swapchain_width);
        EXPECT_EQ(f.gpu.depth_height, f.gpu.swapchain_height);
        EXPECT_EQ((int)f.gpu.depth_width, sizes[i][0]);
        EXPECT_EQ((int)f.gpu.depth_height, sizes[i][1]);
        eng_gpu_end_frame(&f.gpu);
    }
    EXPECT_EQ(f.gpu.frames_skipped, (uint64_t)0);
    close_fixture(&f);
}

UTEST(eng_gpu, rapid_resizes_with_depth_never_fail_or_leak)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required()); /* pas de GPU : sans importance, sauf s'il est exigé */
        return;
    }
    ASSERT_TRUE(eng_gpu_enable_depth(&f.gpu));
    for (int i = 0; i < 100; i++) {
        const int width = 160 + (i * 37) % 700;
        const int height = 120 + (i * 53) % 500;
        ASSERT_TRUE(SDL_SetWindowSize(f.platform.window, width, height));
        (void)eng_platform_pump(&f.platform);
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
        ASSERT_TRUE(eng_gpu_begin_render_pass(&f.gpu) != NULL); /* passe avec profondeur */
        eng_gpu_end_frame(&f.gpu);
    }
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)100);
    EXPECT_EQ(f.gpu.frames_skipped, (uint64_t)0);
    close_fixture(&f); /* ASan/LSan : toute texture de profondeur oubliée serait signalée */
}

UTEST(eng_gpu, the_depth_target_is_cleared_to_the_far_plane_and_not_kept)
{
    SDL_GPUDepthStencilTargetInfo info;
    eng_gpu_fill_depth_target(NULL, &info);
    EXPECT_EQ(info.clear_depth, 1.0F);
    EXPECT_EQ((int)info.load_op, (int)SDL_GPU_LOADOP_CLEAR);
    EXPECT_EQ((int)info.store_op, (int)SDL_GPU_STOREOP_DONT_CARE);
}

/* ── Passe overlay (M1-12) ── */

UTEST(eng_gpu, the_overlay_target_keeps_the_scene_instead_of_clearing_it)
{
    SDL_GPUColorTargetInfo target;
    int fake_texture = 0; /* adresse quelconque : la fonction ne déréférence pas la texture */
    SDL_GPUTexture *texture = (SDL_GPUTexture *)&fake_texture;
    eng_gpu_fill_overlay_target(texture, &target);
    EXPECT_EQ(target.load_op, SDL_GPU_LOADOP_LOAD); /* CLEAR effacerait la scène */
    EXPECT_EQ(target.store_op, SDL_GPU_STOREOP_STORE);
    EXPECT_TRUE(target.texture == texture);
}

UTEST(eng_gpu, the_overlay_pass_follows_the_main_pass_once_per_frame)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    EXPECT_TRUE(eng_gpu_begin_overlay_pass(&f.gpu) == NULL); /* pas d'image en cours */
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
        ASSERT_TRUE(eng_gpu_begin_render_pass(&f.gpu) != NULL);
        SDL_GPURenderPass *overlay = eng_gpu_begin_overlay_pass(&f.gpu);
        EXPECT_TRUE(overlay != NULL);
        EXPECT_TRUE(f.gpu.render_pass == overlay); /* c'est elle que end_frame terminera */
        EXPECT_TRUE(f.gpu.overlay_open);
        EXPECT_TRUE(eng_gpu_begin_overlay_pass(&f.gpu) == NULL); /* une seule par image */
        EXPECT_TRUE(eng_gpu_begin_render_pass(&f.gpu) == NULL);  /* la principale est finie */
        eng_gpu_end_frame(&f.gpu);
        EXPECT_FALSE(f.gpu.overlay_open); /* l'image suivante repart de zéro */
        EXPECT_FALSE(f.gpu.main_pass_ended);
        EXPECT_TRUE(f.gpu.render_pass == NULL);
    }
    EXPECT_EQ(f.gpu.overlay_passes, (uint64_t)3);
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)3);
    EXPECT_EQ(f.gpu.frames_skipped, (uint64_t)0);
    close_fixture(&f);
}

UTEST(eng_gpu, the_overlay_pass_works_even_if_nobody_drew_the_scene)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    EXPECT_TRUE(eng_gpu_begin_overlay_pass(&f.gpu) !=
                NULL); /* la passe principale est ouverte puis terminée */
    EXPECT_TRUE(f.gpu.main_pass_ended);
    eng_gpu_end_frame(&f.gpu);
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)1);
    close_fixture(&f);
}

UTEST(eng_gpu, an_overlay_from_a_frame_without_overlay_does_not_leak_into_the_next)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    EXPECT_TRUE(eng_gpu_begin_overlay_pass(&f.gpu) != NULL);
    eng_gpu_end_frame(&f.gpu);
    /* image suivante, sans overlay : la passe principale doit s'ouvrir normalement */
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    EXPECT_TRUE(eng_gpu_begin_render_pass(&f.gpu) != NULL);
    eng_gpu_end_frame(&f.gpu);
    EXPECT_EQ(f.gpu.overlay_passes, (uint64_t)1);
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)2);
    close_fixture(&f);
}

/* ── HDR et résolution (M1-14) ── */

typedef struct ResolveLog {
    EngGpu *gpu;
    int calls;
    bool had_pass;
    bool had_commands;
    SDL_GPUTexture *hdr_seen;
    uint64_t passes_seen_by_overlay;
    int overlay_calls;
} ResolveLog;

static void log_resolve(void *user, SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass,
                        SDL_GPUTexture *hdr)
{
    ResolveLog *log = (ResolveLog *)user;
    log->calls++;
    log->had_pass = pass != NULL;
    log->had_commands = commands != NULL;
    log->hdr_seen = hdr;
}

UTEST(eng_gpu_hdr, is_off_until_enabled_and_the_scene_format_follows)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    EXPECT_FALSE(f.gpu.hdr_enabled);
    EXPECT_EQ(eng_gpu_scene_format(&f.gpu), eng_gpu_swapchain_format(&f.gpu));
    ASSERT_TRUE(run_frames(&f, 2));
    EXPECT_TRUE(f.gpu.hdr_texture == NULL); /* pas activé : aucune texture */

    ASSERT_TRUE(eng_gpu_enable_hdr(&f.gpu));
    ASSERT_TRUE(eng_gpu_enable_hdr(&f.gpu)); /* idempotent */
    EXPECT_TRUE(f.gpu.hdr_enabled);
    EXPECT_EQ(eng_gpu_scene_format(&f.gpu), (uint32_t)SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT);
    EXPECT_EQ(eng_gpu_hdr_format(), (uint32_t)SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    EXPECT_TRUE(f.gpu.hdr_texture != NULL);
    eng_gpu_end_frame(&f.gpu);

    eng_gpu_disable_hdr(&f.gpu);
    EXPECT_FALSE(f.gpu.hdr_enabled);
    EXPECT_TRUE(f.gpu.hdr_texture == NULL);
    EXPECT_EQ(eng_gpu_scene_format(&f.gpu), eng_gpu_swapchain_format(&f.gpu));
    eng_gpu_disable_hdr(&f.gpu); /* répétable */
    ASSERT_TRUE(run_frames(&f, 2));
    EXPECT_TRUE(f.gpu.hdr_texture == NULL);
    close_fixture(&f);
}

UTEST(eng_gpu_hdr, enabling_on_a_stopped_gpu_is_refused)
{
    EngGpu gpu = {0};
    EXPECT_FALSE(eng_gpu_enable_hdr(&gpu));
    EXPECT_EQ(eng_gpu_scene_format(&gpu), (uint32_t)0);
    eng_gpu_disable_hdr(&gpu); /* sans effet, sans plantage */
}

UTEST(eng_gpu_hdr, the_resolve_runs_once_per_frame_with_the_hdr_texture_and_a_pass)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(eng_gpu_enable_hdr(&f.gpu));
    ResolveLog log = {.gpu = &f.gpu};
    eng_gpu_set_resolve(&f.gpu, log_resolve, &log);
    ASSERT_TRUE(eng_gpu_enable_depth(&f.gpu));
    for (int i = 1; i <= 3; i++) {
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
        EXPECT_TRUE(eng_gpu_begin_render_pass(&f.gpu) != NULL);
        EXPECT_EQ(log.calls, i - 1); /* pas avant la fin de la passe principale */
        eng_gpu_end_frame(&f.gpu);
        EXPECT_EQ(log.calls, i);
        EXPECT_TRUE(log.had_pass);
        EXPECT_TRUE(log.had_commands);
        EXPECT_TRUE(log.hdr_seen == f.gpu.hdr_texture);
        EXPECT_TRUE(log.hdr_seen != NULL);
    }
    EXPECT_EQ(f.gpu.resolve_passes, (uint64_t)3);
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)3);
    EXPECT_EQ(f.gpu.frames_skipped, (uint64_t)0);
    close_fixture(&f);
}

UTEST(eng_gpu_hdr, the_resolve_happens_even_if_nobody_drew_the_scene)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(eng_gpu_enable_hdr(&f.gpu));
    ResolveLog log = {0};
    eng_gpu_set_resolve(&f.gpu, log_resolve, &log);
    ASSERT_TRUE(run_frames(&f, 2)); /* begin_frame puis end_frame, sans passe */
    EXPECT_EQ(log.calls, 2);
    EXPECT_EQ(f.gpu.resolve_passes, (uint64_t)2);
    close_fixture(&f);
}

UTEST(eng_gpu_hdr, the_resolve_comes_before_the_overlay_which_is_not_resolved)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(eng_gpu_enable_hdr(&f.gpu));
    ResolveLog log = {0};
    eng_gpu_set_resolve(&f.gpu, log_resolve, &log);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    ASSERT_TRUE(eng_gpu_begin_render_pass(&f.gpu) != NULL);
    EXPECT_EQ(log.calls, 0);
    EXPECT_TRUE(eng_gpu_begin_overlay_pass(&f.gpu) != NULL);
    EXPECT_EQ(log.calls, 1); /* la résolution a eu lieu AVANT l'ouverture de l'overlay */
    EXPECT_TRUE(eng_gpu_begin_overlay_pass(&f.gpu) == NULL);
    eng_gpu_end_frame(&f.gpu);
    EXPECT_EQ(log.calls, 1); /* terminer l'overlay ne résout pas une seconde fois */
    EXPECT_EQ(f.gpu.resolve_passes, (uint64_t)1);
    EXPECT_EQ(f.gpu.overlay_passes, (uint64_t)1);
    /* image suivante sans overlay : une seule résolution aussi */
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    ASSERT_TRUE(eng_gpu_begin_render_pass(&f.gpu) != NULL);
    eng_gpu_end_frame(&f.gpu);
    EXPECT_EQ(log.calls, 2);
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)2);
    close_fixture(&f);
}

UTEST(eng_gpu_hdr, an_overlay_without_main_pass_still_resolves_first)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(eng_gpu_enable_hdr(&f.gpu));
    ResolveLog log = {0};
    eng_gpu_set_resolve(&f.gpu, log_resolve, &log);
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    EXPECT_TRUE(eng_gpu_begin_overlay_pass(&f.gpu) != NULL); /* ouvre, efface, résout */
    EXPECT_EQ(log.calls, 1);
    eng_gpu_end_frame(&f.gpu);
    EXPECT_EQ(log.calls, 1);
    close_fixture(&f);
}

UTEST(eng_gpu_hdr, no_resolve_function_means_a_black_frame_but_a_valid_one)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(eng_gpu_enable_hdr(&f.gpu));
    ASSERT_TRUE(run_frames(&f, 3));
    EXPECT_EQ(f.gpu.resolve_passes, (uint64_t)3); /* la passe de résolution existe : elle efface */
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)3);
    EXPECT_EQ(f.gpu.frames_skipped, (uint64_t)0);
    ResolveLog log = {0};
    eng_gpu_set_resolve(&f.gpu, log_resolve, &log);
    eng_gpu_set_resolve(&f.gpu, NULL, NULL); /* retirée */
    ASSERT_TRUE(run_frames(&f, 1));
    EXPECT_EQ(log.calls, 0);
    close_fixture(&f);
}

UTEST(eng_gpu_hdr, without_hdr_there_is_no_resolve)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ResolveLog log = {0};
    eng_gpu_set_resolve(&f.gpu, log_resolve, &log); /* installée mais le HDR est éteint */
    ASSERT_TRUE(run_frames(&f, 2));
    EXPECT_EQ(log.calls, 0);
    EXPECT_EQ(f.gpu.resolve_passes, (uint64_t)0);
    close_fixture(&f);
}

UTEST(eng_gpu_hdr, the_hdr_texture_follows_the_swapchain_size_without_failing)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(eng_gpu_enable_hdr(&f.gpu));
    ASSERT_TRUE(eng_gpu_enable_depth(&f.gpu));
    ResolveLog log = {0};
    eng_gpu_set_resolve(&f.gpu, log_resolve, &log);
    const int sizes[][2] = {{640, 360}, {200, 120}, {1024, 768}, {321, 199}};
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        ASSERT_TRUE(SDL_SetWindowSize(f.platform.window, sizes[i][0], sizes[i][1]));
        (void)SDL_SyncWindow(f.platform.window);
        ASSERT_TRUE(run_frames(&f, 3));
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
        EXPECT_TRUE(f.gpu.hdr_texture != NULL);
        EXPECT_EQ((int)f.gpu.hdr_width, sizes[i][0]);
        EXPECT_EQ((int)f.gpu.hdr_height, sizes[i][1]);
        EXPECT_EQ(f.gpu.hdr_width, f.gpu.swapchain_width);
        EXPECT_EQ(f.gpu.hdr_height, f.gpu.swapchain_height);
        eng_gpu_end_frame(&f.gpu);
    }
    for (int i = 0; i < 60; i++) { /* redimensionnements rapides : ni échec ni fuite */
        ASSERT_TRUE(
            SDL_SetWindowSize(f.platform.window, 160 + (i * 37) % 700, 120 + (i * 53) % 500));
        (void)eng_platform_pump(&f.platform);
        ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
        ASSERT_TRUE(eng_gpu_begin_render_pass(&f.gpu) != NULL);
        eng_gpu_end_frame(&f.gpu);
    }
    EXPECT_EQ(f.gpu.frames_skipped, (uint64_t)0);
    close_fixture(&f);
}

UTEST(eng_gpu_hdr, the_resolve_target_clears_to_opaque_black_and_stores)
{
    SDL_GPUColorTargetInfo target;
    int fake = 0; /* seule l'adresse compte : la fonction ne la déréférence pas */
    SDL_GPUTexture *texture = (SDL_GPUTexture *)&fake;
    eng_gpu_fill_resolve_target(texture, &target);
    EXPECT_TRUE(target.texture == texture);
    EXPECT_EQ((int)target.load_op, (int)SDL_GPU_LOADOP_CLEAR);
    EXPECT_EQ((int)target.store_op, (int)SDL_GPU_STOREOP_STORE);
    EXPECT_NEAR(target.clear_color.r, 0.0F, 0.0F);
    EXPECT_NEAR(target.clear_color.g, 0.0F, 0.0F);
    EXPECT_NEAR(target.clear_color.b, 0.0F, 0.0F);
    EXPECT_NEAR(target.clear_color.a, 1.0F, 0.0F);
}

/* ── Images en vol, barrières, redimensionnement (M1-18) ── */

UTEST(eng_gpu_flight, defaults_to_two_and_follows_the_request)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    EXPECT_EQ(eng_gpu_frames_in_flight(&f.gpu), ENG_GPU_DEFAULT_FRAMES_IN_FLIGHT);
    close_fixture(&f);
    for (uint32_t n = 1; n <= ENG_GPU_MAX_FRAMES_IN_FLIGHT; n++) {
        Fixture g = {0};
        ASSERT_TRUE(open_fixture_n(&g, n));
        EXPECT_EQ(eng_gpu_frames_in_flight(&g.gpu), n);
        close_fixture(&g);
    }
    /* une valeur hors de 1..3 donne la valeur par défaut */
    Fixture h = {0};
    ASSERT_TRUE(open_fixture_n(&h, 9U));
    EXPECT_EQ(eng_gpu_frames_in_flight(&h.gpu), ENG_GPU_DEFAULT_FRAMES_IN_FLIGHT);
    close_fixture(&h);
}

UTEST(eng_gpu_flight, set_validates_the_range_and_keeps_the_old_value_on_refusal)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    EXPECT_FALSE(eng_gpu_set_frames_in_flight(&f.gpu, 0U));
    EXPECT_FALSE(eng_gpu_set_frames_in_flight(&f.gpu, ENG_GPU_MAX_FRAMES_IN_FLIGHT + 1U));
    EXPECT_EQ(eng_gpu_frames_in_flight(&f.gpu), ENG_GPU_DEFAULT_FRAMES_IN_FLIGHT);
    EXPECT_TRUE(eng_gpu_set_frames_in_flight(&f.gpu, 3U));
    EXPECT_EQ(eng_gpu_frames_in_flight(&f.gpu), 3U);
    EXPECT_TRUE(eng_gpu_set_frames_in_flight(&f.gpu, 1U));
    EXPECT_EQ(eng_gpu_frames_in_flight(&f.gpu), 1U);
    close_fixture(&f);
    /* GPU arrêté : rien ne marche, rien ne plante */
    EXPECT_EQ(eng_gpu_frames_in_flight(&f.gpu), 0U);
    EXPECT_FALSE(eng_gpu_set_frames_in_flight(&f.gpu, 2U));
    EXPECT_EQ(eng_gpu_frames_pending(&f.gpu), 0U);
    eng_gpu_wait_idle(&f.gpu);
}

UTEST(eng_gpu_flight, never_more_frames_in_flight_than_allowed)
{
    for (uint32_t n = 1; n <= ENG_GPU_MAX_FRAMES_IN_FLIGHT; n++) {
        Fixture f = {0};
        if (!open_fixture_n(&f, n)) {
            EXPECT_FALSE(gpu_required());
            return;
        }
        for (int i = 0; i < 60; i++) {
            (void)eng_platform_pump(&f.platform);
            ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
            /* à ce stade, au plus n - 1 images précédentes sont encore en vol */
            EXPECT_LT(f.gpu.fence_count, n);
            eng_gpu_end_frame(&f.gpu);
            EXPECT_GE(f.gpu.fence_count, 1U); /* celle qu'on vient de soumettre */
            EXPECT_LE(f.gpu.fence_count, n);
        }
        EXPECT_GE(f.gpu.max_pending, 1U);
        EXPECT_LE(f.gpu.max_pending, n);
        close_fixture(&f);
    }
}

UTEST(eng_gpu_flight, with_one_frame_in_flight_the_cpu_waits_for_every_frame)
{
    Fixture f = {0};
    if (!open_fixture_n(&f, 1U)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(run_frames(&f, 20));
    /* chaque image est finie (ou attendue) avant la suivante : jamais deux en vol */
    EXPECT_EQ(f.gpu.max_pending, 1U);
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)20);
    close_fixture(&f);
}

UTEST(eng_gpu_flight, lowering_the_limit_makes_the_next_frame_catch_up)
{
    Fixture f = {0};
    if (!open_fixture_n(&f, 3U)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(run_frames(&f, 10));
    ASSERT_TRUE(eng_gpu_set_frames_in_flight(&f.gpu, 1U));
    ASSERT_TRUE(eng_gpu_begin_frame(&f.gpu));
    EXPECT_EQ(f.gpu.fence_count, 0U); /* le GPU a rattrapé son retard avant cette image */
    eng_gpu_end_frame(&f.gpu);
    close_fixture(&f);
}

UTEST(eng_gpu_flight, every_fence_acquired_is_released_and_wait_idle_empties_the_ring)
{
    Fixture f = {0};
    if (!open_fixture_n(&f, 3U)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(run_frames(&f, 25));
    EXPECT_EQ(f.gpu.fences_acquired, f.gpu.frames_submitted);
    /* invariant à tout instant : obtenues = rendues + encore en vol */
    EXPECT_EQ(f.gpu.fences_acquired, f.gpu.fences_released + (uint64_t)f.gpu.fence_count);
    eng_gpu_wait_idle(&f.gpu);
    EXPECT_EQ(f.gpu.fence_count, 0U);
    EXPECT_EQ(eng_gpu_frames_pending(&f.gpu), 0U);
    EXPECT_EQ(f.gpu.fences_acquired, f.gpu.fences_released);
    eng_gpu_wait_idle(&f.gpu); /* répétable */
    EXPECT_EQ(f.gpu.fences_acquired, f.gpu.fences_released);
    close_fixture(&f);
}

typedef struct ResizeLog {
    uint32_t calls;
    uint32_t last_width;
    uint32_t last_height;
} ResizeLog;

static void log_resize(void *user, uint32_t width, uint32_t height)
{
    ResizeLog *log = (ResizeLog *)user;
    log->calls++;
    log->last_width = width;
    log->last_height = height;
}

UTEST(eng_gpu_flight, the_resize_callback_fires_once_per_size_change_with_the_new_size)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ResizeLog log = {0};
    eng_gpu_set_resize_callback(&f.gpu, log_resize, &log);
    ASSERT_TRUE(run_frames(&f, 3));
    EXPECT_EQ(log.calls, 1U); /* première image : taille initiale, puis plus rien */
    EXPECT_EQ(log.last_width, f.gpu.swapchain_width);
    EXPECT_EQ(log.last_height, f.gpu.swapchain_height);
    const int sizes[][2] = {{640, 360}, {200, 120}, {1024, 768}, {321, 199}};
    uint32_t expected = 1;
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        ASSERT_TRUE(SDL_SetWindowSize(f.platform.window, sizes[i][0], sizes[i][1]));
        (void)SDL_SyncWindow(f.platform.window);
        ASSERT_TRUE(run_frames(&f, 4)); /* plusieurs images à la même taille */
        expected++;
        EXPECT_EQ(log.calls, expected);
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(f.platform.window, &width, &height);
        EXPECT_EQ((int)log.last_width, width);
        EXPECT_EQ((int)log.last_height, height);
    }
    EXPECT_EQ((uint64_t)log.calls, f.gpu.resizes);
    /* sans rappel, le compteur avance quand même et rien ne plante */
    eng_gpu_set_resize_callback(&f.gpu, NULL, NULL);
    ASSERT_TRUE(SDL_SetWindowSize(f.platform.window, 500, 300));
    (void)SDL_SyncWindow(f.platform.window);
    ASSERT_TRUE(run_frames(&f, 3));
    EXPECT_EQ((uint64_t)log.calls + 1U, f.gpu.resizes);
    close_fixture(&f);
}

/* Compteurs de cibles : créées = libérées + vivantes, à tout instant. */
static bool targets_balanced(const EngGpu *gpu)
{
    const uint64_t alive =
        (gpu->depth_texture != NULL ? 1U : 0U) + (gpu->hdr_texture != NULL ? 1U : 0U);
    return gpu->targets_created == gpu->targets_released + alive;
}

UTEST(eng_gpu_flight, rapid_resizes_recreate_the_targets_without_leaking_any_for_every_limit)
{
    for (uint32_t n = 1; n <= ENG_GPU_MAX_FRAMES_IN_FLIGHT; n++) {
        Fixture f = {0};
        if (!open_fixture_n(&f, n)) {
            EXPECT_FALSE(gpu_required());
            return;
        }
        ASSERT_TRUE(eng_gpu_enable_depth(&f.gpu));
        ASSERT_TRUE(eng_gpu_enable_hdr(&f.gpu));
        ResizeLog log = {0};
        eng_gpu_set_resize_callback(&f.gpu, log_resize, &log);
        for (int i = 0; i < 150; i++) {
            const int width = 96 + (i * 37) % 800;
            const int height = 64 + (i * 53) % 600;
            ASSERT_TRUE(SDL_SetWindowSize(f.platform.window, width, height));
            (void)eng_platform_pump(&f.platform);
            ASSERT_TRUE(
                eng_gpu_begin_frame(&f.gpu)); /* sans attendre la fin du redimensionnement */
            EXPECT_TRUE(targets_balanced(&f.gpu));
            /* les cibles ont la taille de l'image en cours */
            EXPECT_EQ(f.gpu.depth_width, f.gpu.swapchain_width);
            EXPECT_EQ(f.gpu.depth_height, f.gpu.swapchain_height);
            EXPECT_EQ(f.gpu.hdr_width, f.gpu.swapchain_width);
            EXPECT_EQ(f.gpu.hdr_height, f.gpu.swapchain_height);
            eng_gpu_end_frame(&f.gpu);
        }
        EXPECT_EQ(f.gpu.frames_skipped, (uint64_t)0);
        EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)150);
        EXPECT_GT(f.gpu.resizes, (uint64_t)20); /* le test a bien changé la taille souvent */
        EXPECT_EQ((uint64_t)log.calls, f.gpu.resizes);
        /* 2 textures par changement de taille (profondeur, HDR), plus les premières */
        EXPECT_EQ(f.gpu.targets_created, 2U * f.gpu.resizes);
        EXPECT_TRUE(targets_balanced(&f.gpu));
        eng_gpu_wait_idle(&f.gpu);
        EXPECT_EQ(f.gpu.fences_acquired, f.gpu.fences_released);
        eng_gpu_disable_hdr(&f.gpu);
        EXPECT_TRUE(targets_balanced(&f.gpu)); /* désactiver le HDR rend sa texture */
        EXPECT_TRUE(f.gpu.hdr_texture == NULL);
        close_fixture(&f);
    }
}

/* GPU simulé : les barrières sont réelles (donc rendues à SDL, aucune fuite) mais c'est le test qui
 * décide quand elles sont « finies ». `done` : nombre d'images soumises que le GPU simulé a finies,
 * dans l'ordre. */
typedef struct FakeGpu {
    void *submitted[64];
    uint32_t submitted_count;
    uint32_t done;
    uint32_t waits;
    bool fast;       /* true : tout est fini tout de suite */
    bool wait_fails; /* true : l'attente échoue */
} FakeGpu;

/* SDL recycle ses barrières : un même pointeur désigne successivement plusieurs images. La plus
 * récente soumise avec ce pointeur est la seule encore en vol, donc on cherche en partant de la
 * fin. */
static uint32_t fake_index(const FakeGpu *g, void *fence)
{
    for (uint32_t i = g->submitted_count; i > 0; i--) {
        if (g->submitted[i - 1U] == fence) {
            return i - 1U;
        }
    }
    return 0xFFFFFFFFU;
}

static bool fake_query(void *user, struct SDL_GPUFence *fence)
{
    const FakeGpu *g = (const FakeGpu *)user;
    return g->fast || fake_index(g, fence) < g->done;
}

static bool fake_wait(void *user, struct SDL_GPUFence *fence)
{
    FakeGpu *g = (FakeGpu *)user;
    g->waits++;
    if (g->wait_fails) {
        return false;
    }
    const uint32_t index = fake_index(g, fence);
    if (index != 0xFFFFFFFFU && index + 1U > g->done) {
        g->done = index + 1U;
    }
    return true;
}

/* Une image vide ; retourne le nombre d'images en vol vu par begin_frame (avant la soumission). */
static uint32_t fake_frame(Fixture *f, FakeGpu *g)
{
    (void)eng_platform_pump(&f->platform);
    if (!eng_gpu_begin_frame(&f->gpu)) {
        return 0xFFFFFFFFU;
    }
    const uint32_t at_begin = f->gpu.fence_count;
    eng_gpu_end_frame(&f->gpu);
    g->submitted[g->submitted_count++] = f->gpu.fences[f->gpu.fence_count - 1U];
    return at_begin;
}

UTEST(eng_gpu_flight, a_simulated_slow_gpu_makes_begin_frame_wait_exactly_at_the_limit)
{
    for (uint32_t n = 1; n <= ENG_GPU_MAX_FRAMES_IN_FLIGHT; n++) {
        Fixture f = {0};
        if (!open_fixture_n(&f, n)) {
            EXPECT_FALSE(gpu_required());
            return;
        }
        FakeGpu g = {0};
        eng_gpu_set_fence_hooks(&f.gpu, fake_query, fake_wait, &g);
        const uint32_t frames = 12;
        for (uint32_t i = 0; i < frames; i++) {
            const uint32_t at_begin = fake_frame(&f, &g);
            ASSERT_NE(at_begin, 0xFFFFFFFFU);
            /* le GPU ne finit rien tout seul : on attend dès que n images sont en vol */
            EXPECT_EQ(at_begin, i < n ? i : n - 1U);
        }
        EXPECT_EQ(g.waits, frames - n);
        EXPECT_EQ(f.gpu.fence_waits, (uint64_t)(frames - n));
        EXPECT_EQ(f.gpu.max_pending, n);
        EXPECT_EQ(f.gpu.fence_count, n);
        eng_gpu_set_fence_hooks(&f.gpu, NULL, NULL, NULL); /* le vrai GPU pour finir */
        eng_gpu_wait_idle(&f.gpu);
        EXPECT_TRUE(eng_gpu_resources_balanced(&f.gpu));
        close_fixture(&f);
    }
}

UTEST(eng_gpu_flight, a_simulated_fast_gpu_never_makes_the_cpu_wait)
{
    Fixture f = {0};
    if (!open_fixture_n(&f, 2U)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    FakeGpu g = {.fast = true};
    eng_gpu_set_fence_hooks(&f.gpu, fake_query, fake_wait, &g);
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(fake_frame(&f, &g), 0U); /* tout est déjà fini quand l'image suivante commence */
    }
    EXPECT_EQ(g.waits, 0U);
    EXPECT_EQ(f.gpu.fence_waits, (uint64_t)0);
    EXPECT_EQ(f.gpu.max_pending, 1U);
    EXPECT_EQ(f.gpu.fences_released, (uint64_t)19); /* une seule reste en vol */
    eng_gpu_set_fence_hooks(&f.gpu, NULL, NULL, NULL);
    close_fixture(&f);
}

UTEST(eng_gpu_flight, fences_are_retired_in_order_without_waiting)
{
    Fixture f = {0};
    if (!open_fixture_n(&f, 3U)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    FakeGpu g = {0};
    eng_gpu_set_fence_hooks(&f.gpu, fake_query, fake_wait, &g);
    for (int i = 0; i < 3; i++) {
        (void)fake_frame(&f, &g);
    }
    EXPECT_EQ(eng_gpu_frames_pending(&f.gpu), 3U);
    g.done = 1; /* le GPU a fini la première image */
    EXPECT_EQ(eng_gpu_frames_pending(&f.gpu), 2U);
    EXPECT_EQ(g.waits, 0U); /* interroger n'attend pas */
    g.done = 2;
    EXPECT_EQ(eng_gpu_frames_pending(&f.gpu), 1U);
    g.done = 3;
    EXPECT_EQ(eng_gpu_frames_pending(&f.gpu), 0U);
    EXPECT_TRUE(eng_gpu_resources_balanced(&f.gpu));
    /* les emplacements libérés de l'anneau sont remis à zéro */
    for (uint32_t i = 0; i < ENG_GPU_MAX_FRAMES_IN_FLIGHT; i++) {
        EXPECT_TRUE(f.gpu.fences[i] == NULL);
    }
    /* plusieurs images finies d'un coup sont toutes rendues par UN SEUL appel, sans attente */
    for (int i = 0; i < 3; i++) {
        (void)fake_frame(&f, &g);
    }
    EXPECT_EQ(eng_gpu_frames_pending(&f.gpu),
              g.submitted_count - g.done); /* rien de fini en plus */
    g.done = g.submitted_count;
    EXPECT_EQ(eng_gpu_frames_pending(&f.gpu), 0U);
    EXPECT_EQ(g.waits, 0U);
    EXPECT_TRUE(eng_gpu_resources_balanced(&f.gpu));
    eng_gpu_set_fence_hooks(&f.gpu, NULL, NULL, NULL);
    close_fixture(&f);
}

UTEST(eng_gpu_flight, wait_idle_waits_for_every_pending_frame_and_lowering_the_limit_catches_up)
{
    Fixture f = {0};
    if (!open_fixture_n(&f, 3U)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    FakeGpu g = {0};
    eng_gpu_set_fence_hooks(&f.gpu, fake_query, fake_wait, &g);
    for (int i = 0; i < 3; i++) {
        (void)fake_frame(&f, &g);
    }
    eng_gpu_wait_idle(&f.gpu);
    EXPECT_EQ(g.waits, 3U);
    EXPECT_EQ(f.gpu.fence_count, 0U);
    for (int i = 0; i < 3; i++) {
        (void)fake_frame(&f, &g);
    }
    ASSERT_TRUE(eng_gpu_set_frames_in_flight(&f.gpu, 1U));
    g.waits = 0;
    EXPECT_EQ(fake_frame(&f, &g), 0U); /* les 3 en vol ont été attendues avant cette image */
    EXPECT_EQ(g.waits, 3U);
    EXPECT_TRUE(eng_gpu_resources_balanced(&f.gpu));
    eng_gpu_set_fence_hooks(&f.gpu, NULL, NULL, NULL);
    close_fixture(&f);
}

UTEST(eng_gpu_flight, a_failing_wait_neither_hangs_nor_leaks_the_fence)
{
    Fixture f = {0};
    if (!open_fixture_n(&f, 1U)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    FakeGpu g = {.wait_fails = true};
    eng_gpu_set_fence_hooks(&f.gpu, fake_query, fake_wait, &g);
    for (int i = 0; i < 8; i++) {
        ASSERT_NE(fake_frame(&f, &g), 0xFFFFFFFFU);
    }
    EXPECT_EQ(g.waits, 7U); /* chaque image attend la précédente, l'échec est ignoré */
    EXPECT_EQ(f.gpu.frames_submitted, (uint64_t)8);
    EXPECT_EQ(f.gpu.frames_skipped, (uint64_t)0);
    EXPECT_TRUE(eng_gpu_resources_balanced(&f.gpu));
    eng_gpu_set_fence_hooks(&f.gpu, NULL, NULL, NULL);
    close_fixture(&f);
}

UTEST(eng_gpu_flight, the_resize_callback_fires_when_only_one_dimension_changes)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ResizeLog log = {0};
    eng_gpu_set_resize_callback(&f.gpu, log_resize, &log);
    ASSERT_TRUE(SDL_SetWindowSize(f.platform.window, 400, 300));
    (void)SDL_SyncWindow(f.platform.window);
    ASSERT_TRUE(run_frames(&f, 3));
    const uint32_t calls = log.calls;
    ASSERT_TRUE(SDL_SetWindowSize(f.platform.window, 500, 300)); /* largeur seule */
    (void)SDL_SyncWindow(f.platform.window);
    ASSERT_TRUE(run_frames(&f, 3));
    EXPECT_EQ(log.calls, calls + 1U);
    EXPECT_EQ(log.last_width, 500U);
    EXPECT_EQ(log.last_height, 300U);
    ASSERT_TRUE(SDL_SetWindowSize(f.platform.window, 500, 350)); /* hauteur seule */
    (void)SDL_SyncWindow(f.platform.window);
    ASSERT_TRUE(run_frames(&f, 3));
    EXPECT_EQ(log.calls, calls + 2U);
    EXPECT_EQ(log.last_height, 350U);
    close_fixture(&f);
}

UTEST(eng_gpu_flight, the_balance_check_detects_a_missing_release)
{
    Fixture f = {0};
    if (!open_fixture(&f)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(eng_gpu_enable_depth(&f.gpu));
    ASSERT_TRUE(run_frames(&f, 5));
    EXPECT_TRUE(eng_gpu_resources_balanced(&f.gpu));
    f.gpu.fences_acquired++; /* une barrière « perdue » */
    EXPECT_FALSE(eng_gpu_resources_balanced(&f.gpu));
    f.gpu.fences_acquired--;
    EXPECT_TRUE(eng_gpu_resources_balanced(&f.gpu));
    f.gpu.targets_created++; /* une cible « perdue » */
    EXPECT_FALSE(eng_gpu_resources_balanced(&f.gpu));
    f.gpu.targets_created--;
    f.gpu.targets_released++; /* ou libérée deux fois */
    EXPECT_FALSE(eng_gpu_resources_balanced(&f.gpu));
    f.gpu.targets_released--;
    EXPECT_TRUE(eng_gpu_resources_balanced(&f.gpu));
    close_fixture(&f);
}

UTEST(eng_gpu_flight, shutdown_returns_every_fence_and_target_even_with_frames_still_pending)
{
    Fixture f = {0};
    if (!open_fixture_n(&f, 3U)) {
        EXPECT_FALSE(gpu_required());
        return;
    }
    ASSERT_TRUE(eng_gpu_enable_depth(&f.gpu));
    ASSERT_TRUE(eng_gpu_enable_hdr(&f.gpu));
    FakeGpu g = {0}; /* GPU simulé lent : trois images restent « en vol » jusqu'à l'arrêt */
    eng_gpu_set_fence_hooks(&f.gpu, fake_query, fake_wait, &g);
    for (int i = 0; i < 3; i++) {
        (void)fake_frame(&f, &g);
    }
    EXPECT_EQ(f.gpu.fence_count, 3U);
    EXPECT_TRUE(eng_gpu_shutdown(&f.gpu)); /* barrières rendues, textures libérées */
    EXPECT_TRUE(eng_gpu_shutdown(&f.gpu)); /* répétable */
    eng_platform_shutdown(&f.platform);
}

UTEST(eng_gpu_flight, shutdown_reports_a_resource_that_was_not_returned)
{
    for (int kind = 0; kind < 3; kind++) {
        Fixture f = {0};
        if (!open_fixture(&f)) {
            EXPECT_FALSE(gpu_required());
            return;
        }
        ASSERT_TRUE(eng_gpu_enable_depth(&f.gpu));
        ASSERT_TRUE(run_frames(&f, 3));
        if (kind == 0) {
            f.gpu.fences_acquired++; /* une barrière jamais rendue */
        } else if (kind == 1) {
            f.gpu.targets_created++; /* une cible jamais libérée */
        } else {
            f.gpu.fences_released++; /* une barrière rendue deux fois */
        }
        EXPECT_FALSE(eng_gpu_shutdown(&f.gpu));
        eng_platform_shutdown(&f.platform);
    }
}
