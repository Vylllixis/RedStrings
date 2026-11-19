#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <math.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/assert.h"
#include "engine/clock.h"
#include "engine/camera.h"
#include "engine/config.h"
#include "engine/engine.h"
#include "engine/files.h"
#include "engine/game_host.h"
#include "engine/gpu.h"
#include "engine/lsan.h"
#include "engine/log.h"
#include "engine/loop.h"
#include "engine/memory.h"
#include "engine/platform.h"
#include "engine/profile.h"
#include "engine/renderer.h"
#include "engine/frame_stats.h"
#include "engine/lighting.h"
#include "engine/shadow.h"
#include "engine/tweak.h"
#include "engine/ui.h"

#include "demo_scene.h"
#include "editor_mode.h"
#if defined(ENG_HAVE_CRASH)
#include "engine/crash.h"
#endif
#include "game_flow.h"
#include "engine/daynight.h"
#include "play_mode.h"
#include "world_bench.h"
#include "skeleton_view.h"

ENG_LSAN_DEFAULT_SUPPRESSIONS();

static void usage(const char *prog)
{
    fprintf(stderr, "usage : %s [--frames N] [--log-level trace|debug|info|warn|error]\n", prog);
    fprintf(stderr, "          [--game chemin_de_la_bibliotheque] [--config chemin]\n");
    fprintf(
        stderr,
        "          [--software] [--assert-test] [--no-mipmaps] [--anisotropy N] [--ui]\n"
        "          [--env image.hdr] [--env-samples N] [--frames-in-flight N] [--stress-resize]\n"
        "          [--watch-assets] [--sun-elevation DEG] [--sun-azimuth DEG]\n          "
        "[--day-night HEURE] [--day-length S]\n          "
        "[--fog-density 1/M] [--rocks N] [--placement] [--placement-seed N] [--grass]\n          "
        "[--water] [--water-level M]\n");
    fprintf(stderr, "  --frames N     quitte après N images (tests automatiques)\n");
    fprintf(stderr,
            "  --log-level L  niveau du journal (sinon variable ENG_LOG_LEVEL, sinon info)\n");
    fprintf(stderr,
            "  --game PATH    bibliothèque du jeu (défaut : game.so à côté de l'exécutable)\n");
    fprintf(
        stderr,
        "  --config PATH  fichier de configuration (défaut : config.ini à côté de l'exécutable)\n");
    fprintf(stderr,
            "  --watch-assets démo : recuit et recharge le modèle quand ses sources changent\n");
    fprintf(
        stderr,
        "  --terrain PATH démo : dessine le champ de hauteurs .hmap (assetc heightmap) à la place "
        "de la démonstration ; point de vue au-dessus du centre\n");
    fprintf(stderr,
            "  --editor       éditeur : charge la scène, clic = sélection, flèches = déplacement, "
            "fenêtre Éditeur (Enregistrer)\n");
    fprintf(stderr, "  --scene PATH   scène de l'éditeur (défaut " EDITOR_DEFAULT_SCENE
                    ", relatif à l'exécutable ; chemin absolu accepté)\n");
    fprintf(stderr,
            "  --editor-selftest (développement) clique, déplace, enregistre et relit, puis "
            "journalise AUTOTEST ÉDITEUR\n");
    fprintf(
        stderr,
        "  --play         mode jouable : personnage, caméra 3e personne, saut, sprint, arène en "
        "boîtes grises (ZQSD, souris, Espace, Maj)\n");
    fprintf(stderr, "  --skeleton F   affiche le squelette F (.skel, voir `assetc skel`) en os "
                    "superposés au modèle central\n");
    fprintf(stderr, "  --play-selftest (développement) autotest du mode jouable sans fenêtre, "
                    "journalise AUTOTEST JEU\n");
    fprintf(stderr, "  --combat       mode jouable + combat : mannequin, camp de 3 ennemis (clic "
                    "gauche/droit, Ctrl, E, Q) ; --no-audio : sans son\n");
    fprintf(stderr,
            "  --crash-test   (développement) plantage volontaire : écrit logs/crash-*.txt\n");
    fprintf(stderr, "  --new-game     avec --combat : saute le menu principal (nouvelle partie)\n");
    fprintf(stderr,
            "  --game-selftest (développement) partie, sauvegarde, relance, reprise sans fenêtre ; "
            "journalise AUTOTEST PARTIE\n");
    fprintf(stderr, "  --combat-autoplay  comme --combat, joué par un pilote automatique "
                    "(démonstration)\n");
    fprintf(stderr, "  --screenshot F N (développement) écrit l'image n° N en PPM dans F\n");
    fprintf(stderr,
            "  --world-bench [KM] benchmark de traversée du monde ouvert (défaut 10 km à 30 "
            "m/s), sans fenêtre ; --world-bench-report F : rapport écrit dans F\n");
    fprintf(stderr, "  --combat-selftest (développement) autotest du combat sans fenêtre, "
                    "journalise AUTOTEST COMBAT\n");
    fprintf(stderr, "  --no-mipmaps   démo : texture du cube sans mipmaps (montre l'aliasing)\n");
    fprintf(stderr, "  --anisotropy N démo : filtrage anisotrope de 0 (aucun) à 16 (défaut 16)\n");
    fprintf(stderr, "  --env FICHIER  démo : ciel = image équirectangulaire Radiance .hdr (sinon "
                    "ciel procédural)\n");
    fprintf(stderr, "  --env-samples N démo : échantillons du préfiltrage du ciel (défaut 512 ; 16 "
                    ": rapide)\n");
    fprintf(stderr,
            "  --frames-in-flight N images préparées par le CPU pendant que le GPU dessine (1 à 3, "
            "défaut 2)\n");
    fprintf(stderr,
            "  --stress-resize (développement) change la taille de la fenêtre à chaque image, "
            "puis vérifie\n                 qu'aucune cible ni barrière n'a fui (code de sortie 1 "
            "sinon)\n");
    fprintf(stderr,
            "  --ui           démarre avec l'interface de debug affichée (sinon F1 l'affiche)\n");
    fprintf(stderr, "  --software     affichage logiciel (sans GPU) : montre le carré du jeu\n");
    fprintf(stderr, "  --assert-test  (développement) déclenche volontairement deux assertions\n");
}

/* Dessins de debug du lanceur (mode jouable, squelette), appelés par la scène
 * (demo_scene_set_debug_hook). */
typedef struct PlayHook {
    const PlayMode *play;   /* NULL : pas de mode jouable */
    SkeletonView *skeleton; /* NULL : pas de squelette */
    double alpha;
} PlayHook;

static void launcher_debug_hook(void *user, EngDebugLines *lines, mat4 model, bool has_model)
{
    const PlayHook *hook = user;
    if (hook->play != NULL) {
        play_mode_debug_draw(hook->play, lines, hook->alpha);
    }
    if (hook->skeleton != NULL && has_model) {
        (void)skeleton_view_draw(hook->skeleton, lines, model);
    }
}

int main(int argc, char **argv)
{
    eng_mem_track_install(); /* avant toute allocation de SDL */

    long max_frames = -1; /* -1 : jusqu'à la fermeture de la fenêtre */
    bool assert_test = false;
    bool force_software = false;
    bool start_with_ui = false;
    bool stress_resize = false;
    bool editor_enabled = false;
    bool editor_selftest = false;
    bool play_enabled = false;
    bool play_selftest = false;
    bool combat_enabled = false;
    bool combat_audio = true;
    bool combat_selftest = false;
    bool combat_autoplay = false;
    double world_bench_km = 0.0;
    const char *terrain_selftest_path = NULL;
    bool game_selftest = false;
    bool start_new_game = false;
    bool crash_test = false;
    const char *world_bench_report = NULL;
    const char *screenshot_path = NULL;
    long screenshot_frame = 0;
    const char *skeleton_path = NULL;
    const char *anim_path = NULL;
    const char *sockets_path = NULL;
    const char *scene_arg = NULL;
    uint32_t frames_in_flight = 0; /* 0 : valeur par défaut du moteur */
    DemoOptions demo_options = {.mipmaps = true,
                                .placement_seed = 1,
                                .anisotropy = 16.0F,
                                .sun_elevation = 45.0F,
                                .sun_azimuth = 30.0F,
                                .fog_density = -1.0F};
    bool use_day_night = false; /* --day-night / --day-length (M8-07) ; la partie l'active aussi */
    double day_night_hour = -1.0;
    double day_length = 0.0;
    const char *game_path = NULL;
    const char *config_path = NULL;
    EngLogLevel log_level = eng_log_env_level(ENG_LEVEL_INFO);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            char *end = NULL;
            max_frames = strtol(argv[++i], &end, 10);
            if (*end != '\0' || max_frames < 0) {
                usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            if (!eng_log_parse_level(argv[++i], &log_level)) {
                usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
            game_path = argv[++i];
        } else if (strcmp(argv[i], "--editor") == 0) {
            editor_enabled = true;
        } else if (strcmp(argv[i], "--editor-selftest") == 0) {
            editor_enabled = true;
            editor_selftest = true;
        } else if (strcmp(argv[i], "--play") == 0) {
            play_enabled = true;
        } else if (strcmp(argv[i], "--play-selftest") == 0) {
            play_selftest = true;
        } else if (strcmp(argv[i], "--combat") == 0) {
            play_enabled = true;
            combat_enabled = true;
        } else if (strcmp(argv[i], "--crash-test") == 0) {
            crash_test = true;
        } else if (strcmp(argv[i], "--new-game") == 0) {
            start_new_game = true;
        } else if (strcmp(argv[i], "--game-selftest") == 0) {
            game_selftest = true;
        } else if (strcmp(argv[i], "--play-terrain-selftest") == 0 && i + 1 < argc) {
            terrain_selftest_path = argv[++i];
        } else if (strcmp(argv[i], "--world-bench") == 0) {
            world_bench_km = 10.0;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                world_bench_km = strtod(argv[++i], NULL);
            }
        } else if (strcmp(argv[i], "--world-bench-report") == 0 && i + 1 < argc) {
            world_bench_report = argv[++i];
        } else if (strcmp(argv[i], "--combat-autoplay") == 0) {
            play_enabled = true;
            combat_enabled = true;
            combat_autoplay = true;
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 2 < argc) {
            screenshot_path = argv[++i];
            screenshot_frame = strtol(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--no-audio") == 0) {
            combat_audio = false;
        } else if (strcmp(argv[i], "--combat-selftest") == 0) {
            combat_selftest = true;
        } else if (strcmp(argv[i], "--skeleton") == 0 && i + 1 < argc) {
            skeleton_path = argv[++i];
        } else if (strcmp(argv[i], "--anim") == 0 && i + 1 < argc) {
            anim_path = argv[++i];
        } else if (strcmp(argv[i], "--sockets") == 0 && i + 1 < argc) {
            sockets_path = argv[++i];
        } else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            scene_arg = argv[++i];
        } else if ((strcmp(argv[i], "--sun-elevation") == 0 ||
                    strcmp(argv[i], "--sun-azimuth") == 0) &&
                   i + 1 < argc) {
            char *end = NULL;
            const bool is_elevation = strcmp(argv[i], "--sun-elevation") == 0;
            const float value = strtof(argv[++i], &end);
            if (*end != '\0' || !isfinite(value) || value < -180.0F || value > 180.0F ||
                (is_elevation && (value < -90.0F || value > 90.0F))) {
                usage(argv[0]);
                return 2;
            }
            if (is_elevation) {
                demo_options.sun_elevation = value;
            } else {
                demo_options.sun_azimuth = value;
            }
        } else if ((strcmp(argv[i], "--day-night") == 0 || strcmp(argv[i], "--day-length") == 0) &&
                   i + 1 < argc) {
            const bool is_hour = strcmp(argv[i], "--day-night") == 0;
            char *end = NULL;
            const double value = strtod(argv[++i], &end);
            if (*end != '\0' || !isfinite(value) || value < 0.0 ||
                (is_hour ? value >= 24.0 : (value < 1.0 || value > 1e6))) {
                usage(argv[0]);
                return 2;
            }
            use_day_night = true;
            if (is_hour) {
                day_night_hour = value;
            } else {
                day_length = value;
            }
        } else if (strcmp(argv[i], "--fog-density") == 0 && i + 1 < argc) {
            char *end = NULL;
            const float value = strtof(argv[++i], &end);
            if (*end != '\0' || !isfinite(value) || value < 0.0F || value > 1.0F) {
                usage(argv[0]);
                return 2;
            }
            demo_options.fog_density = value;
        } else if (strcmp(argv[i], "--rocks") == 0 && i + 1 < argc) {
            char *end = NULL;
            const unsigned long value = strtoul(argv[++i], &end, 10);
            if (*end != '\0' || end == argv[i] || value > 1000000UL) {
                usage(argv[0]);
                return 2;
            }
            demo_options.rock_count = (uint32_t)value;
        } else if (strcmp(argv[i], "--grass") == 0) {
            demo_options.grass = true;
        } else if (strcmp(argv[i], "--water") == 0) {
            demo_options.water = true;
        } else if (strcmp(argv[i], "--water-level") == 0 && i + 1 < argc) {
            char *end = NULL;
            const float value = strtof(argv[++i], &end);
            if (*end != '\0' || end == argv[i] || !isfinite(value) || fabsf(value) > 100000.0F) {
                usage(argv[0]);
                return 2;
            }
            demo_options.water = true;
            demo_options.water_level_set = true;
            demo_options.water_level = value;
        } else if (strcmp(argv[i], "--placement") == 0) {
            demo_options.placement = true;
        } else if (strcmp(argv[i], "--placement-seed") == 0 && i + 1 < argc) {
            char *end = NULL;
            const unsigned long value = strtoul(argv[++i], &end, 10);
            if (*end != '\0' || end == argv[i] || value > 0xFFFFFFF0UL) {
                usage(argv[0]);
                return 2;
            }
            demo_options.placement_seed = (uint32_t)value;
        } else if (strcmp(argv[i], "--terrain") == 0 && i + 1 < argc) {
            demo_options.terrain_path = argv[++i];
        } else if (strcmp(argv[i], "--watch-assets") == 0) {
            demo_options.watch_assets = true;
        } else if (strcmp(argv[i], "--no-mipmaps") == 0) {
            demo_options.mipmaps = false;
        } else if (strcmp(argv[i], "--anisotropy") == 0 && i + 1 < argc) {
            char *end = NULL;
            demo_options.anisotropy = strtof(argv[++i], &end);
            if (*end != '\0' || !(demo_options.anisotropy >= 0.0F)) {
                usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[i], "--env") == 0 && i + 1 < argc) {
            demo_options.environment_path = argv[++i];
        } else if (strcmp(argv[i], "--env-samples") == 0 && i + 1 < argc) {
            char *end = NULL;
            const long samples = strtol(argv[++i], &end, 10);
            if (*end != '\0' || samples < 1 || samples > 65536) {
                usage(argv[0]);
                return 2;
            }
            demo_options.environment_samples = (uint32_t)samples;
        } else if (strcmp(argv[i], "--frames-in-flight") == 0 && i + 1 < argc) {
            char *end = NULL;
            const long count = strtol(argv[++i], &end, 10);
            if (*end != '\0' || count < 1 || count > (long)ENG_GPU_MAX_FRAMES_IN_FLIGHT) {
                usage(argv[0]);
                return 2;
            }
            frames_in_flight = (uint32_t)count;
        } else if (strcmp(argv[i], "--stress-resize") == 0) {
            stress_resize = true;
        } else if (strcmp(argv[i], "--ui") == 0) {
            start_with_ui = true;
        } else if (strcmp(argv[i], "--software") == 0) {
            force_software = true;
        } else if (strcmp(argv[i], "--assert-test") == 0) {
            assert_test = true;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    /* Si le fichier ne peut pas être créé, l'erreur est déjà journalisée : on continue
     * avec la console seule. */
    char log_dir[512];
    if (!eng_path_resolve(log_dir, sizeof log_dir, "logs")) {
        snprintf(log_dir, sizeof log_dir, "logs"); /* repli : relatif au dossier courant */
    }
    (void)eng_log_init(log_dir, log_level);

    ENG_LOG_INFO("engine %s", eng_version());
    /* M9-03 : rapport de crash à côté du journal */
#if defined(ENG_HAVE_CRASH)
    if (eng_crash_install(log_dir, eng_version())) {
        eng_crash_note("journal", log_dir);
    }
#else
    ENG_LOG_WARN("rapport de crash désactivé (engine/src/crash.c absent de cette build)");
#endif
    if (crash_test) { /* (développement) plantage volontaire : le rapport doit être écrit */
#if defined(__SANITIZE_ADDRESS__)
        /* sous ASan/UBSan, une écriture nulle est interceptée avant le signal : on l'envoie
         * directement (même gestionnaire, même rapport) */
        ENG_LOG_WARN("test de crash : signal SIGSEGV (build instrumenté)");
        (void)raise(SIGSEGV);
#else
        ENG_LOG_WARN("test de crash : écriture à l'adresse nulle");
        volatile int *volatile null_ptr = NULL;
        *null_ptr = 42; /* NOLINT(clang-analyzer-core.NullDereference) : plantage voulu */
#endif
    }
    ENG_LOG_INFO("journal dans %s", log_dir);
    eng_print_deps();

    if (game_selftest) { /* sans fenêtre : nouvelle partie, sauvegarde, relance, reprise */
        const bool game_ok = game_flow_selftest();
        eng_log_shutdown();
        return game_ok ? 0 : 1;
    }
    if (terrain_selftest_path != NULL) { /* sans fenêtre : personnage sur un terrain .hmap */
        const bool terrain_ok = play_mode_terrain_selftest(terrain_selftest_path);
        eng_log_shutdown();
        return terrain_ok ? 0 : 1;
    }
    if (world_bench_km > 0.0) { /* sans fenêtre : traversée du monde ouvert mesurée */
        const bool bench_ok = world_bench_run(world_bench_km, 30.0, world_bench_report);
        eng_log_shutdown();
        return bench_ok ? 0 : 1;
    }
    if (combat_selftest) { /* sans fenêtre ni GPU : personnage, combat et IA scriptés */
        const bool combat_ok = play_combat_selftest();
        eng_log_shutdown();
        return combat_ok ? 0 : 1;
    }
    if (play_selftest) { /* sans fenêtre ni GPU : physique et entrées scriptées seulement */
        const bool play_ok = play_mode_selftest();
        eng_log_shutdown();
        return play_ok ? 0 : 1;
    }

    if (assert_test) {
        int answer = 41;
        ENG_LOG_INFO("test d'assertion (assertions %s)",
                     ENG_ASSERTS_ENABLED ? "actives" : "désactivées");
        ENG_ASSERT(argc > 0); /* vraie : ne doit rien afficher */
        ENG_ASSERT_MSG(answer == 42, "la réponse vaut %d au lieu de 42", answer);
        ENG_LOG_INFO("l'exécution a repris après la première assertion");
        ENG_ASSERT(answer == 42);
        ENG_LOG_INFO("l'exécution a repris après la seconde assertion");
    }

    /* Configuration : valeurs par défaut si le fichier est absent ; les lignes invalides sont
     * signalées puis ignorées. */
    char default_config_path[512];
    if (config_path == NULL) {
        if (eng_path_resolve(default_config_path, sizeof default_config_path, "config.ini")) {
            config_path = default_config_path;
        } else {
            config_path = "config.ini";
        }
    }
    EngConfig config;
    EngArena config_scratch;
    if (!eng_arena_init(&config_scratch, "config", ENG_CONFIG_MAX_FILE_SIZE + 4096)) {
        eng_log_shutdown();
        return 1;
    }
    if (eng_config_load(&config, &config_scratch, config_path)) {
        ENG_LOG_INFO("configuration lue : %s (%d ligne(s) ignorée(s))", config_path,
                     config.issue_count);
    } else {
        ENG_LOG_INFO("pas de configuration (%s) : valeurs par défaut", config_path);
    }
    eng_arena_shutdown(&config_scratch);

    EngWindowDesc desc = {
        .title = "Open World",
        .width = config.window_width,
        .height = config.window_height,
        .resizable = true,
        .fullscreen = config.fullscreen,
        .vsync = config.vsync,
        .no_software_surface = !force_software, /* la fenêtre ira au GPU, sauf repli */
    };
    EngPlatform platform;
    if (!eng_platform_init(&platform, &desc)) {
        eng_log_shutdown();
        return 1;
    }

    /* GPU : si aucun pilote n'est utilisable (pas de Vulkan, pilote vidéo factice des tests),
     * on se rabat sur l'affichage logiciel provisoire. */
    EngGpu gpu = {0};
    bool use_gpu = false;
    if (!force_software) {
        const EngGpuDesc gpu_desc = {
#ifndef NDEBUG
            .debug = true,
#endif
            .present_mode = config.vsync ? ENG_PRESENT_VSYNC : ENG_PRESENT_IMMEDIATE,
            .frames_in_flight = frames_in_flight,
        };
        use_gpu = eng_gpu_init(&gpu, platform.window, &gpu_desc);
        if (use_gpu && screenshot_path != NULL) {
            gpu.capture_path = screenshot_path;
            gpu.capture_frame = screenshot_frame > 0 ? (uint64_t)screenshot_frame : 1U;
        }
        if (use_gpu) {
            platform.vsync = gpu.present_mode == ENG_PRESENT_VSYNC;
        } else {
            ENG_LOG_WARN("GPU indisponible : affichage logiciel (le rendu GPU arrive en M1)");
            eng_platform_use_software_surface(&platform, config.vsync);
        }
    }

    EngMemory memory;
    const EngMemoryConfig memory_config = {
        .permanent_bytes = (size_t)64 * 1024 * 1024,
        /* un terrain de 1025 x 1025 échantillons demande 75 Mio de maillage CPU (engine/terrain.h)
         */
        .level_bytes = (size_t)(demo_options.terrain_path != NULL ? 256 : 64) * 1024 * 1024,
        .frame_bytes = (size_t)8 * 1024 * 1024,
    };
    if (!eng_memory_init(&memory, &memory_config)) {
        eng_gpu_shutdown(&gpu);
        eng_platform_shutdown(&platform);
        eng_log_shutdown();
        return 1;
    }

    eng_config_apply_input(&config, &platform.input);

    /* Caméra libre de debug (M1-09). L'axe LOOK est en « unités de regard » : on convertit la
     * sensibilité de la souris (radians par pixel, config.ini) vers cette unité. Sans capture de
     * la souris (F2), la souris n'agit plus sur le regard : seul le stick droit reste actif. */
    const float mouse_look_sensitivity = config.mouse_sensitivity / ENG_CAMERA_LOOK_UNIT;
    EngCamera camera;
    eng_camera_init(&camera, (float[3]){0.0F, 3.0F, 14.0F}, 0.0F, -0.2F);
    EngCamera camera_prev = camera;
    if (eng_platform_set_mouse_capture(&platform, true)) {
        ENG_LOG_INFO("caméra libre : souris capturée (F2 pour la libérer), ZQSD/WASD, Espace/Ctrl "
                     "(haut/bas), Maj (sprint), molette (vitesse)");
    }

    /* Le jeu : bibliothèque dynamique rechargeable à chaud, état dans la mémoire permanente. */
    EngServices services;
    eng_game_services_init(&services, &memory);
    char default_game_path[512];
    if (game_path == NULL) {
        if (!eng_game_default_path(default_game_path, sizeof default_game_path)) {
            ENG_LOG_ERROR("chemin du jeu introuvable : utilise --game");
            eng_memory_shutdown(&memory);
            eng_gpu_shutdown(&gpu);
            eng_platform_shutdown(&platform);
            eng_log_shutdown();
            return 1;
        }
        game_path = default_game_path;
    }
    EngGameHost game;
    if (!eng_game_host_init(&game, game_path, &services, &memory.permanent, 0.25)) {
        eng_memory_shutdown(&memory);
        eng_gpu_shutdown(&gpu);
        eng_platform_shutdown(&platform);
        eng_log_shutdown();
        return 1;
    }

    /* Renderer et scène de démonstration (un cube qui tourne). Sans eux (shaders absents), l'image
     * est seulement effacée. En Debug, les shaders sont recompilés dès qu'on modifie le .hlsl. */
    EngRenderer renderer;
    DemoScene scene;
    bool use_renderer = false;
    if (use_gpu) {
        EngRendererDesc renderer_desc = {
            .depth_buffer = true, .hdr = true, .max_shadow_commands = 256};
#ifdef OW_SHADER_SOURCE_DIR
        renderer_desc.source_dir = OW_SHADER_SOURCE_DIR;
#endif
        use_renderer =
            eng_renderer_init(&renderer, &gpu, &memory.permanent, &memory.frame, &renderer_desc) &&
            demo_scene_init(&scene, &renderer, &memory.permanent, &memory.level, &memory.frame,
                            &demo_options);
        if (!use_renderer) {
            (void)eng_renderer_shutdown(&renderer);
            ENG_LOG_WARN("renderer indisponible : l'image sera seulement effacée");
        }
    }

    /* Interface de debug (M1-12) : Dear ImGui dessinée par-dessus la scène, dans une passe à part.
     * F1 l'affiche ou la masque ; masquée, elle ne coûte rien (aucune image d'interface). */
    EngUi ui = {0};
    bool use_ui = false;
    if (use_renderer && eng_ui_available()) {
        use_ui = eng_ui_init(&ui, platform.window, gpu.device, eng_gpu_swapchain_format(&gpu));
        if (use_ui) {
            platform.event_hook = eng_ui_event_hook;
            platform.event_hook_user = &ui;
            eng_renderer_set_overlay(&renderer, eng_ui_overlay_prepare, eng_ui_overlay_draw, &ui);
            platform.debug_ui = start_with_ui;
            ENG_LOG_INFO("interface de debug : F1 l'affiche (la souris est alors libérée), F3 le "
                         "debug draw");
        } else {
            ENG_LOG_WARN("interface de debug indisponible");
        }
    }
    /* Terrain (--terrain) : point de vue au-dessus du centre, plan lointain et vitesse adaptés à un
     * terrain de l'ordre du kilomètre (la diagonale de 1025 m en fait 1450). */
    float terrain_position[3];
    if (use_renderer && demo_scene_terrain_view(&scene, terrain_position)) {
        eng_camera_init(&camera, terrain_position, 0.0F, -0.35F);
        camera.far_z = 3000.0F;
        eng_camera_set_speed(&camera, 40.0F);
        camera_prev = camera;
    }
    bool ui_was_shown = false;
    bool mouse_was_captured_before_ui = false;

    /* Éditeur (M2-10b) : un monde chargé depuis la scène, dessiné à la place du modèle central. */
    EditorMode editor_mode = {0};
    bool use_editor = false;
    bool selftest_done = false;
    bool selftest_failed = false;
    if (editor_enabled) {
        if (!use_renderer) {
            ENG_LOG_WARN("éditeur indisponible sans rendu GPU%s",
                         editor_selftest ? " : AUTOTEST ÉDITEUR : IGNORÉ (pas de rendu GPU)" : "");
            selftest_done = true;
        } else if (editor_mode_init(&editor_mode, scene_arg)) {
            use_editor = true;
            demo_scene_set_world(&scene, &editor_mode.world);
            if (use_ui) {
                platform.debug_ui = true; /* la fenêtre d'éditeur est affichée dès le départ */
            }
            ENG_LOG_INFO("éditeur : clic gauche = sélection, flèches de couleur = déplacement sur "
                         "l'axe, F2 = capturer/libérer la souris pour voler, F1 = interface");
        }
    }

    /* Variables réglables en direct par l'interface (adresses de nos propres variables). */
    EngTweaks tweaks;
    eng_tweaks_init(&tweaks);
    float demo_time_scale = 1.0F;
    float fov_degrees = camera.fov_y * (180.0F / 3.14159265F);
    float clear_color[3] = {gpu.clear_color[0], gpu.clear_color[1], gpu.clear_color[2]};
    (void)eng_tweaks_add_float(&tweaks, "vitesse de la démo", &demo_time_scale, 0.0F, 4.0F);
    (void)eng_tweaks_add_float(&tweaks, "champ de vision (deg)", &fov_degrees, 30.0F, 110.0F);
    (void)eng_tweaks_add_float(&tweaks, "vitesse caméra (m/s)", &camera.speed, ENG_CAMERA_SPEED_MIN,
                               50.0F);
    /* Tonemapping (M1-14) : la scène est en HDR, l'exposition et la courbe se règlent en direct. */
    EngTonemap tonemap;
    eng_tonemap_default(&tonemap);
    if (use_renderer) {
        tonemap = eng_renderer_tonemap(&renderer);
    }
    tonemap.fxaa = true; /* post-process (M8-06) ; le menu Options le règle ensuite */
    tonemap.bloom = 0.5F;
    int32_t tonemap_operator = (int32_t)tonemap.op;
    (void)eng_tweaks_add_int(&tweaks, "tonemap : 0 aucun, 1 ACES, 2 AgX", &tonemap_operator, 0,
                             (int32_t)ENG_TONEMAP_COUNT - 1);
    (void)eng_tweaks_add_float(&tweaks, "exposition (EV)", &tonemap.exposure_ev,
                               ENG_EXPOSURE_EV_MIN, ENG_EXPOSURE_EV_MAX);
    (void)eng_tweaks_add_bool(&tweaks, "anticrénelage FXAA", &tonemap.fxaa);
    (void)eng_tweaks_add_float(&tweaks, "bloom", &tonemap.bloom, 0.0F, 2.0F);
    (void)eng_tweaks_add_float(&tweaks, "fond rouge", &clear_color[0], 0.0F, 1.0F);
    (void)eng_tweaks_add_float(&tweaks, "fond vert", &clear_color[1], 0.0F, 1.0F);
    (void)eng_tweaks_add_float(&tweaks, "fond bleu", &clear_color[2], 0.0F, 1.0F);
    (void)eng_tweaks_add_bool(&tweaks, "debug draw (F3)", &platform.debug_draw);

    /* Mode jouable (M4) : monde physique, personnage, caméra 3e personne. */
    PlayMode play = {0};
    bool use_play = false;
    static GameFlow flow;
    bool use_flow = false;
    bool flow_menu_was_open = false;
    SkeletonView skeleton_view = {0};
    PlayHook play_hook = {0};
    bool use_debug_hook = false;
    if (play_enabled) {
        if (!use_renderer || use_editor) {
            ENG_LOG_WARN("mode jouable indisponible %s",
                         use_editor ? "avec l'éditeur" : "sans rendu GPU");
        } else if (demo_options.terrain_path != NULL
                       ? play_mode_init_terrain(&play, &tweaks, config.camera_sensitivity,
                                                config.camera_invert_y, demo_options.terrain_path)
                       : play_mode_init(&play, &tweaks, config.camera_sensitivity,
                                        config.camera_invert_y)) {
            use_play = true;
            if (combat_enabled && !play_mode_enable_combat(&play, combat_audio)) {
                ENG_LOG_WARN("combat indisponible : mode jouable sans combat");
            }
            play.autoplay = combat_autoplay;
            /* M8 : menus, sauvegarde, options ; la démonstration automatique saute le menu */
            use_flow = play.combat != NULL && game_flow_init(&flow, &play, &platform.input, NULL);
            if (use_flow && (start_new_game || combat_autoplay)) {
                game_flow_start_new(&flow, &play);
            }
            if (use_flow && day_night_hour >= 0.0) {
                (void)eng_daynight_set_hour(&flow.daynight, day_night_hour);
            }
            if (use_flow && day_length > 0.0) {
                flow.daynight.day_seconds = (float)day_length;
            }
            play_hook.play = &play;
            use_debug_hook = true;
            play_mode_view(&play, &camera);
            camera_prev = camera;
        }
    }
    /* Squelette de debug (M4-12) : os superposés au modèle central. */
    if (skeleton_path != NULL || anim_path != NULL) {
        if (!use_renderer) {
            ENG_LOG_WARN("--skeleton / --anim indisponibles sans rendu GPU");
        } else if (skeleton_path == NULL || skeleton_view_load(&skeleton_view, skeleton_path)) {
            /* M4-16 : --anim anime le squelette ; sans --skeleton, celui que désigne le clip */
            if (anim_path != NULL &&
                !skeleton_view_load_anim(&skeleton_view, skeleton_path, anim_path)) {
                skeleton_view_shutdown(&skeleton_view);
            }
            if (skeleton_view.ready && sockets_path != NULL &&
                !skeleton_view_load_sockets(&skeleton_view, sockets_path)) {
                skeleton_view_shutdown(&skeleton_view);
            }
            if (skeleton_view.ready) {
                play_hook.skeleton = &skeleton_view;
                use_debug_hook = true;
                if (skeleton_view.animated) {
                    EngAnimDebug *a = &skeleton_view.anim;
                    (void)eng_tweaks_add_bool(&tweaks, "animation : pause (P)", &a->paused);
                    (void)eng_tweaks_add_float(&tweaks, "animation : instant (s)", &a->time, 0.0F,
                                               skeleton_view.clip.duration);
                    (void)eng_tweaks_add_float(&tweaks, "animation : vitesse", &a->speed,
                                               -ENG_ANIM_DEBUG_MAX_SPEED, ENG_ANIM_DEBUG_MAX_SPEED);
                    (void)eng_tweaks_add_bool(&tweaks, "animation : boucle", &a->loop);
                    (void)eng_tweaks_add_bool(&tweaks, "animation : pose de repos", &a->show_rest);
                    (void)eng_tweaks_add_bool(&tweaks, "animation : trajectoire (T)",
                                              &a->show_trail);
                    (void)eng_tweaks_add_int(&tweaks, "animation : articulation suivie (J K)",
                                             &a->selected, -1,
                                             (int32_t)skeleton_view.skeleton.joint_count - 1);
                }
            }
        }
    }
    if (use_debug_hook) {
        demo_scene_set_debug_hook(&scene, launcher_debug_hook, &play_hook);
    }
    /* Éclairage du modèle (PBR, M1-15) : le soleil se règle par azimut et hauteur. */
    EngLighting lighting;
    eng_lighting_default(&lighting);
    lighting.ambient_intensity =
        1.0F; /* intensité de l'environnement (M1-16) : 1 = le ciel tel quel */
    float sun_azimuth = demo_options.sun_azimuth;
    float sun_elevation = demo_options.sun_elevation;
    /* Cycle jour / nuit (M8-07) : celui de la partie, sinon une horloge locale (--day-night). */
    static EngDayNight local_daynight;
    eng_daynight_init(&local_daynight, day_night_hour >= 0.0 ? day_night_hour : 9.0);
    if (day_length > 0.0) {
        local_daynight.day_seconds = (float)day_length;
    }
    EngDayNight *daynight = use_flow ? &flow.daynight : (use_day_night ? &local_daynight : NULL);
    float daynight_hour_ui = daynight != NULL ? (float)daynight->hour : 0.0F;
    float daynight_hour_shown = daynight_hour_ui;
    if (daynight != NULL) {
        (void)eng_tweaks_add_float(&tweaks, "jour/nuit : heure", &daynight_hour_ui, 0.0F, 23.99F);
        (void)eng_tweaks_add_float(&tweaks, "jour/nuit : vitesse", &daynight->speed, 0.0F, 60.0F);
    }
    (void)eng_tweaks_add_float(&tweaks, "soleil : azimut (deg)", &sun_azimuth, -180.0F, 180.0F);
    (void)eng_tweaks_add_float(&tweaks, "soleil : hauteur (deg)", &sun_elevation, -90.0F, 90.0F);
    if (use_renderer) {
        (void)eng_tweaks_add_float(&tweaks, "brouillard : densité (1/m)", &scene.fog.density, 0.0F,
                                   0.02F);
        (void)eng_tweaks_add_float(&tweaks, "brouillard : décroissance (1/m)",
                                   &scene.fog.height_falloff, 0.0F, 0.02F);
        (void)eng_tweaks_add_float(&tweaks, "brouillard : opacité max", &scene.fog.max_opacity,
                                   0.0F, 1.0F);
    }
    (void)eng_tweaks_add_float(&tweaks, "soleil : intensité", &lighting.sun_intensity, 0.0F, 4.0F);
    (void)eng_tweaks_add_float(&tweaks, "environnement : intensité", &lighting.ambient_intensity,
                               0.0F, 4.0F);
    /* Ombre du soleil (M1-17) : biais en texels de la carte, rayon du filtre, force. */
    bool shadow_enabled = true;
    EngShadowParams shadow_params;
    eng_shadow_params_default(&shadow_params);
    int32_t shadow_radius = (int32_t)shadow_params.pcf_radius;
    (void)eng_tweaks_add_bool(&tweaks, "ombre : active", &shadow_enabled);
    (void)eng_tweaks_add_float(&tweaks, "ombre : biais constant (texels)",
                               &shadow_params.constant_bias, 0.0F, 8.0F);
    (void)eng_tweaks_add_float(&tweaks, "ombre : biais de pente", &shadow_params.slope_bias, 0.0F,
                               4.0F);
    (void)eng_tweaks_add_float(&tweaks, "ombre : biais normal (texels)", &shadow_params.normal_bias,
                               0.0F, 8.0F);
    (void)eng_tweaks_add_int(&tweaks, "ombre : rayon du filtre (texels)", &shadow_radius, 0,
                             (int32_t)ENG_SHADOW_MAX_PCF_RADIUS);
    (void)eng_tweaks_add_float(&tweaks, "ombre : force", &shadow_params.strength, 0.0F, 1.0F);
    EngFrameStats frame_stats;
    eng_frame_stats_init(&frame_stats);
    double demo_time = 0.0;
    double previous_cpu_ms = 0.0;
    uint64_t draws_last_frame = 0;
    uint64_t binds_last_frame = 0;
    uint64_t uniforms_last_frame = 0;

    /* Échauffement : SDL alloue paresseusement sa file d'événements. On la sollicite une fois
     * hors du compteur. */
    bool alive = eng_platform_pump(&platform);

    /* Le compteur couvre la mise à jour (événements et jeu). L'affichage logiciel provisoire en
     * est exclu : SDL_UpdateWindowSurface alloue 2 fois par image en interne (mesuré avec le
     * pilote dummy). Il disparaît avec Vulkan (M1) ; le compteur couvrira alors l'image
     * entière. Le contrôle de rechargement du jeu (accès disque, copie) est hors compteur lui
     * aussi : c'est un outil de développement. */
    EngClock clock;
    eng_clock_init(&clock);
    EngLoop loop;
    if (!eng_loop_init(&loop, clock.frequency, 60, 0.25)) {
        if (use_ui) {
            platform.event_hook = NULL;
            eng_ui_shutdown(&ui);
        }
        if (use_renderer) {
            demo_scene_shutdown(&scene, &renderer);
            (void)eng_renderer_shutdown(&renderer);
        }
        eng_game_host_shutdown(&game);
        eng_memory_shutdown(&memory);
        eng_gpu_shutdown(&gpu);
        eng_platform_shutdown(&platform);
        eng_log_shutdown();
        return 1;
    }
    uint64_t frame = 0;
    ENG_PROF_THREAD_NAME("principal");
    while (alive && (max_frames < 0 || (long)frame < max_frames)) {
        ENG_PROF_ZONE(z_frame, "frame");
        eng_clock_tick(&clock);
        const uint64_t frame_start = eng_clock_counter();
        if (frame > 0) { /* durée TOTALE de l'image précédente et son temps CPU */
            (void)eng_frame_stats_push(&frame_stats, (float)(clock.dt * 1000.0),
                                       (float)previous_cpu_ms);
        }
        ENG_PROF_ZONE(z_poll, "game_reload_check");
        (void)eng_game_host_poll(&game, clock.time);
        if (use_renderer) {
            /* Rechargement des shaders : outil de développement, hors compteur d'allocations. */
            (void)eng_renderer_update(&renderer, clock.time);
            demo_scene_update(&scene, clock.time); /* recuisson et rechargement des assets */
        }
        ENG_PROF_ZONE_END(z_poll);
        eng_memory_begin_frame(&memory);
        ENG_PROF_ZONE(z_events, "events");
        alive = eng_platform_pump(&platform);
        if (stress_resize && use_gpu) {
            /* taille différente à chaque image, sans attendre que la précédente soit prise en
             * compte */
            const int width = 160 + (int)((frame * 37U) % 700U);
            const int height = 120 + (int)((frame * 53U) % 500U);
            (void)eng_platform_set_window_size(&platform, width, height);
        }
        ENG_PROF_ZONE_END(z_events);
        uint32_t steps = eng_loop_advance(&loop, clock.dt_counter);
        ENG_PROF_PLOT("pas de simulation", steps);
        ENG_PROF_ZONE(z_sim, "simulation");
        for (uint32_t i = 0; i < steps; i++) {
            ENG_PROF_ZONE(z_tick, "tick");
            EngInputState input;
            platform.input.axes[ENG_AXIS_LOOK].mouse_sensitivity =
                platform.mouse_captured ? mouse_look_sensitivity : 0.0F;
            eng_input_begin_tick(&platform.input, &input); /* UNE fois par pas */
            camera_prev = camera;
            if (use_play) {
                if (!use_flow || game_flow_tick(&flow, &play, &input, (float)loop.tick_seconds)) {
                    play_mode_tick(&play, &input, (float)loop.tick_seconds);
                }
                play_mode_view(&play, &camera);
                if (use_flow && flow.quit) {
                    alive = false;
                }
                if (use_flow && flow.graphics_dirty) { /* M8-06 */
                    tonemap.fxaa = flow.options.fxaa;  /* appliqué au renderer plus bas */
                    tonemap.bloom = flow.options.bloom;
                    flow.graphics_dirty = false;
                }
            } else {
                eng_camera_update(&camera, &input, (float)loop.tick_seconds);
            }
            ENG_PROF_ZONE(z_update, "game.update");
            game.api->update(game.state, &input, loop.tick_seconds);
            ENG_PROF_ZONE_END(z_update);
            ENG_PROF_ZONE_END(z_tick);
        }
        ENG_PROF_ZONE_END(z_sim);
        eng_memory_end_frame(&memory);
        EngClockStats stats;
        if (eng_clock_take_stats(&clock, &stats)) {
            char title[96];
            snprintf(title, sizeof title, "Open World - %.1f fps, dt %.2f ms (max %.2f ms)%s",
                     stats.fps, stats.avg_dt_ms, stats.max_dt_ms, platform.vsync ? " [vsync]" : "");
            eng_platform_set_title(&platform, title);
            ENG_LOG_DEBUG("%.1f fps, dt moyen %.2f ms, max %.2f ms, vsync %s", stats.fps,
                          stats.avg_dt_ms, stats.max_dt_ms, platform.vsync ? "oui" : "non");
        }
        if (alive) {
            EngDebugSquare square;
            ENG_PROF_ZONE(z_render, "game.render");
            game.api->render(game.state, loop.alpha, &square);
            ENG_PROF_ZONE_END(z_render);
            ENG_PROF_ZONE(z_present, "present");
            if (use_gpu) {
                /* Le renderer exécute la file de commandes ; le carré du jeu n'est visible
                 * qu'avec --software tant que le jeu ne dessine pas par le renderer. */
                if (use_renderer) {
                    /* Temps de la démo : accumulé ici pour que son réglage (curseur) ne fasse pas
                     * sauter la rotation. dt est plafonné comme celui de la simulation. */
                    demo_time += (clock.dt < 0.25 ? clock.dt : 0.25) * (double)demo_time_scale;
                    const double sim_time = demo_time;
                    camera.fov_y = fov_degrees * (3.14159265F / 180.0F);
                    camera_prev.fov_y = camera.fov_y;
                    eng_renderer_set_clear_color(&renderer, clear_color[0], clear_color[1],
                                                 clear_color[2], 1.0F);
                    tonemap.op = (EngTonemapOperator)tonemap_operator;
                    EngTonemap frame_tonemap = tonemap;
                    scene.night = 0.0F;
                    if (daynight != NULL) {
                        if (daynight_hour_ui != daynight_hour_shown) { /* curseur F1 déplacé */
                            (void)eng_daynight_set_hour(daynight, (double)daynight_hour_ui);
                        }
                        if (daynight == &local_daynight) {
                            eng_daynight_tick(daynight, clock.dt < 0.25 ? clock.dt : 0.25);
                        }
                        eng_daynight_sun(daynight, &sun_azimuth, &sun_elevation);
                        scene.night = eng_daynight_night_weight(sun_elevation);
                        frame_tonemap.exposure_ev +=
                            eng_daynight_exposure_bias(daynight, sun_elevation);
                        daynight_hour_ui = (float)daynight->hour;
                        daynight_hour_shown = daynight_hour_ui;
                    }
                    eng_renderer_set_tonemap(&renderer, &frame_tonemap);
                    const float aspect =
                        platform.height > 0 ? (float)platform.width / (float)platform.height : 1.0F;
                    EngCamera view_camera;
                    eng_camera_lerp(&camera_prev, &camera, (float)loop.alpha, &view_camera);
                    skeleton_view_update(&skeleton_view, (float)clock.dt,
                                         !use_ui || !eng_ui_wants_keyboard(&ui));
                    EngRenderQueue *queue = eng_renderer_begin_frame(&renderer);
                    (void)eng_lighting_set_sun_angles(&lighting, sun_azimuth, sun_elevation);
                    shadow_params.pcf_radius = (uint32_t)shadow_radius;
                    demo_scene_draw(&scene, &renderer, queue, sim_time, &view_camera, aspect,
                                    &lighting, shadow_enabled ? &shadow_params : NULL);
                    if (use_editor) {
                        if (editor_selftest) {
                            /* le modèle est chargé par demo_scene_update : on attend quelques
                             * images, puis le test se déroule en une seule image */
                            if (!selftest_done && frame >= 8) {
                                char out[512];
                                if (eng_path_resolve(out, sizeof out,
                                                     "logs/editor_selftest.scene") &&
                                    editor_mode_selftest(&editor_mode, &scene, &view_camera, aspect,
                                                         (float)platform.width,
                                                         (float)platform.height, out)) {
                                    selftest_failed = false;
                                } else {
                                    selftest_failed = true;
                                }
                                selftest_done = true;
                            }
                        } else if (!platform.mouse_captured) {
                            float mouse_x = 0.0F;
                            float mouse_y = 0.0F;
                            const SDL_MouseButtonFlags buttons =
                                SDL_GetMouseState(&mouse_x, &mouse_y);
                            int window_w = 0;
                            int window_h = 0;
                            (void)SDL_GetWindowSize(platform.window, &window_w, &window_h);
                            EngRay ray;
                            if (eng_ray_from_screen(&view_camera, aspect, mouse_x, mouse_y,
                                                    (float)window_w, (float)window_h, &ray)) {
                                (void)eng_editor_pointer(&editor_mode.editor, &editor_mode.world,
                                                         demo_scene_entity_bounds, &scene, &ray,
                                                         view_camera.position,
                                                         (buttons & SDL_BUTTON_LMASK) != 0,
                                                         use_ui && eng_ui_wants_mouse(&ui));
                            }
                        } else {
                            eng_editor_drag_end(
                                &editor_mode.editor); /* souris capturée : pas de clic */
                        }
                    }
                    play_hook.alpha = loop.alpha;
                    if (platform.debug_draw || use_editor || use_play ||
                        play_hook.skeleton != NULL) {
                        /* F3 ; en dernier : les lignes recouvrent la scène */
                        demo_scene_draw_debug(&scene, &renderer, queue, sim_time, &view_camera,
                                              aspect, platform.debug_draw,
                                              use_editor ? &editor_mode.editor : NULL);
                    }
                    if (use_ui) {
                        /* F1 : à l'ouverture la souris est libérée (sinon on ne pourrait pas
                         * cliquer), et rendue à la fermeture si elle était capturée avant. */
                        if (platform.debug_ui != ui_was_shown) {
                            if (platform.debug_ui) {
                                mouse_was_captured_before_ui = platform.mouse_captured;
                                (void)eng_platform_set_mouse_capture(&platform, false);
                            } else if (mouse_was_captured_before_ui) {
                                (void)eng_platform_set_mouse_capture(&platform, true);
                            }
                            ui_was_shown = platform.debug_ui;
                        }
                        if (use_flow) { /* menu ouvert : souris libre ; fermé : regard à la souris
                                         */
                            const bool menu_open = eng_menu_is_open(&flow.menu);
                            if (menu_open != flow_menu_was_open && !platform.debug_ui) {
                                (void)eng_platform_set_mouse_capture(&platform, !menu_open);
                            }
                            flow_menu_was_open = menu_open;
                        }
                        eng_ui_set_visible(&ui, platform.debug_ui || use_flow);
                        if (eng_ui_begin_frame(&ui, platform.width, platform.height,
                                               (float)clock.dt)) {
                            EngUiPanelData panel = {
                                .frames = &frame_stats,
                                .draws = (uint32_t)draws_last_frame,
                                .pipeline_binds = (uint32_t)binds_last_frame,
                                .uniform_pushes = (uint32_t)uniforms_last_frame,
                                .rejected_commands = renderer.stats.rejected_commands,
                                .dropped_commands = renderer.stats.dropped_commands,
                                .terrain_chunks_drawn = scene.terrain_chunks_drawn,
                                .terrain_chunks_total = scene.terrain_chunk_total,
                                .arena_count = 3,
                                .frame_allocs = eng_memory_frame_allocs(&memory),
                                .tweaks = &tweaks,
                                .animation = skeleton_view.animated ? skeleton_view.status : NULL,
                            };
                            panel.arenas[0] =
                                (EngUiArena){"permanent", memory.permanent.used,
                                             memory.permanent.peak, memory.permanent.capacity};
                            panel.arenas[1] =
                                (EngUiArena){"level", memory.level.used, memory.level.peak,
                                             memory.level.capacity};
                            panel.arenas[2] =
                                (EngUiArena){"frame", memory.frame.used, memory.frame.peak,
                                             memory.frame.capacity};
                            if (platform.debug_ui) {
                                eng_ui_draw_panel(&ui, &panel);
                            }
                            if (use_flow) {
                                game_flow_draw(&flow, &ui, &play, &view_camera, aspect,
                                               platform.width, platform.height);
                            }
                            if (use_editor) {
                                EngUiEditorResult editor_result = {0};
                                eng_ui_draw_editor(&ui, &editor_mode.world, &editor_mode.editor,
                                                   &editor_result);
                                if (editor_result.save_requested) {
                                    (void)editor_mode_save(&editor_mode);
                                }
                            }
                            eng_ui_end_frame(&ui);
                        }
                    }
                    /* Fin du travail CPU de l'image (la présentation, qui attend la synchro
                     * verticale, n'en fait pas partie). */
                    previous_cpu_ms =
                        eng_clock_ms_between(&clock, frame_start, eng_clock_counter());
                    const EngRendererStats before = renderer.stats;
                    eng_renderer_end_frame(&renderer);
                    draws_last_frame = renderer.stats.draws - before.draws;
                    binds_last_frame = renderer.stats.pipeline_binds - before.pipeline_binds;
                    uniforms_last_frame = renderer.stats.uniform_pushes - before.uniform_pushes;
                } else {
                    (void)eng_gpu_begin_frame(&gpu);
                    eng_gpu_end_frame(&gpu);
                }
            } else {
                eng_platform_present(&platform, &square);
            }
            ENG_PROF_ZONE_END(z_present);
            frame++;
        }
        ENG_PROF_ZONE_END(z_frame);
        ENG_PROF_FRAME();
    }

    ENG_LOG_INFO(
        "simulation : %llu pas à %u Hz, %llu image(s) plafonnée(s), %u rechargement(s) du jeu",
        (unsigned long long)loop.tick_count, (unsigned)loop.tick_hz,
        (unsigned long long)loop.clamped_frames, (unsigned)game.reload_count);
    eng_memory_log_stats(&memory);
    if (use_ui) {
        platform.event_hook = NULL;
        eng_renderer_set_overlay(&renderer, NULL, NULL, NULL);
        eng_ui_shutdown(&ui); /* avant le GPU : le backend détient des ressources GPU */
    }
    eng_tweaks_clear(&tweaks);
    if (use_renderer) {
        ENG_LOG_INFO("renderer : %llu image(s), %llu passe(s) d'ombre",
                     (unsigned long long)renderer.stats.frames,
                     (unsigned long long)renderer.stats.shadow_passes);
        if (use_editor) {
            demo_scene_set_world(&scene, NULL);
            editor_mode_shutdown(&editor_mode);
        }
        demo_scene_set_debug_hook(&scene, NULL, NULL);
        if (use_play) {
            play_mode_shutdown(&play);
        }
        skeleton_view_shutdown(&skeleton_view);
        demo_scene_shutdown(&scene, &renderer);
        const uint32_t leaked = eng_renderer_shutdown(&renderer);
        if (leaked != 0) {
            ENG_LOG_WARN("renderer : %u ressource(s) non détruite(s) libérée(s) à l'arrêt", leaked);
        }
    }
    eng_game_host_shutdown(&game);
    eng_memory_shutdown(&memory);
    int exit_code = selftest_failed ? 1 : 0;
    if (use_gpu) {
        eng_gpu_wait_idle(&gpu);
        const bool balanced = eng_gpu_resources_balanced(&gpu);
        ENG_LOG_INFO(
            "GPU : %llu redimensionnement(s), %llu image(s) ignorée(s), %u image(s) en vol "
            "au plus : %s",
            (unsigned long long)gpu.resizes, (unsigned long long)gpu.frames_skipped,
            (unsigned)gpu.max_pending, balanced ? "sans fuite" : "FUITE de barrière ou de cible");
        if (!balanced || (stress_resize && gpu.frames_skipped != 0)) {
            exit_code = 1;
        }
    }
    if (!eng_gpu_shutdown(&gpu)) {
        exit_code = 1;
    }
    eng_platform_shutdown(&platform);
    ENG_LOG_INFO("arrêt propre après %llu images", (unsigned long long)frame);
    eng_log_shutdown();
    return exit_code;
}
