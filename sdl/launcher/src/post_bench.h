#pragma once

/* Mesure du coût de chaque effet de post-process (finition M8-06, `--post-bench N`) : le lanceur
 * dessine N images par configuration (sans les 30 premières, le temps que les cibles se recréent),
 * présentation immédiate (sans vsync), et journalise la durée moyenne d'une image pour chacune :
 * base (rien), + FXAA, + bloom, + SSAO, tout, tout à 75 %, tout à 50 %. SDL_GPU n'a pas de requêtes
 * de temps GPU : c'est la durée de l'image entière, qui ne mesure le GPU que si c'est lui qui
 * limite (grande fenêtre, plein écran). */

#include <stdbool.h>
#include <stdint.h>

#include "engine/gpu.h"
#include "engine/tonemap.h"

#define POST_BENCH_CONFIGS 7U
#define POST_BENCH_WARMUP 30U

typedef struct PostBench {
    double sum_ms[POST_BENCH_CONFIGS];
    uint32_t counted[POST_BENCH_CONFIGS];
    uint32_t frames_per_config; /* images mesurées par configuration */
    uint32_t config;            /* configuration en cours */
    uint32_t frame;             /* images déjà dessinées dans la configuration */
    bool active;
} PostBench;

/* `frames_per_config` > 0 : mesure active. */
void post_bench_init(PostBench *b, uint32_t frames_per_config);

/* Avant le rendu de chaque image : `previous_frame_ms` est la durée de l'image précédente (celle de
 * la configuration en cours). Règle `tonemap` et l'échelle de `gpu` pour la configuration de
 * l'image, et la présentation immédiate. Renvoie false quand toutes les configurations sont
 * mesurées (résultats journalisés une fois). */
bool post_bench_frame(PostBench *b, double previous_frame_ms, EngTonemap *tonemap, EngGpu *gpu);

/* Durée moyenne d'une image (ms) de la configuration `k`, -1 si rien n'a été mesuré. */
double post_bench_mean(const PostBench *b, uint32_t k);
