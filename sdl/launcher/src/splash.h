#ifndef LAUNCHER_SPLASH_H
#define LAUNCHER_SPLASH_H

/* Écran de chargement animé (2026-11-22). Avant : entre l'ouverture de la fenêtre et la première
 * image du jeu (plusieurs secondes : éclairage d'ambiance, carte du monde, personnages), la
 * fenêtre n'était jamais dessinée et montrait une copie figée de l'écran.
 *
 * Maintenant : dès que le GPU est prêt, le fil PRINCIPAL dessine une scène animée
 * (shaders/splash.frag.hlsl : l'aube sur la carte du monde, le chevalier, le fil rouge, le titre)
 * pendant que les étapes longues du chargement s'exécutent dans un fil à part (splash_run).
 *
 * Règles de SDL_GPU respectées : l'image de la fenêtre n'est demandée que par le fil principal ;
 * chaque fil n'utilise que ses propres tampons de commandes ; le fil de chargement ne fait que
 * créer et remplir des ressources. Pendant un splash_run, le fil principal ne touche ni au
 * renderer du moteur ni à l'état d'EngGpu : seulement au périphérique et à la fenêtre.
 *
 * Sans écran (splash_init non appelé ou en échec : tests, --no-splash), splash_run exécute le
 * travail directement, sur le fil appelant : le chargement est alors exactement celui d'avant. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/gpu.h"
#include "engine/platform.h"

#define SPLASH_MIN_SECONDS 3.4   /* le fil se déroule et le titre apparaît avant la sortie */
#define SPLASH_OUTRO_SECONDS 0.9 /* éclat du soleil puis fondu au noir */

typedef bool (*SplashJob)(void *user);

typedef struct Splash {
    struct SDL_GPUDevice *device;
    struct SDL_Window *window;
    struct SDL_GPUGraphicsPipeline *pipeline;
    struct SDL_GPUTexture *text; /* titre et légende en champ de distance */
    struct SDL_GPUSampler *sampler;
    float title_rect[4]; /* uv du titre dans l'image (u0, v0, u1, v1) */
    float label_rect[4];
    float text_info[4];      /* largeur / hauteur du titre, de la légende */
    uint64_t last_ns;        /* horloge de la dernière image */
    double time_s;           /* temps de l'animation */
    float progress;          /* progression affichée (lissée) */
    float target;            /* progression visée */
    float stage_end;         /* fin de l'étape en cours */
    uint32_t frames;         /* images présentées */
    uint32_t frames_loading; /* dont pendant un travail en tâche de fond */
    uint32_t jobs;           /* travaux exécutés dans le fil de chargement */
    double loading_s;        /* durée cumulée de ces travaux */
    double worst_gap_ms;     /* plus long écart entre deux images */
    bool loading;            /* un travail est en cours dans le fil de chargement */
    bool skip; /* une touche a été pressée : la sortie commence dès que possible */
    bool ready;
} Splash;

/* Crée le pipeline et l'image du titre. false (journalisé) : pas d'écran de chargement, le
 * lancement continue comme avant. */
bool splash_init(Splash *s, EngGpu *gpu, EngPlatform *platform);
/* Une image de l'animation, et les événements de la fenêtre. Sans effet si l'écran n'est pas prêt.
 */
void splash_frame(Splash *s, EngPlatform *platform);
/* Exécute `job(user)` dans un fil à part pendant que l'écran s'anime ; la progression avance
 * jusqu'à `progress_end` (0 à 1). Renvoie le résultat du travail. Écran non prêt : le travail est
 * exécuté directement. */
bool splash_run(Splash *s, EngPlatform *platform, SplashJob job, void *user, float progress_end);
/* Fin du chargement : laisse le titre apparaître (SPLASH_MIN_SECONDS, sauf touche pressée ou
 * fermeture demandée), joue la sortie, puis libère tout. */
void splash_finish(Splash *s, EngPlatform *platform);
void splash_shutdown(Splash *s);
/* Mesures d'une image capturée (autotest) : parts de pixels, de 0 à 1. */
typedef struct SplashStats {
    float luma;        /* luminosité moyenne de l'image (0 à 1) */
    float title;       /* pixels ivoire (les lettres) dans la bande du titre */
    float title_left;  /* ... dans sa moitié gauche */
    float title_right; /* ... dans sa moitié droite (le titre apparaît de gauche à droite) */
    float thread;      /* pixels rouges (le fil) dans le ciel, entre le chevalier et le château */
    float bar;         /* pixels rouges sur la corde de progression */
} SplashStats;

/* Développement et tests : dessine l'image de l'instant `time_s` (progression, sortie et taille
 * données) hors écran, l'écrit en PPM (`path` NULL : pas de fichier) et la mesure (`stats` NULL :
 * pas de mesure). false si l'écran n'est pas prêt ou en cas d'échec. */
bool splash_capture(Splash *s, double time_s, float progress, float outro, uint32_t width,
                    uint32_t height, const char *path, SplashStats *stats);

#endif
