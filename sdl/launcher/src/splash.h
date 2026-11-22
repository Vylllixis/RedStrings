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
 * travail directement, sur le fil appelant : le chargement est alors exactement celui d'avant.
 *
 * ÉCRAN D'ATTENTE (retour de Théo, 2026-11-22 : « encore 5 secondes de vide, où il y a mon fond
 * d'écran au lieu du chargement »). Le GPU lui-même met du temps à être prêt : 2,7 s mesurées à
 * froid sur sa machine (la carte dédiée d'un portable dort tant que rien ne s'en sert), 0,2 s à
 * chaud. Pendant ce temps la fenêtre, déjà ouverte, n'était pas dessinée. Maintenant
 * splash_prelude_run la peint tout de suite, avec le processeur (surface de la fenêtre), et y
 * anime la corde de chargement de la scène (engine/boot_screen.h) pendant que le pilote démarre
 * dans un fil à part. Quand le GPU prend le relais, la corde est dessinée au même endroit, au
 * même instant de sa vibration, et la scène apparaît en fondu derrière elle. */

#include <stdbool.h>
#include <stdint.h>

#include "engine/gpu.h"
#include "engine/platform.h"

#define SPLASH_MIN_SECONDS 3.4   /* le fil se déroule et le titre apparaît avant la sortie */
#define SPLASH_OUTRO_SECONDS 0.9 /* éclat du soleil puis fondu au noir */

typedef bool (*SplashJob)(void *user);

/* Le titre et la légende en champ de distance (engine/text_sdf.h), dans une seule image à un
 * canal construite par le processeur : l'écran d'attente y lit la légende, puis l'image part au
 * GPU pour la scène. `pixels` est alloué : splash_text_free. */
typedef struct SplashText {
    uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    float title_rect[4]; /* uv du titre dans l'image (u0, v0, u1, v1) */
    float label_rect[4];
    float text_info[4]; /* largeur / hauteur du titre, de la légende */
} SplashText;

bool splash_text_build(SplashText *text);
void splash_text_free(SplashText *text);

/* L'écran d'attente : ce qu'il a montré, pour que la scène reprenne au même point. */
typedef struct SplashPrelude {
    SplashText text;     /* construit ici, envoyé au GPU par splash_init */
    double time_s;       /* temps d'animation montré */
    float progress;      /* progression atteinte par la corde */
    uint32_t frames;     /* images dessinées */
    double first_ms;     /* délai entre l'appel et la première image (fenêtre peinte) */
    double job_s;        /* durée du travail (démarrage du pilote) */
    double worst_gap_ms; /* plus long écart entre deux images */
    bool shown;          /* l'écran d'attente a été affiché */
} SplashPrelude;

/* Exécute `job(user)` (le démarrage du GPU : eng_gpu_create) dans un fil à part pendant que la
 * fenêtre montre l'écran d'attente, puis rend la fenêtre (plus de surface logicielle : elle peut
 * être rattachée au GPU). Renvoie le résultat du travail. Si l'écran d'attente n'est pas
 * disponible (pilote vidéo autre que x11 et offscreen, surface ou fil refusés), le travail est
 * exécuté directement, comme avant ; `shown` reste faux. Le travail ne doit pas toucher à la
 * fenêtre. */
bool splash_prelude_run(SplashPrelude *p, EngPlatform *platform, SplashJob job, void *user);
void splash_prelude_shutdown(SplashPrelude *p);

typedef struct Splash {
    struct SDL_GPUDevice *device;
    struct SDL_Window *window;
    struct SDL_GPUGraphicsPipeline *pipeline;
    struct SDL_GPUTexture *text; /* titre et légende en champ de distance */
    struct SDL_GPUSampler *sampler;
    float title_rect[4]; /* uv du titre dans l'image (u0, v0, u1, v1) */
    float label_rect[4];
    float text_info[4]; /* largeur / hauteur du titre, de la légende */
    uint64_t last_ns;   /* horloge de la dernière image */
    double time_s;      /* temps de l'animation */
    double prelude_s;   /* écran d'attente déjà montré (s) : la corde continue ; 0 : aucun */
    float progress;     /* progression affichée (lissée) */
    float target;       /* progression visée */
    float stage_end;    /* fin de l'étape en cours */
    uint32_t frames;    /* images présentées */
    uint32_t frames_loading; /* dont pendant un travail en tâche de fond */
    uint32_t jobs;           /* travaux exécutés dans le fil de chargement */
    double loading_s;        /* durée cumulée de ces travaux */
    double worst_gap_ms;     /* plus long écart entre deux images */
    bool loading;            /* un travail est en cours dans le fil de chargement */
    bool skip; /* une touche a été pressée : la sortie commence dès que possible */
    bool ready;
    bool done; /* le chargement est fini (splash_finish) : la scène ne sert plus qu'au titre */
} Splash;

/* Crée le pipeline et l'image du titre. false (journalisé) : pas d'écran de chargement, le
 * lancement continue comme avant. `prelude` (NULL : aucun) : l'écran d'attente qui a précédé ; s'il
 * a été affiché, la scène reprend son texte, sa progression et l'instant de sa corde. */
bool splash_init(Splash *s, EngGpu *gpu, EngPlatform *platform, const SplashPrelude *prelude);
/* Une image de l'animation, et les événements de la fenêtre. Sans effet si l'écran n'est pas prêt.
 */
void splash_frame(Splash *s, EngPlatform *platform);
/* Exécute `job(user)` dans un fil à part pendant que l'écran s'anime ; la progression avance
 * jusqu'à `progress_end` (0 à 1). Renvoie le résultat du travail. Écran non prêt : le travail est
 * exécuté directement. */
bool splash_run(Splash *s, EngPlatform *platform, SplashJob job, void *user, float progress_end);
/* Fin du chargement : laisse le titre apparaître (SPLASH_MIN_SECONDS, sauf touche pressée ou
 * fermeture demandée) puis, si `outro`, joue la sortie (éclat du soleil, fondu au noir : le jeu
 * commence tout de suite). Sans sortie, la scène continue : c'est l'écran-titre (title.h) qui la
 * reprend au même instant. Les ressources restent en place jusqu'à splash_shutdown. */
void splash_finish(Splash *s, EngPlatform *platform, bool outro);
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

/* Le relais entre l'écran d'attente et la scène, image contre image : la DERNIÈRE image d'une
 * attente de `prelude_s` secondes (dessinée par le processeur) comparée à la PREMIÈRE image de la
 * scène qui la suit (dessinée par le GPU, hors écran). */
typedef struct SplashHandover {
    uint32_t worst;     /* plus grand écart d'une composante, sur 255 */
    uint32_t differing; /* pixels dont une composante s'écarte de plus de 2 */
    uint32_t lit;       /* pixels éclairés dans l'image du processeur (la corde est bien là) */
    uint32_t pixels;    /* pixels comparés */
} SplashHandover;

/* `dir` non NULL : écrit aussi les deux images (`dir`/relais-<name>-gpu.ppm et -cpu.ppm). false si
 * l'écran n'est pas prêt ou en cas d'échec. */
bool splash_handover(Splash *s, const SplashText *text, double prelude_s, float progress,
                     uint32_t width, uint32_t height, const char *dir, const char *name,
                     SplashHandover *out);

#endif
