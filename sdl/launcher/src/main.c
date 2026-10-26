#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/engine.h"
#include "engine/platform.h"

static void usage(const char *prog)
{
    fprintf(stderr, "usage : %s [--frames N]\n", prog);
    fprintf(stderr, "  --frames N  quitte après N images (tests automatiques)\n");
}

int main(int argc, char **argv)
{
    long max_frames = -1; /* -1 : jusqu'à la fermeture de la fenêtre */

    for (int i = 1; i < argc; i++) {
        char *end = NULL;
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            max_frames = strtol(argv[++i], &end, 10);
            if (*end != '\0' || max_frames < 0) {
                usage(argv[0]);
                return 2;
            }
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    printf("engine %s\n", eng_version());
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
        return 1;
    }

    uint64_t frame = 0;
    while (eng_platform_pump(&platform) && (max_frames < 0 || (long)frame < max_frames)) {
        eng_platform_present(&platform, frame);
        frame++;
    }

    eng_platform_shutdown(&platform);
    printf("arrêt propre après %llu images\n", (unsigned long long)frame);
    return 0;
}
