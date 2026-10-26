#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/engine.h"
#include "engine/log.h"
#include "engine/platform.h"

static void usage(const char *prog)
{
    fprintf(stderr, "usage : %s [--frames N] [--log-level trace|debug|info|warn|error]\n", prog);
    fprintf(stderr, "  --frames N     quitte après N images (tests automatiques)\n");
    fprintf(stderr,
            "  --log-level L  niveau du journal (sinon variable ENG_LOG_LEVEL, sinon info)\n");
}

int main(int argc, char **argv)
{
    long max_frames = -1; /* -1 : jusqu'à la fermeture de la fenêtre */
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
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    /* Si le fichier ne peut pas être créé, l'erreur est déjà journalisée : on continue
     * avec la console seule. */
    (void)eng_log_init("logs", log_level);

    ENG_LOG_INFO("engine %s", eng_version());
    eng_print_deps();

    EngWindowDesc desc = {
        .title = "Open World",
        .width = 1280,
        .height = 720,
        .resizable = true,
        .fullscreen = false,
    };
    EngPlatform platform;
    if (!eng_platform_init(&platform, &desc)) {
        eng_log_shutdown();
        return 1;
    }

    uint64_t frame = 0;
    while (eng_platform_pump(&platform) && (max_frames < 0 || (long)frame < max_frames)) {
        eng_platform_present(&platform, frame);
        frame++;
    }

    eng_platform_shutdown(&platform);
    ENG_LOG_INFO("arrêt propre après %llu images", (unsigned long long)frame);
    eng_log_shutdown();
    return 0;
}
